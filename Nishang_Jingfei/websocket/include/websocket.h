#ifndef __WEBSOCKET_H_
#define __WEBSOCKET_H_

#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <stdbool.h>
#include <stdint.h>

// WebSocket帧类型
typedef enum {
	WS_OPCODE_CONTINUE = 0x00,
	WS_OPCODE_TEXT = 0x01,
	WS_OPCODE_BINARY = 0x02,
	WS_OPCODE_CLOSE = 0x08,
	WS_OPCODE_PING = 0x09,
	WS_OPCODE_PONG = 0x0A
} ws_opcode_t;

// WebSocket连接状态
typedef enum {
	WS_STATE_DISCONNECTED,
	WS_STATE_CONNECTING,
	WS_STATE_CONNECTED,
	WS_STATE_ERROR
} ws_state_t;

// WebSocket客户端结构体(模拟类)
typedef struct {
	int socket_fd;		  // 套接字句柄
	ws_state_t state;	  // 连接状态
	char host[64];		  // 服务器主机名
	uint16_t port;		  // 服务器端口
	char *path;			  // 请求路径
	char error_msg[128];  // 错误信息
} websocket_client_t;

/**
 * 解析ws://格式的URL并初始化客户端
 * @param url 格式如"ws://host:port/path"
 * @param client 客户端对象指针
 * @return 成功返回true，失败返回false
 */
bool websocket_init(websocket_client_t *client, const char *url);

/**
 * 连接到WebSocket服务器
 * @param client 客户端对象
 * @return 成功返回true，失败返回false
 */
bool websocket_connect(websocket_client_t *client);

/**
 * 发送WebSocket消息
 * @param client 客户端对象
 * @param opcode 消息类型
 * @param data 消息数据
 * @param len 数据长度
 * @return 成功返回发送的字节数，失败返回-1
 */
int websocket_send(websocket_client_t *client, ws_opcode_t opcode,
				   const void *data, size_t len);

/**
 * 接收WebSocket消息(阻塞方式)
 * @param client 客户端对象
 * @param buffer 接收缓冲区
 * @param max_len 缓冲区最大长度
 * @param opcode 接收消息的类型
 * @return 成功返回接收的字节数，失败返回-1
 */
int websocket_recv(websocket_client_t *client, void *buffer, size_t max_len,
				   ws_opcode_t *opcode);

/**
 * 关闭WebSocket连接
 * @param client 客户端对象
 */
void websocket_close(websocket_client_t *client);

/**
 * 检查连接是否有效
 * @param client 客户端对象
 * @return 连接有效返回true，否则返回false
 */
bool websocket_is_connected(websocket_client_t *client);

#endif
