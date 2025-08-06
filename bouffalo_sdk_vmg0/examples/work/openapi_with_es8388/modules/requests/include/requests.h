// requests.h
#ifndef BL_REQUESTS_H
#define BL_REQUESTS_H

#include <stdint.h>
#include "req_error.h" // 引入我们统一的错误码定义

/**
 * @brief HTTP 请求选项结构体
 * 用于封装发起一次 HTTP/S 请求所需的所有参数。
 */
typedef struct {
    const char *method;         ///< HTTP 方法 (必须, e.g., "GET", "POST")
    const char *url;            ///< 完整的 URL (必须, e.g., "https://example.com/api")
    const char *custom_headers; ///< 额外的自定义 HTTP 头部 (可选, 以 \r\n 分隔并结尾, e.g., "Accept: application/json\r\n")
    const char *content_type;   ///< 请求体的 MIME 类型 (可选, POST/PUT 时常用, e.g., "application/json")
    const char *request_body;   ///< 指向请求体数据的指针 (可选, POST/PUT 时使用)
    int body_len;               ///< 请求体数据长度 (可选, POST/PUT 时使用)
    int timeout_ms;             ///< 整个请求的总超时时间（毫秒, 必须 > 0）
    // --- 扩展字段 (未来可能需要) ---
    // const char *ca_cert_path; ///< 用于 HTTPS 验证的 CA 证书路径 (暂不使用)
    // int verify_peer;          ///< 是否验证服务器证书 (暂不使用, 由 BL_VERIFY 宏控制)
} RequestOptions;

/**
 * @brief HTTP 响应结果结构体
 * 用于存储 HTTP/S 请求执行后的结果。
 */
typedef struct {
    /**
     * @brief HTTP 状态码或客户端错误码。
     * - >= 200: 表示成功的 HTTP 状态码 (e.g., 200 OK, 201 Created)。
     * - > 0 且 < 200 或 >= 300: 表示 HTTP 错误状态码 (e.g., 404 Not Found, 500 Server Error)。
     * - < 0: 表示请求过程中发生的客户端错误 (来自 req_err_t, e.g., REQ_ERR_TIMEOUT, REQ_ERR_CONNECT)。
     */
    int status_code;

    /**
     * @brief [输入/输出] 指向存储响应体的缓冲区的指针。
     * @warning 这个缓冲区必须由调用者分配和提供！库本身不分配响应体内存。
     */
    uint8_t *response_buf;

    /**
     * @brief [输入] 调用者提供的 response_buf 的大小（字节）。
     */
    int response_buf_len;

    /**
     * @brief [输出] 实际接收到的响应体长度（字节）。
     * 如果发生错误，此值可能为 0 或未定义。如果响应体超出了 response_buf_len，
     * 此值可能等于 response_buf_len，并且 status_code 可能会被设置为 REQ_ERR_RESP_TOO_LARGE。
     */
    int actual_resp_len;

    // --- 扩展字段 (未来可能需要) ---
    // char response_headers[...]; ///< 解析后的响应头 (暂不实现)
    // int header_len;

} Response;

/**
 * @brief 发送 HTTP/S 请求。
 *
 * 根据 RequestOptions 中的 URL 自动选择 HTTP 或 HTTPS，并执行请求。
 * 调用者需要预先分配好 Response 结构体中的 response_buf。
 *
 * @param opts 指向请求选项的指针。
 * @param resp 指向用于存储响应结果的结构体的指针。
 * 其中的 response_buf 和 response_buf_len 必须由调用者设置。
 * @return int 返回最终的 HTTP 状态码 (>= 200) 或客户端错误码 (< 0, 来自 req_err_t)。
 */
int send_request(RequestOptions *opts, Response *resp);

#endif // BL_REQUESTS_H