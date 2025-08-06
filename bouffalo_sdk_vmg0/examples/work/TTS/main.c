#include "bflb_mtimer.h"
#include "board.h"

#define DBG_TAG "MAIN"
#include "auto_connect.h"
#include "http.h"
#include "log.h"

char route[20];
bool WIFI_CONNECTED;

int main(void) {
	board_init();

	http_main();
	audio_main();

	auto_connect_init("HONOR", "8888888888");

	vTaskStartScheduler();

	while (1) {
	}
}
