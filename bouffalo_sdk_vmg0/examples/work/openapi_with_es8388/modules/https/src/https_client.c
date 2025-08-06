#include "https_client.h"
#include "core_http_client.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_mtimer.h"
#include "log.h"
#include "https.h"     // For blTcpSsl* functions
#include "bl_error.h"  // For BL_TCP_* error codes
#include "req_error.h" // 统一错误码头文件
#include "bl_utils.h"  // memmem 工具头文件
#define DBG_TAG "HTTPS_CLIENT"

/* ========================================================================== */
/* 内部辅助函数 (HTTPS 版本)                                                 */
/* ========================================================================== */

// --- HTTPS 协议操作封装 ---

int _https_connect(const char *server, uint16_t port, int timeout_ms, int32_t *fd_out)
{
    int32_t fd = -1;
    int32_t state;
    uint64_t start_us = bflb_mtimer_get_time_us();
    uint64_t connect_start_us;
    int ret_err = REQ_ERR_INTERNAL;

    *fd_out = -1;

    LOG_I("[%s] Connecting (TLS): Attempting to connect to %s:%u...\r\n", DBG_TAG, server, port);

    fd = blTcpSslConnect(server, port);

    // 处理 blTcpSslConnect 的错误返回
    if (fd == BL_TCP_ARG_INVALID) {
        LOG_E("[%s] Connect failed: blTcpSslConnect returned Invalid Arguments.\r\n", DBG_TAG);
        return REQ_ERR_BAD_REQUEST;
    } else if (fd == BL_TCP_CREATE_CONNECT_ERR) {
        LOG_E("[%s] Connect failed: blTcpSslConnect returned Create Connect Error (%ld).\r\n", DBG_TAG, fd);
        return REQ_ERR_CONNECT;
    } else if (fd == BL_TCP_DNS_PARSE_ERR) {
        LOG_E("[%s] Connect failed: blTcpSslConnect returned DNS Parse Error (%ld).\r\n", DBG_TAG, fd);
        return REQ_ERR_DNS;
    } else if (fd == 0) {
        LOG_E("[%s] Connect failed: blTcpSslConnect returned unexpected value 0.\r\n", DBG_TAG);
        return REQ_ERR_CONNECT;
    }
    LOG_I("[%s] Connecting (TLS): blTcpSslConnect returned handle: %ld (0x%08lX). Waiting for handshake...\r\n", DBG_TAG, fd, fd);
    connect_start_us = bflb_mtimer_get_time_us();

    // 检查 TLS 握手状态
    while (1) {
        if ((bflb_mtimer_get_time_us() - start_us) / 1000 > timeout_ms) {
            LOG_E("[%s] Connect failed: Timeout waiting for TLS handshake completion.\r\n", DBG_TAG);
            blTcpSslDisconnect(fd);
            return REQ_ERR_TIMEOUT;
        }

        state = blTcpSslState(fd);

        if (state == BL_TCP_NO_ERROR) {
            LOG_I("[%s] Connecting (TLS): Handshake successful!\r\n", DBG_TAG);
            *fd_out = fd;
            return REQ_OK;
        } else if (state == BL_TCP_CONNECTING || state == BL_TCP_DNS_PARSING) {
            LOG_D("[%s] Connecting (TLS): State is Connecting/DNS Parsing (%ld), waiting...\r\n", DBG_TAG, state);
            if ((bflb_mtimer_get_time_us() - connect_start_us) / 1000 > (timeout_ms > 15000 ? 15000 : timeout_ms)) {
                LOG_E("[%s] Connect failed: Handshake phase timeout.\r\n", DBG_TAG);
                blTcpSslDisconnect(fd);
                return REQ_ERR_TIMEOUT;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            LOG_E("[%s] Connect failed: Handshake/Connect failed with state code: %ld\r\n", DBG_TAG, state);
            blTcpSslDisconnect(fd);
            if (state == BL_TCP_CONNECT_TIMEOUT || state == BL_TCP_DNS_PARSE_TIMEOUT) {
                ret_err = REQ_ERR_TIMEOUT;
            } else if (state == BL_TCP_DNS_PARSE_ERR) {
                ret_err = REQ_ERR_DNS;
            } else {
                ret_err = REQ_ERR_CONNECT;
            }
            return ret_err;
        }
    }
}

int _https_send(int32_t fd, const uint8_t *data, size_t len, int flags)
{
    // blTcpSslSend 的长度参数为 uint16_t
    if (len > UINT16_MAX) {
        LOG_E("[%s] Send failed: Data length (%zu) exceeds uint16_t limit for blTcpSslSend.\r\n", DBG_TAG, len);
        return REQ_ERR_BAD_REQUEST;
    }
    uint16_t send_len = (uint16_t)len;
    int32_t send_ret = blTcpSslSend(fd, data, send_len);

    if (send_ret > 0) {
        return (int)send_ret;
    } else if (send_ret == 0) {
        LOG_D("[%s] Send busy/retry (0) from blTcpSslSend.\r\n", DBG_TAG);
        return 0;
    } else {
        LOG_E("[%s] Send failed: blTcpSslSend returned error code: %ld\r\n", DBG_TAG, send_ret);
        if (send_ret == BL_TCP_SEND_ERR) {
            return REQ_ERR_SEND;
        } else if (send_ret == BL_TCP_ARG_INVALID) {
            return REQ_ERR_BAD_REQUEST;
        } else {
            return REQ_ERR_SEND;
        }
    }
}

/**
 * @brief blTcpSslRead 的包装，统一返回值
 * @param fd TLS 句柄
 * @param buf 缓冲区
 * @param len 缓冲区大小
 * @return >0: 接收字节数; 0: 忙/重试; REQ_ERR_RECV_CLOSED: 连接关闭; <0: 其他错误
 */
int _bl_ssl_read_wrapper(int32_t fd, uint8_t *buf, uint16_t len)
{
    int32_t read_ret = blTcpSslRead(fd, buf, len);
    if (read_ret > 0) {
        return (int)read_ret;
    } else if (read_ret == 0) {
        return 0;
    } else if (read_ret == BL_TCP_READ_ERR) {
        return REQ_ERR_RECV_CLOSED;
    } else {
        LOG_E("_bl_ssl_read_wrapper failed with code: %ld\r\n", read_ret);
        return REQ_ERR_RECV;
    }
}

/**
 * @brief 处理接收和解析 HTTP/S 响应
 * @param recv_func 实际读取数据的函数指针
 * @param fd 句柄
 * @param response_buf 响应缓冲区
 * @param response_buf_len 缓冲区大小
 * @param actual_resp_len_ptr [OUT] 实际接收体长度
 * @param start_us 操作开始时间戳
 * @param timeout_ms 超时时间
 * @return HTTP 状态码 (>=200) 或负数错误码
 */
static int _process_response(
    int (*recv_func)(int32_t fd, uint8_t *buf, uint16_t len),
    int32_t fd,
    uint8_t *response_buf, int response_buf_len, int *actual_resp_len_ptr,
    uint64_t start_us, int timeout_ms)
{
    int ret_code = REQ_ERR_INTERNAL;
    int total_received = 0;
    *actual_resp_len_ptr = 0;

    LOG_I("Waiting for response...\r\n");
    while (1) {
        if ((bflb_mtimer_get_time_us() - start_us) / 1000 > timeout_ms) {
            if (ret_code >= 0 || ret_code == REQ_ERR_INTERNAL)
                ret_code = REQ_ERR_TIMEOUT;
            break;
        }
        int space_left = response_buf_len - 1 - total_received;
        if (space_left <= 0) {
            if (ret_code >= 0 || ret_code == REQ_ERR_INTERNAL)
                ret_code = REQ_ERR_RESP_TOO_LARGE;
            break;
        }
        uint16_t len_to_read = (space_left > UINT16_MAX) ? UINT16_MAX : (uint16_t)space_left;
        int read_ret = recv_func(fd, response_buf + total_received, len_to_read);

        if (read_ret > 0) {
            total_received += read_ret;
            LOG_D("Recv %d bytes (total %d)\r\n", read_ret, total_received);
        } else if (read_ret == 0) {
            LOG_D("Recv func busy/retry (0)\r\n");
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        } else if (read_ret == REQ_ERR_RECV_CLOSED) {
            LOG_I("Connection closed.\r\n");
            if (ret_code == REQ_ERR_INTERNAL)
                ret_code = REQ_OK;
            break;
        } else {
            LOG_E("Recv func failed: %d\r\n", read_ret);
            if (ret_code >= 0 || ret_code == REQ_ERR_INTERNAL)
                ret_code = read_ret;
            break;
        }
    }
    // Null 结尾
    if (total_received < response_buf_len)
        response_buf[total_received] = '\0';
    else {
        response_buf[response_buf_len - 1] = '\0';
        if (ret_code == REQ_OK || ret_code == REQ_ERR_INTERNAL)
            ret_code = REQ_ERR_RESP_TOO_LARGE;
    }
    *actual_resp_len_ptr = total_received;
    LOG_I("Total received: %d bytes\r\n", total_received);
    if (ret_code >= 0 && (bflb_mtimer_get_time_us() - start_us) / 1000 > timeout_ms) {
        ret_code = REQ_ERR_TIMEOUT;
    }

    // 解析响应头
    int http_status = -1;
    char *body_ptr = NULL;
    int resp_header_len = 0;
    int resp_body_len = 0;
    if (*actual_resp_len_ptr > 0 && ret_code >= 0) {
        char *header_end_ptr = memmem(response_buf, *actual_resp_len_ptr, "\r\n\r\n", 4);
        if (header_end_ptr != NULL) {
            resp_header_len = (header_end_ptr - (char *)response_buf) + 4;
            body_ptr = header_end_ptr + 4;
            resp_body_len = *actual_resp_len_ptr - resp_header_len;
            if (resp_body_len < 0)
                resp_body_len = 0;
            if (sscanf((char *)response_buf, "HTTP/%*d.%*d %d", &http_status) == 1) {
                LOG_I("Parsed HTTP Status: %d\r\n", http_status);
                ret_code = http_status;
            } else {
                LOG_E("Status line parse failed.\r\n");
                ret_code = REQ_ERR_RESP_PARSE;
                resp_body_len = 0;
            }
            if (resp_body_len > 0)
                memmove(response_buf, body_ptr, resp_body_len);
            *actual_resp_len_ptr = resp_body_len;
            response_buf[resp_body_len] = '\0';
        } else {
            LOG_E("Header end marker not found.\r\n");
            if (ret_code >= 0)
                ret_code = REQ_ERR_RESP_PARSE;
            *actual_resp_len_ptr = 0;
            response_buf[0] = '\0';
        }
    } else if (ret_code >= 0) {
        LOG_W("No data received.\r\n");
        ret_code = REQ_ERR_RECV;
    }

    return ret_code;
}

int https_client_get(const char *server, uint16_t port, const char *path,
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

    http_proto_ops_t https_ops = {
        .connect = _https_connect,
        .send = _https_send,
        .recv = _bl_ssl_read_wrapper,
        .disconnect = blTcpSslDisconnect
    };

    LOG_D("[%s] Calling core request function for HTTPS GET.\r\n", DBG_TAG);
    return _http_request_internal(&config, &https_ops);
}

int https_client_post(const char *server, uint16_t port, const char *path,
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

    http_proto_ops_t https_ops = {
        .connect = _https_connect,
        .send = _https_send,
        .recv = _bl_ssl_read_wrapper,
        .disconnect = blTcpSslDisconnect
    };

    LOG_D("[%s] Calling core request function for HTTPS POST.\r\n", DBG_TAG);
    return _http_request_internal(&config, &https_ops);
}
