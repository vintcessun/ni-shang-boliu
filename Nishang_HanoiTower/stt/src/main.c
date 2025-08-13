#define DBG_TAG "STT"
#include "audio.h"
#include "bflb_mtimer.h"
#include "es8388_driver.h"
#include "http.h"
#include "log.h"
#include "rtos.h"
#include "stt.h"
#include "websocket.h"

extern char route[20];
websocket_client_t client;
#define LAN_AUDIO_FETCH_CHUNK_SIZE 16000
static uint8_t *lan_audio_chunk_buffer;

bool STATE;

static void stt_send(void *p) {
	uint32_t fetched_data_len = 0;
	es8388_audio_start_capture();
	while (STATE) {
		TickType_t get_data_timeout = pdMS_TO_TICKS(50);
		es8388_audio_get_data(lan_audio_chunk_buffer,
							  LAN_AUDIO_FETCH_CHUNK_SIZE, &fetched_data_len,
							  get_data_timeout);
		if (fetched_data_len == 0) {
			LOG_I("没有音频信息跳过\n");
			continue;
		}
		int send_len = websocket_send(&client, WS_OPCODE_BINARY,
									  lan_audio_chunk_buffer, fetched_data_len);
		if (send_len == -2) {
			LOG_W("发送消息失败，需要重试\n");
		} else if (send_len <= 0) {
			LOG_E("发送消息失败\n");
		} else {
			LOG_I("发送消息长度: %d\n", send_len);
		}
		bflb_mtimer_delay_ms(50);
	}
	vTaskDelete(NULL);
}

#define BUFFER_SIZE 500

static void stt_receive(void *p) {
	uint8_t buffer[BUFFER_SIZE];
	while (1) {
		ws_opcode_t opcode;
		// LOG_I("等待接收消息...\n");

		int recv_len = websocket_recv(&client, buffer, BUFFER_SIZE, &opcode);
		if (recv_len > 0) {
			buffer[recv_len] = '\0';
			if (opcode == WS_OPCODE_TEXT) {
				LOG_I("收到文本消息长度: %s (长度: %d)\n", buffer, recv_len);
				audio_play(buffer);
			} else if (opcode == WS_OPCODE_BINARY) {
				LOG_I("收到二进制消息 (长度: %d)\n", recv_len);
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
	websocket_close(&client);
	STATE = false;

	vTaskDelete(NULL);
}

static void stt_task(void *p) {
	char url[500];
	sprintf(url, BASE_WS(stt) "?route=%s", route);

	// 初始化随机数生成器（用于掩码和密钥生成）
	srand(time(NULL));

	// 1. 初始化WebSocket客户端，解析ws://地址
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

	STATE = true;

	xTaskCreate(stt_receive, "STT_Recv", 2048, NULL, tskIDLE_PRIORITY + 1,
				NULL);
	xTaskCreate(stt_send, "STT_Send", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
	// while (1);

	vTaskDelete(NULL);
}

void stt_main() {
	audio_main();
	lan_audio_chunk_buffer =
		pvPortMallocStack(LAN_AUDIO_FETCH_CHUNK_SIZE * sizeof(uint8_t));
	if (lan_audio_chunk_buffer == NULL) {
		LOG_E("Failed to malloc lan_audio_chunk\n");
		return;
	}
	xTaskCreate(stt_task, "STT_Task", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
}