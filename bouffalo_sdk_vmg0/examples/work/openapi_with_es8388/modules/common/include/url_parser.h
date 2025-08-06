// url_parser.h
#ifndef URL_PARSER_H
#define URL_PARSER_H

#include <stdint.h>

// 定义解析后的 URL 各部分存储结构体
// 注意：这里的缓冲区大小是固定的，需要确保 URL 不会太长导致溢出。
// 如果需要支持更长的 URL，可能需要动态分配内存或更大的缓冲区。
#define URL_SCHEME_MAX_LEN 7   // "https" + '\0'
#define URL_HOST_MAX_LEN   128 // 主机名最大长度
#define URL_PATH_MAX_LEN   512 // 路径和查询参数最大长度

typedef struct {
    char scheme[URL_SCHEME_MAX_LEN]; // 协议/方案 (例如 "http" 或 "https")
    char host[URL_HOST_MAX_LEN];     // 主机名 (例如 "www.example.com")
    uint16_t port;                   // 端口号 (例如 80 或 443)
    char path[URL_PATH_MAX_LEN];     // 路径 (例如 "/index.html?query=1")
} parsed_url_t;

// 定义解析错误码
#define URL_PARSE_OK                     0  // 解析成功
#define URL_PARSE_ERR_INVALID            -1 // URL 格式无效
#define URL_PARSE_ERR_NO_HOST            -2 // 未找到主机名
#define URL_PARSE_ERR_BAD_PORT           -3 // 端口号格式错误
#define URL_PARSE_ERR_BUF_OVF            -4 // 内部缓冲区溢出 (主机或路径太长)
#define URL_PARSE_ERR_UNSUPPORTED_SCHEME -5 // 不支持的协议 (非 http/https)

/**
 * @brief 解析给定的 URL 字符串。
 *
 * @param url 要解析的 URL 字符串。
 * @param result 指向 parsed_url_t 结构体的指针，用于存储解析结果。
 * @return int 成功返回 URL_PARSE_OK (0)，失败返回负数错误码。
 */
int parse_url(const char *url, parsed_url_t *result);

#endif // URL_PARSER_H