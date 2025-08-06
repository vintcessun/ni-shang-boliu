// url_parser.c
#include "url_parser.h"
#include <string.h>
#include <stdio.h>  // for sscanf
#include <stdlib.h> // for atoi
#include <ctype.h>  // for tolower

// 内部辅助函数，用于安全地复制字符串，防止溢出
static int safe_strncpy(char *dest, const char *src, size_t dest_size, size_t count)
{
    if (count >= dest_size) {
        dest[0] = '\0';               // 清空目标缓冲区以示错误
        return URL_PARSE_ERR_BUF_OVF; // 返回缓冲区溢出错误
    }
    strncpy(dest, src, count);
    dest[count] = '\0'; // 确保 null 结尾
    return URL_PARSE_OK;
}

int parse_url(const char *url, parsed_url_t *result)
{
    if (!url || !result) {
        return URL_PARSE_ERR_INVALID;
    }

    // 初始化结果结构体
    memset(result, 0, sizeof(parsed_url_t));
    result->port = 0;          // 默认端口未设置
    strcpy(result->path, "/"); // 默认路径为根

    const char *scheme_ptr = strstr(url, "://");
    const char *authority_ptr = url; // 指向权限部分的开始 (host[:port])

    // 1. 解析协议 (scheme)
    if (scheme_ptr != NULL) {
        size_t scheme_len = scheme_ptr - url;
        if (safe_strncpy(result->scheme, url, URL_SCHEME_MAX_LEN, scheme_len) != URL_PARSE_OK) {
            return URL_PARSE_ERR_BUF_OVF;
        }
        // 转小写
        for (int i = 0; result->scheme[i]; i++) {
            result->scheme[i] = tolower(result->scheme[i]);
        }

        if (strcmp(result->scheme, "http") != 0 && strcmp(result->scheme, "https") != 0) {
            return URL_PARSE_ERR_UNSUPPORTED_SCHEME;
        }
        authority_ptr = scheme_ptr + 3; // 跳过 "://"
    } else {
        // 如果没有 "://", 默认使用 http
        strcpy(result->scheme, "http");
    }

    // 设置默认端口
    if (strcmp(result->scheme, "http") == 0) {
        result->port = 80;
    } else if (strcmp(result->scheme, "https") == 0) {
        result->port = 443;
    }

    // 2. 查找权限部分的结束和路径的开始
    const char *path_ptr = strchr(authority_ptr, '/');
    const char *authority_end_ptr = path_ptr ? path_ptr : (authority_ptr + strlen(authority_ptr));

    if (authority_ptr == authority_end_ptr) {
        return URL_PARSE_ERR_NO_HOST; // 权限部分为空，没有主机名
    }

    // 3. 解析主机名 (host) 和端口号 (port)
    const char *port_ptr = NULL;
    // IPv6 地址可能包含 ':', 但会被 '[' 和 ']' 包裹，先简单处理 IPv4 和域名
    // TODO: 添加 IPv6 地址支持 ([...]:port)
    port_ptr = strchr(authority_ptr, ':');

    if (port_ptr != NULL && port_ptr < authority_end_ptr) { // 找到了端口分隔符 ':' 且在权限部分内
        size_t host_len = port_ptr - authority_ptr;
        if (safe_strncpy(result->host, authority_ptr, URL_HOST_MAX_LEN, host_len) != URL_PARSE_OK) {
            return URL_PARSE_ERR_BUF_OVF;
        }

        // 解析端口号
        if (sscanf(port_ptr + 1, "%hu", &result->port) != 1) {
            // 检查是否端口部分为空或非数字
            const char *p = port_ptr + 1;
            if (p == authority_end_ptr || !isdigit(*p)) {
                return URL_PARSE_ERR_BAD_PORT;
            }
            // 尝试手动转换，如果 sscanf 失败
            result->port = (uint16_t)atoi(port_ptr + 1);
            if (result->port == 0 && *(port_ptr + 1) != '0') { // atoi 遇到非数字返回 0
                return URL_PARSE_ERR_BAD_PORT;
            }
        }
        // 检查 authority_end_ptr 是否紧跟在数字之后
        const char *port_end_check = port_ptr + 1;
        while (isdigit(*port_end_check))
            port_end_check++;
        if (port_end_check != authority_end_ptr) {
            // 端口号后面还有非数字字符，且不是路径分隔符 '/'
            return URL_PARSE_ERR_BAD_PORT;
        }

    } else { // 没有找到端口分隔符
        size_t host_len = authority_end_ptr - authority_ptr;
        if (safe_strncpy(result->host, authority_ptr, URL_HOST_MAX_LEN, host_len) != URL_PARSE_OK) {
            return URL_PARSE_ERR_BUF_OVF;
        }
        // 端口使用上面设置的默认值
    }

    // 4. 解析路径 (path)
    if (path_ptr != NULL) { // 如果找到了路径分隔符 '/'
        size_t path_len = strlen(path_ptr);
        if (safe_strncpy(result->path, path_ptr, URL_PATH_MAX_LEN, path_len) != URL_PARSE_OK) {
            return URL_PARSE_ERR_BUF_OVF;
        }
    } else {
        // 如果 URL 中完全没有路径部分 (e.g., "http://example.com")，保持默认路径 "/"
        // strcpy(result->path, "/"); // 已在初始化时设置
    }

    return URL_PARSE_OK;
}