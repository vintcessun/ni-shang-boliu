#ifndef REQ_ERROR_H
#define REQ_ERROR_H

// 统一的客户端请求错误码
typedef enum {
    // 成功或 HTTP 状态码
    // REQ_OK 仅用于内部表示操作本身无错
    REQ_OK = 0,

    // 客户端通用错误 (-3001 ~ -3099)
    REQ_ERR_MEM = -3001,            // 内存分配失败
    REQ_ERR_DNS = -3002,            // DNS 解析失败
    REQ_ERR_SOCKET = -3003,         // Socket 创建失败
    REQ_ERR_CONNECT = -3004,        // 连接失败
    REQ_ERR_SEND = -3005,           // 发送失败
    REQ_ERR_RECV = -3006,           // 接收失败
    REQ_ERR_TIMEOUT = -3007,        // 操作超时
    REQ_ERR_BAD_REQUEST = -3008,    // 请求构建失败/参数错误
    REQ_ERR_RESP_PARSE = -3009,     // 响应解析失败
    REQ_ERR_RESP_TOO_LARGE = -3010, // 响应缓冲区太小
    REQ_ERR_INTERNAL = -3011,       // 其他内部错误

    // 特定通信阶段错误 (-3100 ~ -3199)
    REQ_ERR_RECV_CLOSED = -3100,    // 接收时连接正常关闭

    // 可扩展更多错误码

} req_err_t;

#endif // REQ_ERROR_H
