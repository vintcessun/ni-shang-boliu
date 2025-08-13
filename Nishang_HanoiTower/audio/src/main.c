#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "audio.h"
#include "base64.h"
#include "bflb_dma.h"
#include "bflb_gpio.h"
#include "bflb_i2c.h"
#include "bflb_i2s.h"
#include "bflb_l1c.h"
#include "bflb_mtimer.h"
#include "bl616_glb.h"
#include "board.h"
#include "bsp_es8388.h"
#include "es8388_driver.h"
#include "http.h"
#include "websocket.h"

#define DBG_TAG "AUDIO"
#include "log.h"

#define BUFFER_SIZE 4096

bool PLAY_STATE;

int audio_main(void) {
	es8388_audio_init();
	return 0;
}

void play_audio(char *encode_str) {
	char *url = (char *)malloc(strlen(encode_str) + 500);
	sprintf(url, BASE_WS(tts) "?data=%s&chunk_size=%d", encode_str,
			BUFFER_SIZE);

	// 初始化随机数生成器（用于掩码和密钥生成）
	srand(time(NULL));

	// 1. 初始化WebSocket客户端，解析ws://地址
	websocket_client_t client;
	if (!websocket_init(&client, url)) {
		LOG_I("初始化失败: %s\n", client.error_msg);
		return;
	}

	// 2. 连接到WebSocket服务器
	printf("连接到 %s:%d%s...\n", client.host, client.port, client.path);
	if (!websocket_connect(&client)) {
		printf("连接失败: %s\n", client.error_msg);
		return;
	}
	printf("连接成功！\n");

	// es8388_audio_start_capture();

	char *message = "START";

	LOG_I("开始发送信息 %s\n", message);
	int send_len =
		websocket_send(&client, WS_OPCODE_TEXT, message, strlen(message));
	if (send_len <= 0) {
		printf("发送消息失败\n");
		websocket_close(&client);
		return;
	}
	LOG_I("发送消息: %s (长度: %d)\n", message, send_len);

	uint8_t buffer[BUFFER_SIZE + 50];
	while (1) {
		// 4. 主动接收消息（阻塞方式）
		ws_opcode_t opcode;
		LOG_I("等待接收消息...\n");

		int recv_len = websocket_recv(&client, buffer, BUFFER_SIZE, &opcode);
		if (recv_len > 0) {
			buffer[recv_len] = '\0';
			if (opcode == WS_OPCODE_TEXT) {
				LOG_I("收到文本消息长度: %s (长度: %d)\n", buffer, recv_len);
				if (buffer[0] == 'E' && buffer[1] == 'N' && buffer[2] == 'D') {
					LOG_I("收到关闭连接请求\n");
					break;
				} else if (buffer[0] == 'S' && buffer[1] == 'T' &&
						   buffer[2] == 'A' && buffer[3] == 'R' &&
						   buffer[4] == 'T') {
					LOG_I("收到开始音频请求\n");
				}
			} else if (opcode == WS_OPCODE_BINARY) {
				LOG_I("收到二进制消息 (长度: %d)\n", recv_len);
				es8388_audio_play(buffer, BUFFER_SIZE);
				continue;
			} else if (opcode == WS_OPCODE_CLOSE) {
				LOG_I("收到关闭连接请求\n");
				break;
			}
		} else if (recv_len == -2) {
			LOG_I("接收缓冲区不足\n");
		} else {
			LOG_I("接收消息失败\n");
		}
	}

	// 5. 关闭连接
	websocket_close(&client);
	LOG_I("连接已关闭\n");

	// es8388_audio_stop_capture();
}
