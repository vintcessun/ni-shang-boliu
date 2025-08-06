#include "../include/http_client.h"
#include "core_http_client.h"
#include <string.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/errno.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"
#include "log.h"
#include "req_error.h"
#include "bl_utils.h"

#define DBG_TAG "HTTP_CLIENT"

/* ========================================================================== */
/* Internal Helper Functions                                                  */
/* ========================================================================== */

// --- HTTP Protocol Operation Wrappers ---

int _http_connect(const char *server, uint16_t port, int timeout_ms, int32_t *fd_out)
{
    struct hostent *hp = NULL;
    struct sockaddr_in serv_addr;
    int sockfd = -1;
    int connect_ret_code = REQ_ERR_INTERNAL;
    uint64_t start_us = bflb_mtimer_get_time_us();
    char ip_str[16];

    *fd_out = -1;

    // DNS resolution
    LOG_D("[%s] Connecting: Resolving hostname: %s\r\n", DBG_TAG, server);
    hp = gethostbyname(server);
    if (hp == NULL) {
        LOG_E("[%s] Connect failed: DNS resolution error (errno: %d).\r\n", DBG_TAG, errno);
        return REQ_ERR_DNS;
    }
    if (inet_ntop(AF_INET, hp->h_addr_list[0], ip_str, sizeof(ip_str)) == NULL) {
        strncpy(ip_str, "[ip fmt err]", sizeof(ip_str) - 1);
        ip_str[sizeof(ip_str) - 1] = '\0';
        LOG_W("[%s] Failed to format resolved IP address.\r\n", DBG_TAG);
    }
    LOG_I("[%s] Connecting: DNS resolved %s to %s\r\n", DBG_TAG, server, ip_str);

    // Create socket
    LOG_D("[%s] Connecting: Creating socket...\r\n", DBG_TAG);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_E("[%s] Connect failed: Socket creation error (errno: %d).\r\n", DBG_TAG, errno);
        return REQ_ERR_SOCKET;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr, hp->h_addr_list[0], hp->h_length);

    // Set non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        LOG_E("[%s] Connect failed: fcntl(F_GETFL) error (errno: %d).\r\n", DBG_TAG, errno);
        close(sockfd);
        return REQ_ERR_SOCKET;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        LOG_E("[%s] Connect failed: fcntl(F_SETFL O_NONBLOCK) error (errno: %d).\r\n", DBG_TAG, errno);
        close(sockfd);
        return REQ_ERR_SOCKET;
    }
    LOG_D("[%s] Connecting: Socket set to non-blocking.\r\n", DBG_TAG);

    // Connect (non-blocking)
    LOG_I("[%s] Connecting: Initiating connection to %s (%s):%u...\r\n", DBG_TAG, server, ip_str, port);
    int ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (ret < 0) {
        if (errno == EINPROGRESS) {
            LOG_I("[%s] Connecting: Connection in progress (EINPROGRESS)...\r\n", DBG_TAG);
            fd_set write_fds;
            struct timeval tv;

            FD_ZERO(&write_fds);
            FD_SET(sockfd, &write_fds);

            int remaining_ms = timeout_ms - (int)((bflb_mtimer_get_time_us() - start_us) / 1000);
            if (remaining_ms <= 0) {
                LOG_E("[%s] Connect failed: Timeout before select().\r\n", DBG_TAG);
                close(sockfd);
                return REQ_ERR_TIMEOUT;
            }
            tv.tv_sec = remaining_ms / 1000;
            tv.tv_usec = (remaining_ms % 1000) * 1000;
            if (tv.tv_sec == 0 && tv.tv_usec < 1000)
                tv.tv_usec = 1000;

            LOG_D("[%s] Connecting: Calling select() with timeout %d.%06ld s\r\n", DBG_TAG, (int)tv.tv_sec, tv.tv_usec);
            int select_ret = select(sockfd + 1, NULL, &write_fds, NULL, &tv);

            if (select_ret > 0) {
                int sock_err = 0;
                socklen_t err_len = sizeof(sock_err);
                if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sock_err, &err_len) == 0) {
                    if (sock_err == 0) {
                        LOG_I("[%s] Connecting: Connection successful (non-blocking).\r\n", DBG_TAG);
                        *fd_out = sockfd;
                        return REQ_OK;
                    } else {
                        LOG_E("[%s] Connect failed: Socket error after select (SO_ERROR: %d).\r\n", DBG_TAG, sock_err);
                        connect_ret_code = REQ_ERR_CONNECT;
                    }
                } else {
                    LOG_E("[%s] Connect failed: getsockopt(SO_ERROR) error (errno: %d).\r\n", DBG_TAG, errno);
                    connect_ret_code = REQ_ERR_CONNECT;
                }
            } else if (select_ret == 0) {
                LOG_E("[%s] Connect failed: Timeout during non-blocking connect (select timed out).\r\n", DBG_TAG);
                connect_ret_code = REQ_ERR_TIMEOUT;
            } else {
                LOG_E("[%s] Connect failed: select() error (errno: %d).\r\n", DBG_TAG, errno);
                connect_ret_code = REQ_ERR_CONNECT;
            }
        } else {
            LOG_E("[%s] Connect failed: connect() error (errno: %d).\r\n", DBG_TAG, errno);
            connect_ret_code = REQ_ERR_CONNECT;
        }
    } else {
        LOG_I("[%s] Connecting: Connection successful immediately (non-blocking).\r\n", DBG_TAG);
        *fd_out = sockfd;
        return REQ_OK;
    }

    close(sockfd);
    return connect_ret_code;
}

int _http_send(int32_t fd, const uint8_t *data, size_t len, int flags)
{
    int sock_fd = (int)fd;
    int bytes_sent = send(sock_fd, data, len, flags);

    if (bytes_sent > 0) {
        return bytes_sent;
    } else if (bytes_sent == 0) {
        LOG_W("[%s] Send: send() returned 0.\r\n", DBG_TAG);
        return REQ_ERR_SEND;
    } else {
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK) {
            LOG_D("[%s] Send: Socket busy (EAGAIN/EWOULDBLOCK), need retry.\r\n", DBG_TAG);
            return 0;
        } else {
            LOG_E("[%s] Send failed: send() error (errno: %d).\r\n", DBG_TAG, err);
            return REQ_ERR_SEND;
        }
    }
}

void _http_disconnect(int32_t fd)
{
    int sock_fd = (int)fd;
    if (sock_fd >= 0) {
        LOG_D("[%s] Disconnecting socket fd %d\r\n", DBG_TAG, sock_fd);
        close(sock_fd);
    }
}

/**
 * @brief recv wrapper, unified return value
 * @param fd Socket descriptor
 * @param buf Buffer for data
 * @param len Buffer size
 * @return >0: bytes received; 0: busy/retry; REQ_ERR_RECV_CLOSED: connection closed; <0: other error
 */
int _recv_wrapper(int32_t fd, uint8_t *buf, uint16_t len)
{
    int sock_fd = (int)fd;
    size_t read_len = (size_t)len;

    int read_ret = recv(sock_fd, buf, read_len, 0);

    if (read_ret > 0) {
        return read_ret;
    } else if (read_ret == 0) {
        return REQ_ERR_RECV_CLOSED;
    } else {
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK) {
            return 0;
        } else {
            LOG_E("_recv_wrapper failed. errno: %d\r\n", err);
            return REQ_ERR_RECV;
        }
    }
}

// HTTP GET
int http_client_get(const char *server, uint16_t port, const char *path,
                    const char *custom_headers,
                    uint8_t *response_buf, int response_buf_len, int *actual_resp_len_ptr,
                    int timeout_ms)
{
    http_request_config_t config = {
        .server = server,
        .port = port,
        .path = path,
        .method = "GET",
        .content_type = NULL,
        .request_body = NULL,
        .body_len = 0,
        .custom_headers = custom_headers,
        .response_buf = response_buf,
        .response_buf_len = response_buf_len,
        .actual_resp_len_ptr = actual_resp_len_ptr,
        .timeout_ms = timeout_ms
    };

    http_proto_ops_t http_ops = {
        .connect = _http_connect,
        .send = _http_send,
        .recv = _recv_wrapper,
        .disconnect = _http_disconnect
    };

    LOG_D("[%s] Calling core request function for GET.\r\n", DBG_TAG);
    return _http_request_internal(&config, &http_ops);
}

// HTTP POST
int http_client_post(const char *server, uint16_t port, const char *path,
                     const char *content_type, const char *request_body, int body_len,
                     const char *custom_headers,
                     uint8_t *response_buf, int response_buf_len, int *actual_resp_len_ptr,
                     int timeout_ms)
{
    http_request_config_t config = {
        .server = server,
        .port = port,
        .path = path,
        .method = "POST",
        .content_type = content_type,
        .request_body = request_body,
        .body_len = body_len,
        .custom_headers = custom_headers,
        .response_buf = response_buf,
        .response_buf_len = response_buf_len,
        .actual_resp_len_ptr = actual_resp_len_ptr,
        .timeout_ms = timeout_ms
    };

    http_proto_ops_t http_ops = {
        .connect = _http_connect,
        .send = _http_send,
        .recv = _recv_wrapper,
        .disconnect = _http_disconnect
    };

    LOG_D("[%s] Calling core request function for POST.\r\n", DBG_TAG);
    return _http_request_internal(&config, &http_ops);
}
