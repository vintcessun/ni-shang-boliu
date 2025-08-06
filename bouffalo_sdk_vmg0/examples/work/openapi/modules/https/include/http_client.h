#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include <stdint.h>
#include "req_error.h"

/**
 * @brief 执行 HTTP GET 请求 (非 HTTPS)
 *
 * @param server            服务器域名或 IP
 * @param port              端口 (通常为 80)
 * @param path              请求路径 (如 "/search?query=bl618&page=1")
 * @param custom_headers    额外 HTTP 头部 (每行以 \r\n 结尾), 可为 NULL
 * @param response_buf      [OUT] 响应体缓冲区 ('\0' 结尾)
 * @param response_buf_len  缓冲区大小 (字节)
 * @param actual_resp_len_ptr [OUT] 实际响应体长度 (不含 '\0')
 * @param timeout_ms        超时时间 (毫秒)
 *
 * @return int              成功返回 HTTP 状态码，失败返回负的 REQ_ERR_* 错误码
 */
int http_client_get(const char *server, uint16_t port, const char *path,
                    const char *custom_headers,
                    uint8_t *response_buf, int response_buf_len, int *actual_resp_len_ptr,
                    int timeout_ms);

/**
 * @brief 执行 HTTP POST 请求 (非 HTTPS)
 *
 * @param server            服务器域名或 IP
 * @param port              端口 (通常为 80)
 * @param path              请求路径 (如 "/api/resource")
 * @param content_type      请求体 MIME 类型 (如 "application/json")
 * @param request_body      请求体数据
 * @param body_len          请求体长度
 * @param custom_headers    额外 HTTP 头部 (每行以 \r\n 结尾), 可为 NULL
 * @param response_buf      [OUT] 响应体缓冲区 ('\0' 结尾)
 * @param response_buf_len  缓冲区大小 (字节)
 * @param actual_resp_len_ptr [OUT] 实际响应体长度 (不含 '\0')
 * @param timeout_ms        超时时间 (毫秒)
 *
 * @return int              成功返回 HTTP 状态码，失败返回负的 REQ_ERR_* 错误码
 */
int http_client_post(const char *server, uint16_t port, const char *path,
                     const char *content_type, const char *request_body, int body_len,
                     const char *custom_headers,
                     uint8_t *response_buf, int response_buf_len, int *actual_resp_len_ptr,
                     int timeout_ms);

#endif // __HTTP_CLIENT_H__
