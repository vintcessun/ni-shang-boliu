#ifndef CORE_HTTP_CLIENT_H
#define CORE_HTTP_CLIENT_H

#include <stdint.h>
#include "req_error.h" // 统一错误码定义
#include <stddef.h>

/**
 * @brief HTTP 请求配置结构体
 * 用于描述一次 HTTP 请求的所有参数，包括服务器信息、请求方法、路径、请求体、超时等。
 */
typedef struct {
    const char *server;         ///< 服务器域名或 IP 地址
    uint16_t port;              ///< 服务器端口号
    const char *path;           ///< 请求路径（包含查询参数）
    const char *method;         ///< HTTP 方法（如 "GET", "POST", "PUT", "DELETE" 等）
    const char *content_type;   ///< 请求体的 MIME 类型（POST/PUT 时使用）
    const char *request_body;   ///< 指向请求体数据的指针（POST/PUT 时使用）
    int body_len;               ///< 请求体数据长度（POST/PUT 时使用）
    const char *custom_headers; ///< 额外自定义 HTTP 头部（以 \r\n 分隔并结尾）
    uint8_t *response_buf;      ///< [OUT] 存储响应体的缓冲区
    int response_buf_len;       ///< 响应缓冲区大小
    int *actual_resp_len_ptr;   ///< [OUT] 实际响应体长度的指针
    int timeout_ms;             ///< 整个请求的总超时时间（毫秒）
    // 可扩展字段：如是否允许重定向、重试次数等
} http_request_config_t;

/**
 * @brief 协议操作函数指针结构体
 * 用于抽象底层传输协议（如 TCP、SSL）的连接、发送、接收和断开操作。
 */
typedef struct {
    /**
     * @brief 连接到服务器
     * @param server 服务器地址
     * @param port 端口号
     * @param timeout_ms 连接超时时间（毫秒）
     * @param fd_out [OUT] 成功时返回连接句柄
     * @return int 成功返回 REQ_OK (0)，失败返回负的 req_err_t 错误码
     */
    int (*connect)(const char *server, uint16_t port, int timeout_ms, int32_t *fd_out);

    /**
     * @brief 发送数据
     * @param fd 连接句柄
     * @param data 要发送的数据缓冲区
     * @param len 要发送的数据长度
     * @param flags 保留参数，当前未使用
     * @return int 成功返回实际发送字节数 (>0)，忙/需重试返回 0，失败返回负的 req_err_t 错误码
     */
    int (*send)(int32_t fd, const uint8_t *data, size_t len, int flags);

    /**
     * @brief 接收数据
     * @param fd 连接句柄
     * @param buf 接收数据的缓冲区
     * @param len 缓冲区大小
     * @return int >0: 接收字节数; 0: 忙/重试; REQ_ERR_RECV_CLOSED: 连接关闭; <0 (除CLOSED外): 其他错误
     */
    int (*recv)(int32_t fd, uint8_t *buf, uint16_t len);

    /**
     * @brief 断开连接并清理资源
     * @param fd 连接句柄
     */
    void (*disconnect)(int32_t fd);

} http_proto_ops_t;

/**
 * @brief 构建 HTTP 请求头到缓冲区
 *
 * @param method        HTTP 方法（如 "GET", "POST"）
 * @param path          请求路径（如 "/index.html"）
 * @param server        服务器域名或 IP（用于 Host 头部）
 * @param content_type  （可选）请求体的 Content-Type（仅 POST/PUT 等需要）
 * @param body_len      请求体长度（用于 Content-Length，仅 POST/PUT 等需要）
 * @param custom_headers （可选）额外自定义 HTTP 头部字符串（必须以 \r\n 结尾）
 * @param buffer        [OUT] 存储生成头部的缓冲区
 * @param buffer_size   缓冲区总大小
 *
 * @return int 成功返回实际构建的头部长度 (>0)，失败返回负的 req_err_t 错误码
 */
int _build_http_request_header(const char *method, const char *path, const char *server,
                               const char *content_type, int body_len,
                               const char *custom_headers,
                               char *buffer, int buffer_size);

/**
 * @brief 内部核心 HTTP/S 请求函数
 *
 * @param config 指向请求配置的指针
 * @param ops 指向协议操作函数集的指针
 * @return int 成功返回 HTTP 状态码 (>=200)，失败返回负的 req_err_t 错误码
 */
int _http_request_internal(const http_request_config_t *config, const http_proto_ops_t *ops);

#endif // CORE_HTTP_CLIENT_H