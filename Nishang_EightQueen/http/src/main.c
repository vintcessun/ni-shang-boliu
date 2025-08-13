#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>

#include "FreeRTOS.h"
#include "bflb_irq.h"
#include "bflb_uart.h"
#include "bl616_glb.h"
#include "bl_fw_api.h"
#include "board.h"
#include "mem.h"
#include "rfparam_adapter.h"
#include "rtos.h"
#include "task.h"
#include "timers.h"
#include "wifi_mgmr.h"
#include "wifi_mgmr_ext.h"

#define DBG_TAG "HTTP"
#include "auto_connect.h"
#include "http.h"
#include "log.h"

extern struct bflb_device_s *gpio;
extern bool WIFI_CONNECTED;

#define WIFI_STACK_SIZE (4096)
#define TASK_PRIORITY_FW (tskIDLE_PRIORITY + 16)

extern char route[20];

static TaskHandle_t wifi_fw_task;

static wifi_conf_t conf = {
	.country_code = "CN",
};

int wifi_start_firmware_task(void) {
	LOG_I("[WIFI] Starting wifi\n");

	/* enable wifi clock */

	GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY |
						 GLB_AHB_CLOCK_IP_WIFI_MAC_PHY |
						 GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
	GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);

	/* Enable wifi irq */

	extern void interrupt0_handler(void);
	bflb_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
	bflb_irq_enable(WIFI_IRQn);

	xTaskCreate(wifi_main, (char *)"fw", WIFI_STACK_SIZE, NULL,
				TASK_PRIORITY_FW, &wifi_fw_task);

	return 0;
}

static void wifi_connected_task(void *p) {
	while (strlen(route) < 16) {
		char response[500] = {0};
		int result = http_get(BASE_URL(start?data=eight_queen), response, 500);
		for (int i = 0; i < 16; i++) {
			route[i] = response[i];
		}
		route[16] = 0;
		bflb_mtimer_delay_ms(1000);
	}

	es8388_audio_start_capture();
	stt_main();
	WIFI_CONNECTED = true;
	//*/

	vTaskDelete(NULL);
}

void wifi_event_handler(uint32_t code) {
	switch (code) {
		case CODE_WIFI_ON_INIT_DONE: {
			LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_INIT_DONE\n", __func__);
			wifi_mgmr_init(&conf);
		} break;
		case CODE_WIFI_ON_MGMR_DONE: {
			LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_MGMR_DONE\n", __func__);
			auto_connect_signal_wifi_ready();
		} break;
		case CODE_WIFI_ON_SCAN_DONE: {
			LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_SCAN_DONE\n", __func__);
			wifi_mgmr_sta_scanlist();
		} break;
		case CODE_WIFI_ON_CONNECTED: {
			LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_CONNECTED\n", __func__);
			void mm_sec_keydump();
			mm_sec_keydump();
		} break;
		case CODE_WIFI_ON_GOT_IP: {
			LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_GOT_IP\n", __func__);
			xTaskCreate(wifi_connected_task, (char *)"WIFI_CONNECTED_TASK",
						1024, NULL, tskIDLE_PRIORITY + 1, NULL);
		} break;
		case CODE_WIFI_ON_DISCONNECT: {
			LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_DISCONNECT\n", __func__);
			WIFI_CONNECTED = false;
		} break;
		case CODE_WIFI_ON_AP_STARTED: {
			LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STARTED\n", __func__);
		} break;
		case CODE_WIFI_ON_AP_STOPPED: {
			LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STOPPED\n", __func__);
		} break;
		case CODE_WIFI_ON_AP_STA_ADD: {
			LOG_I("[APP] [EVT] [AP] [ADD] %lld\n", xTaskGetTickCount());
		} break;
		case CODE_WIFI_ON_AP_STA_DEL: {
			LOG_I("[APP] [EVT] [AP] [DEL] %lld\n", xTaskGetTickCount());
		} break;
		default: {
			LOG_I("[APP] [EVT] Unknown code %u\n", code);
		}
	}
}

int http_get(const char *url, uint8_t *response, size_t max_len) {
	LOG_I("[HTTP] Starting request to: %s\n", url);

	int len = strlen(url);
	char host[128], *path = (char *)pvPortMalloc(len);
	int port = 80;
	char *p1, *p2;
	struct hostent *server;
	struct sockaddr_in serv_addr;
	int sockfd, n;
	char *request = (char *)pvPortMalloc(len + 500);

	// 解析URL，假设格式为 http://host/path
	if (strncmp(url, "http://", 7) == 0) {
		p1 = (char *)url + 7;
		// LOG_I("[HTTP] URL protocol: HTTP\n");
	} else {
		p1 = (char *)url;
		// LOG_I("[HTTP] URL protocol: Unknown\n");
	}
	p2 = strchr(p1, '/');
	if (p2) {
		strncpy(host, p1, p2 - p1);
		host[p2 - p1] = '\0';
		strncpy(path, p2, len - 1);
		path[len - 1] = '\0';
	} else {
		strncpy(host, p1, sizeof(host) - 1);
		host[sizeof(host) - 1] = '\0';
		strcpy(path, "/");
	}

	// DNS 解析
	// LOG_I("[HTTP] Resolving host: %s\n", host);
	server = gethostbyname(host);
	if (server == NULL) {
		LOG_E("[HTTP] ERROR: Failed to resolve host %s\n", host);
		return -1;
	}
	// LOG_I("[HTTP] Host resolved successfully: %s -> %s\n", host,
	//	  inet_ntoa(*(struct in_addr *)server->h_addr));
	//  创建 socket
	// LOG_I("[HTTP] Creating socket...\n");
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		LOG_E("[HTTP] ERROR: Failed to create socket (errno: %d)\n", errno);
		return -1;
	}
	// LOG_I("[HTTP] Socket created successfully (fd: %d)\n", sockfd);

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	// 连接服务器
	// LOG_I("[HTTP] Connecting to %s:%d...\n", inet_ntoa(serv_addr.sin_addr),
	//	  ntohs(serv_addr.sin_port));
	int connect_result =
		connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (connect_result < 0) {
		LOG_E("[HTTP] ERROR: Connection failed (errno: %d)\n", errno);
		close(sockfd);
		return -1;
	}
	// LOG_I("[HTTP] Connected successfully to %s:%d\n",
	//	  inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));

	// 发送HTTP GET请求
	// LOG_I("[HTTP] Preparing GET request...\n");
	sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
	// LOG_I("[HTTP] Request content:\n%s\n", request);
	int bytes_sent = write(sockfd, request, strlen(request));
	// LOG_I("[HTTP] Sent %d bytes of request data\n", bytes_sent);
	if (bytes_sent < 0) {
		LOG_E("[HTTP] ERROR: Failed to send request (errno: %d)\n", errno);
		close(sockfd);
		return -1;
	}

	vPortFree(path);
	vPortFree(request);

	// 接收响应
	// LOG_I("[HTTP] Starting to receive response...\n");
	size_t total = 0;
	uint8_t *body_start = NULL;

	// 先接收完整响应(包含头部和内容)
	while ((n = read(sockfd, response + total, max_len - total - 1)) > 0) {
		total += n;
		if (total >= max_len - 1) {
			// LOG_I("[HTTP] WARNING: Response buffer full (%d bytes)\n",
			// max_len);
			break;
		}
	}
	response[total] = '\0';

	// 打印完整响应头
	// LOG_I("[HTTP] Full response headers:\n");
	body_start = (uint8_t *)strstr((char *)response, "\r\n\r\n");
	if (body_start) {
		*body_start = '\0';	 // 临时截断以便打印头部
		// LOG_I("%s\n", response);
		*body_start = '\r';	 // 恢复原始数据
		body_start += 4;	 // 跳过空行
	} else {
		// LOG_I("[HTTP] WARNING: No header/body separator found\n");
		body_start = response;
	}

	// 打印响应内容
	// LOG_I("[HTTP] Response body length: %d bytes\n",
	//	  (int)(total - (body_start - response)));
	// LOG_I("[HTTP] Response body (first 200 bytes):\n%.200s\n", body_start);

	// 将body内容移动到response起始位置
	if (body_start != response) {
		memmove(response, body_start, total - (body_start - response) + 1);
		total = total - (body_start - response);
	}

	close(sockfd);
	// LOG_I("[HTTP] Socket closed\n");
	// LOG_I("[HTTP] Request completed with status: %s\n",
	//	  total > 0 ? "SUCCESS" : "FAILED");
	return total > 0 ? 0 : -1;
}

int http_main(void) {
	if (0 != rfparam_init(0, NULL, 0)) {
		LOG_I("[HTTP] PHY RF init failed\n");
		return 0;
	}

	LOG_I("[HTTP] PHY RF init success\n");

	tcpip_init(NULL, NULL);

	wifi_start_firmware_task();

	return 0;
}
