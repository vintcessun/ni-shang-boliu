// requests.c
#include "requests.h"         // 包含我们定义的结构体和函数声明
#include "url_parser.h"       // 包含 URL 解析器
#include "core_http_client.h" // 包含核心 HTTP 请求函数 _http_request_internal
#include "http_client.h"      // 需要访问 HTTP 的协议操作函数 (可能需要调整可见性)
#include "https_client.h"     // 需要访问 HTTPS 的协议操作函数 (可能需要调整可见性)
#include "https.h"            // 需要 blTcpSslDisconnect 等 (https_client 内部调用)
#include "log.h"
#include <string.h>

#ifndef DBG_TAG
#define DBG_TAG "REQUESTS"
#endif

// --- 从 http_client.c 和 https_client.c 中 "借用" 协议操作函数 ---
// 注意：如果这些函数在原文件中是 static 的，这里需要将它们在原文件中改为非 static
// 或者将 requests.c 的内容合并到 http_client.c/https_client.c 中。
// 我们先假设可以访问。

// --- HTTP 操作函数集 (来自 http_client.c) ---
// 需要确保 http_client.c 中的 _http_connect, _http_send, _recv_wrapper, _http_disconnect 可被调用
extern int _http_connect(const char *server, uint16_t port, int timeout_ms, int32_t *fd_out);
extern int _http_send(int32_t fd, const uint8_t *data, size_t len, int flags);
extern int _recv_wrapper(int32_t fd, uint8_t *buf, uint16_t len); // 来自 http_client.c
extern void _http_disconnect(int32_t fd);

static const http_proto_ops_t http_ops = {
    .connect = _http_connect,
    .send = _http_send,
    .recv = _recv_wrapper,
    .disconnect = _http_disconnect
};

// --- HTTPS 操作函数集 (来自 https_client.c) ---
// 需要确保 https_client.c 中的 _https_connect, _https_send, _bl_ssl_read_wrapper 可被调用
// 以及 https.c 中的 blTcpSslDisconnect 可被调用
extern int _https_connect(const char *server, uint16_t port, int timeout_ms, int32_t *fd_out);
extern int _https_send(int32_t fd, const uint8_t *data, size_t len, int flags);
extern int _bl_ssl_read_wrapper(int32_t fd, uint8_t *buf, uint16_t len); // 来自 https_client.c
extern void blTcpSslDisconnect(int32_t fd);                              // 来自 https.c

static const http_proto_ops_t https_ops = {
    .connect = _https_connect,
    .send = _https_send,
    .recv = _bl_ssl_read_wrapper,
    .disconnect = blTcpSslDisconnect
};

// --- 统一请求函数实现 ---
int send_request(RequestOptions *opts, Response *resp)
{
    // 1. 输入参数校验
    if (!opts || !resp) {
        LOG_E("RequestOptions or Response pointer is NULL.\r\n");
        return REQ_ERR_BAD_REQUEST; // 使用我们定义的错误码
    }
    if (!opts->method || !opts->url || opts->timeout_ms <= 0) {
        LOG_E("Missing required options: method, url, or positive timeout.\r\n");
        return REQ_ERR_BAD_REQUEST;
    }
    if (!resp->response_buf || resp->response_buf_len <= 0) {
        LOG_E("Response buffer not provided or invalid length.\r\n");
        return REQ_ERR_BAD_REQUEST;
    }

    // 2. 初始化响应结构体
    resp->status_code = REQ_ERR_INTERNAL; // 默认内部错误
    resp->actual_resp_len = 0;
    // 确保缓冲区是干净的（至少开头是 null 结尾）
    resp->response_buf[0] = '\0';

    // 3. 解析 URL
    parsed_url_t parsed_url;
    int parse_ret = parse_url(opts->url, &parsed_url);
    if (parse_ret != URL_PARSE_OK) {
        LOG_E("URL parsing failed for '%s' with error code %d.\r\n", opts->url, parse_ret);
        resp->status_code = REQ_ERR_BAD_REQUEST; // 可以映射为更具体的错误，但暂时用 BAD_REQUEST
        return resp->status_code;
    }

    LOG_D("URL parsed: Scheme=%s, Host=%s, Port=%u, Path=%s\r\n",
          parsed_url.scheme, parsed_url.host, parsed_url.port, parsed_url.path);

    // 4. 选择协议操作集
    const http_proto_ops_t *selected_ops = NULL;
    if (strcmp(parsed_url.scheme, "http") == 0) {
        selected_ops = &http_ops;
        LOG_D("Selected HTTP protocol operations.\r\n");
    } else if (strcmp(parsed_url.scheme, "https") == 0) {
        selected_ops = &https_ops;
        LOG_D("Selected HTTPS protocol operations.\r\n");
    } else {
        LOG_E("Unsupported scheme: %s\r\n", parsed_url.scheme);
        resp->status_code = REQ_ERR_BAD_REQUEST; // 或定义一个 REQ_ERR_UNSUPPORTED_SCHEME
        return resp->status_code;
    }

    // 5. 准备核心请求配置
    http_request_config_t core_config = {
        .server = parsed_url.host,
        .port = parsed_url.port,
        .path = parsed_url.path,
        .method = opts->method,
        .content_type = opts->content_type, // 可能为 NULL
        .request_body = opts->request_body, // 可能为 NULL
        .body_len = opts->body_len,
        .custom_headers = opts->custom_headers, // 可能为 NULL
        .response_buf = resp->response_buf,
        .response_buf_len = resp->response_buf_len,
        .actual_resp_len_ptr = &resp->actual_resp_len,
        .timeout_ms = opts->timeout_ms
    };

    // 6. 调用核心 HTTP/S 请求函数
    LOG_I("Sending %s request to %s://%s:%d%s\r\n",
          core_config.method, parsed_url.scheme, core_config.server, core_config.port, core_config.path);

    resp->status_code = _http_request_internal(&core_config, selected_ops);

    LOG_I("Request finished. Final status code: %d, Actual response length: %d\r\n",
          resp->status_code, resp->actual_resp_len);

    // 7. 返回最终状态码 (HTTP 状态码或客户端错误码)
    return resp->status_code;
}