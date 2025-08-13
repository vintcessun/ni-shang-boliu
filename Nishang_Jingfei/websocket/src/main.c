#include <base64.h>
#include <sha1.h>
#include <stdio.h>
#include <string.h>

#define DBG_TAG "WEBSOCKET"
#include "log.h"
#include "websocket.h"

#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define HTTP_RESPONSE_BUFFER_SIZE 1024

uint64_t ntohll(uint64_t n);

// 解析URL中的主机、端口和路径
static bool parse_url(const char *url, char *host, uint16_t *port,
					  char **path) {
	// 检查是否以ws://开头
	if (strncmp(url, "ws://", 5) != 0) {
		return false;
	}

	*path = NULL;  // 初始化为NULL

	const char *ptr = url + 5;
	const char *host_start = ptr;

	// 查找主机名结束位置(:或/)
	while (*ptr && *ptr != ':' && *ptr != '/') {
		ptr++;
	}

	// 提取主机名
	size_t host_len = ptr - host_start;
	if (host_len == 0 || host_len >= 64) {
		return false;
	}
	strncpy(host, host_start, host_len);
	host[host_len] = '\0';

	// 设置默认端口和路径
	*port = 80;
	*path = pvPortMalloc(2);  // 分配最小空间给"/"
	if (*path == NULL) {
		return false;
	}
	strcpy(*path, "/");

	// 处理端口
	if (*ptr == ':') {
		ptr++;
		const char *port_start = ptr;

		while (*ptr && *ptr != '/') {
			if (*ptr < '0' || *ptr > '9') {
				return false;  // 端口必须是数字
			}
			ptr++;
		}

		if (ptr > port_start) {
			char port_str[8];
			size_t port_len = ptr - port_start;
			strncpy(port_str, port_start, port_len);
			port_str[port_len] = '\0';
			*port = atoi(port_str);

			if (*port == 0 || *port > 65535) {
				return false;
			}
		}
	}

	// 处理路径
	if (*ptr == '/') {
		const char *path_start = ptr;
		ptr++;	// 跳过 '/'

		while (*ptr && *ptr != ' ') {
			ptr++;
		}

		size_t path_len = ptr - path_start;
		if (path_len > 0) {
			// 释放之前分配的默认路径
			if (*path) {
				free(*path);
			}
			// 分配足够空间存储路径(包括null终止符)
			*path = pvPortMalloc(path_len + 1);
			if (*path == NULL) {
				return false;
			}
			strncpy(*path, path_start, path_len);
			(*path)[path_len] = '\0';
		}
	}

	return true;
}

// 生成随机的WebSocket密钥
static void generate_websocket_key(char *key) {
	uint8_t random_bytes[16];
	int i;

	// 生成16字节随机数据
	for (i = 0; i < 16; i++) {
		random_bytes[i] = (uint8_t)(rand() % 256);
	}

	// 进行Base64编码
	base64_encode(random_bytes, 16, key, 256);
}

// 验证服务器握手响应
static bool verify_handshake_response(const char *response,
									  const char *client_key) {
	// 检查是否包含101状态码
	if (strstr(response, "HTTP/1.1 101 Switching Protocols") == NULL) {
		return false;
	}

	// 检查Upgrade头
	if (strstr(response, "upgrade: websocket") == NULL) {
		return false;
	}

	// 检查Connection头
	if (strstr(response, "connection: upgrade") == NULL) {
		return false;
	}

	// 提取服务器Accept密钥
	const char *accept_start = strstr(response, "sec-websocket-accept: ");
	if (!accept_start) {
		return false;
	}
	accept_start += 22;	 // 跳过"sec-websocket-accept: "

	char server_accept[256] = {0};
	const char *accept_end = strstr(accept_start, "\r\n");
	if (accept_end) {
		strncpy(server_accept, accept_start, accept_end - accept_start);
	} else {
		return false;
	}

	// 计算期望的Accept密钥
	char expected_accept[256] = {0};
	char temp[256];
	uint8_t sha1_hash[20];

	strcpy(temp, client_key);
	strcat(temp, GUID);
	sha1((uint8_t *)temp, strlen(temp), sha1_hash);
	base64_encode(sha1_hash, 20, expected_accept, 256);

	// 比较计算结果和服务器返回的结果
	return strcmp(server_accept, expected_accept) == 0;
}

bool websocket_init(websocket_client_t *client, const char *url) {
	if (!client || !url) {
		return false;
	}

	// 初始化客户端状态
	memset(client, 0, sizeof(websocket_client_t));
	client->state = WS_STATE_DISCONNECTED;

	// 解析URL
	if (!parse_url(url, client->host, &client->port, &client->path)) {
		strcpy(client->error_msg, "Invalid URL format");
		client->state = WS_STATE_ERROR;
		return false;
	}

	return true;
}

bool websocket_connect(websocket_client_t *client) {
	if (!client || client->state != WS_STATE_DISCONNECTED) {
		return false;
	}

	client->state = WS_STATE_CONNECTING;

	// 创建TCP socket
	// LOG_I("创建 TCP socket\n");
	client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client->socket_fd < 0) {
		strcpy(client->error_msg, "Failed to create socket");
		client->state = WS_STATE_ERROR;
		return false;
	}

	// 解析主机名到IP地址
	// LOG_I("解析主机名到IP地址\n");
	struct hostent *host_entry = gethostbyname(client->host);
	if (!host_entry) {
		strcpy(client->error_msg, "Failed to resolve host");
		closesocket(client->socket_fd);
		client->socket_fd = -1;
		client->state = WS_STATE_ERROR;
		return false;
	}

	// 设置服务器地址
	// LOG_I("设置服务器地址\n");
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(client->port);
	memcpy(&server_addr.sin_addr.s_addr, host_entry->h_addr,
		   host_entry->h_length);

	char host_header[256];
	if (client->port == 80) {
		sprintf(host_header, "Host: %s\r\n", client->host);
	} else {
		sprintf(host_header, "Host: %s:%d\r\n", client->host, client->port);
	}

	// 连接到服务器
	// LOG_I("连接到服务器\n");
	if (connect(client->socket_fd, (struct sockaddr *)&server_addr,
				sizeof(server_addr)) < 0) {
		sprintf(client->error_msg, "Failed to connect to %s:%d", client->host,
				client->port);
		closesocket(client->socket_fd);
		client->socket_fd = -1;
		client->state = WS_STATE_ERROR;
		return false;
	}

	// 生成客户端密钥
	// LOG_I("生成客户端密钥\n");
	char client_key[256];
	generate_websocket_key(client_key);

	// 构建握手请求
	// LOG_I("构建握手请求\n");
	char *handshake_request =
		(char *)pvPortMallocStack(strlen(client->path) + 600);
	sprintf(handshake_request,
			"GET %s HTTP/1.1\r\n"
			"%s"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: %s\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"\r\n",
			client->path, host_header, client_key);

	// 发送握手请求
	// LOG_I("发送握手请求\n");
	if (send(client->socket_fd, handshake_request, strlen(handshake_request),
			 0) <= 0) {
		strcpy(client->error_msg, "Failed to send handshake");
		closesocket(client->socket_fd);
		client->socket_fd = -1;
		client->state = WS_STATE_ERROR;
		return false;
	}

	vPortFreeStack(handshake_request);

	// 接收服务器响应
	// LOG_I("接收服务器响应\n");
	char response[HTTP_RESPONSE_BUFFER_SIZE];
	int bytes_read =
		recv(client->socket_fd, response, HTTP_RESPONSE_BUFFER_SIZE - 1, 0);
	if (bytes_read <= 0) {
		strcpy(client->error_msg, "Failed to receive handshake response");
		closesocket(client->socket_fd);
		client->socket_fd = -1;
		client->state = WS_STATE_ERROR;
		return false;
	}
	response[bytes_read] = '\0';

	// 验证握手响应
	// LOG_I("验证握手响应\n");
	if (!verify_handshake_response(response, client_key)) {
		strcpy(client->error_msg, "Handshake verification failed");
		closesocket(client->socket_fd);
		client->socket_fd = -1;
		client->state = WS_STATE_ERROR;
		return false;
	}

	// 握手成功，标记为已连接
	// LOG_I("握手成功，标记为已连接\n");
	client->state = WS_STATE_CONNECTED;
	return true;
}

int websocket_send(websocket_client_t *client, ws_opcode_t opcode,
				   const void *data, size_t len) {
	if (!client || !websocket_is_connected(client)) {
		LOG_E("连接已断开\n");
		return -1;
	}

	if (!data || len == 0) {
		LOG_E("没有要发送的数据\n");
		return -1;
	}

	uint8_t header[14];	 // 最大头部长度
	int header_len = 2;

	// 构建帧头部
	header[0] = (1 << 7) | (opcode & 0x0F);	 // FIN=1, 操作码

	// 设置payload长度
	if (len <= 125) {
		header[1] = len;
	} else if (len <= 65535) {
		header[1] = 126;
		header[2] = (len >> 8) & 0xFF;
		header[3] = len & 0xFF;
		header_len = 4;
	} else {
		header[1] = 127;
		for (int i = 0; i < 8; i++) {
			header[2 + i] = (len >> (8 * (7 - i))) & 0xFF;
		}
		header_len = 10;
	}

	// 客户端发送的帧需要掩码
	header[1] |= (1 << 7);

	// 生成随机掩码
	uint8_t mask[4];
	for (int i = 0; i < 4; i++) {
		mask[i] = (uint8_t)(rand() % 256);
	}
	memcpy(&header[header_len], mask, 4);
	header_len += 4;

	// 发送头部（必须确保发送成功）
	if (send(client->socket_fd, header, header_len, 0) != header_len) {
		LOG_E("发送头部失败\n");
		return -1;
	}

	// 应用掩码并发送数据（非阻塞方式）
	uint8_t *masked_data = (uint8_t *)pvPortMallocStack(len * sizeof(uint8_t));
	if (!masked_data) {
		LOG_E("分配masked_data失败\n");
		return -1;
	}

	for (size_t i = 0; i < len; i++) {
		masked_data[i] = ((uint8_t *)data)[i] ^ mask[i % 4];
	}

	// 阻塞发送
	int bytes_sent = send(client->socket_fd, masked_data, len, 0);

	vPortFreeStack(masked_data);

	// 返回-1表示需要重试，其他情况返回实际发送的字节数
	if (bytes_sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		return -2;	// 特殊返回值表示需要重试
	}
	return bytes_sent;
}

int websocket_recv(websocket_client_t *client, void *buffer, size_t max_len,
				   ws_opcode_t *opcode) {
	if (!client || !websocket_is_connected(client) || !buffer || max_len == 0 ||
		!opcode) {
		LOG_E("连接已关闭\n");
		return -1;
	}

	uint8_t first_byte, second_byte;
	size_t payload_len = 0;
	uint8_t mask[4] = {0};
	int bytes_read;
	bool fin_flag;

	// 读取第一个字节（FIN和操作码）
	bytes_read = recv(client->socket_fd, &first_byte, 1, 0);
	if (bytes_read != 1) {
		LOG_E("读取FIN失败\n");
		return -1;
	}

	fin_flag = (first_byte & 0x80) != 0;
	*opcode = first_byte & 0x0F;

	// 检查操作码是否有效
	if (*opcode != WS_OPCODE_TEXT && *opcode != WS_OPCODE_BINARY) {
		LOG_E("不支持的消息格式\n");
		return -1;	// 仅支持文本和二进制帧
	}

	// 读取第二个字节（掩码和 payload长度）
	bytes_read = recv(client->socket_fd, &second_byte, 1, 0);
	if (bytes_read != 1) {
		LOG_E("掩码和payload长度读取失败\n");
		return -1;
	}

	bool masked = (second_byte & 0x80) != 0;
	uint8_t len_field = second_byte & 0x7F;

	// 解析 payload长度
	if (len_field == 126) {
		uint16_t extended_len;
		if (recv(client->socket_fd, &extended_len, 2, 0) != 2) {
			LOG_E("继续读取短payload长度失败\n");
			return -1;
		}
		payload_len = ntohs(extended_len);
	} else if (len_field == 127) {
		uint64_t extended_len;
		if (recv(client->socket_fd, &extended_len, 8, 0) != 8) {
			LOG_E("继续读取长payload长度失败\n");
			return -1;
		}
		payload_len = ntohll(extended_len);
	} else {
		payload_len = len_field;
	}

	// 检查缓冲区是否足够
	if (payload_len > max_len) {
		LOG_E("缓冲区不足: 需要 %d 字节, 只有 %d 字节\n", payload_len, max_len);
		return -2;
	}

	// 读取掩码（如果有）
	if (masked) {
		if (recv(client->socket_fd, mask, 4, 0) != 4) {
			LOG_E("读取掩码失败\n");
			return -1;
		}
	}

	// 检查FIN标志，确保是完整帧
	if (!fin_flag) {
		LOG_E("分帧消息不支持\n");
		return -1;	// 不支持分帧消息
	}

	// 读取帧数据
	bytes_read = recv(client->socket_fd, buffer, payload_len, 0);
	if (bytes_read != (int)payload_len) {
		LOG_E("读取帧数据异常\n");
		// 读取并丢弃剩余数据
		size_t remaining = payload_len - bytes_read;
		while (remaining > 0) {
			char dummy[1024];
			size_t to_read =
				remaining > sizeof(dummy) ? sizeof(dummy) : remaining;
			int n = recv(client->socket_fd, dummy, to_read, 0);
			if (n <= 0) break;
			remaining -= n;
		}
		return -1;
	}

	// 应用掩码解码
	if (masked) {
		for (size_t i = 0; i < payload_len; i++) {
			((uint8_t *)buffer)[i] ^= mask[i % 4];
		}
	}

	return payload_len;
}

void websocket_close(websocket_client_t *client) {
	if (!client) {
		return;
	}

	if (client->socket_fd != -1) {
		// 发送关闭帧
		if (client->state == WS_STATE_CONNECTED) {
			websocket_send(client, WS_OPCODE_CLOSE, NULL, 0);
		}
		closesocket(client->socket_fd);
		client->socket_fd = -1;
	}

	// 释放动态分配的path内存
	if (client->path) {
		vPortFree(client->path);
		client->path = NULL;
	}

	client->state = WS_STATE_DISCONNECTED;
	client->error_msg[0] = '\0';
}

bool websocket_is_connected(websocket_client_t *client) {
	return client && client->state == WS_STATE_CONNECTED &&
		   client->socket_fd != -1;
}

// 辅助函数：64位网络字节序转主机字节序
uint64_t ntohll(uint64_t n) {
#if BYTE_ORDER == LITTLE_ENDIAN
	return (uint64_t)ntohl((uint32_t)(n & 0xFFFFFFFF)) << 32 |
		   ntohl((uint32_t)(n >> 32));
#else
	return n;
#endif
}
