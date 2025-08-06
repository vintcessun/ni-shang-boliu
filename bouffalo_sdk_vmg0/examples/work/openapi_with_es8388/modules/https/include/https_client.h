#ifndef __HTTPS_CLIENT_H__
#define __HTTPS_CLIENT_H__

#include <stdint.h>
#include "req_error.h"

/**
 * @brief 执行 HTTPS GET 请求
 *
 * @param server            服务器域名或 IP
 * @param port              服务器端口 (通常为 443)
 * @param path              请求路径（含查询参数）
 * @param custom_headers    额外 HTTP 头部（可为 NULL，每行以 \r\n 结尾）
 * @param response_buf      [OUT] 响应体缓冲区（以 '\0' 结尾）
 * @param response_buf_len  缓冲区大小（字节）
 * @param actual_resp_len_ptr [OUT] 实际响应体长度（不含 '\0'）
 * @param timeout_ms        总超时时间（毫秒）
 *
 * @return int              成功返回 HTTP 状态码，失败返回负错误码
 */
int https_client_get(const char *server, uint16_t port, const char *path,
                     const char *custom_headers,
                     uint8_t *response_buf, int response_buf_len, int *actual_resp_len_ptr,
                     int timeout_ms);

/**
 * @brief 执行 HTTPS POST 请求
 *
 * @param server            服务器域名或 IP
 * @param port              服务器端口 (通常为 443)
 * @param path              请求路径
 * @param content_type      请求体 MIME 类型
 * @param request_body      请求体数据
 * @param body_len          请求体长度
 * @param custom_headers    额外 HTTP 头部（可为 NULL，每行以 \r\n 结尾）
 * @param response_buf      [OUT] 响应体缓冲区（以 '\0' 结尾）
 * @param response_buf_len  缓冲区大小（字节）
 * @param actual_resp_len_ptr [OUT] 实际响应体长度（不含 '\0'）
 * @param timeout_ms        总超时时间（毫秒）
 *
 * @return int              成功返回 HTTP 状态码，失败返回负错误码
 */
int https_client_post(const char *server, uint16_t port, const char *path,
                      const char *content_type, const char *request_body, int body_len,
                      const char *custom_headers,
                      uint8_t *response_buf, int response_buf_len, int *actual_resp_len_ptr,
                      int timeout_ms);

#endif // __HTTPS_CLIENT_H__
