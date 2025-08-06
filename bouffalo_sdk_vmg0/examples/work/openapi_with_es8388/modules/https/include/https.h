#ifndef __HTTPS_H__
#define __HTTPS_H__

#include <stdint.h>

/**
 * @brief 创建非阻塞加密TCP连接
 * @param dst  目标IP地址
 * @param port 目标端口号
 * @retval <0  失败
 * @retval >0  成功，返回套接字
 */
int32_t blTcpSslConnect(const char* dst, uint16_t port);

/**
 * @brief 查询加密TCP连接状态
 * @param fd  套接字
 * @retval 错误码
 */
int32_t blTcpSslState(int32_t fd);

/**
 * @brief 断开加密TCP连接
 * @param fd  套接字
 */
void blTcpSslDisconnect(int32_t fd);

/**
 * @brief 发送加密TCP数据（非阻塞）
 * @param fd   套接字
 * @param buf  待发送数据缓冲区
 * @param len  数据长度 [0, 512)
 * @retval 错误码
 */
int32_t blTcpSslSend(int32_t fd, const uint8_t* buf, uint16_t len);

/**
 * @brief 读取加密TCP数据（非阻塞）
 * @param fd   套接字
 * @param buf  接收数据缓冲区
 * @param len  缓冲区最大长度 [0, 512)
 * @retval 错误码
 */
int32_t blTcpSslRead(int32_t fd, uint8_t* buf, uint16_t len);

#endif
