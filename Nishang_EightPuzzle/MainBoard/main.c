#include "bflb_mtimer.h"
#include "board.h"

#define DBG_TAG "MAIN"
#include "auto_connect.h"
#include "http.h"
#include "log.h"
#include "rtos.h"
#include "spi.h"

char route[20];
bool WIFI_CONNECTED;

static void receive_task(void* p) {
	uint8_t data[9];
	char url[500];
	char response[500] = {0};
	while (1) {
		uint8_t tag[9] = {0};
		receive_9_bytes(data);
		LOG_I("Received data: ");
		for (int i = 0; i < 9; i++) {
			LOG_I("%02X ", data[i]);
		}
		LOG_I("\r\n");
		if (WIFI_CONNECTED) {
			sprintf(url,
					BASE_URL(cam) "?route=%s&data=%d,%d,%d,%d,%d,%d,%d,%d,%d",
					route, data[0], data[1], data[2], data[3], data[4], data[5],
					data[6], data[7], data[8]);
			http_get(url, response, 500);
		}
	}
	vTaskDelete(NULL);
}

int main(void) {
	board_init();

	http_main();
	spi_main();

	auto_connect_init("HONOR", "8888888888");

	xTaskCreate(receive_task, (char*)"RECEIVE", 1024, NULL,
				tskIDLE_PRIORITY + 1, NULL);

	vTaskStartScheduler();

	while (1) {
	}
}
