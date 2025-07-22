#include "bflb_mtimer.h"
#include "board.h"
#include "rtos.h"

#define DBG_TAG "MAIN"
#include "hall.h"
#include "http.h"
#include "lcd_task.h"
#include "log.h"

int j[3] = {3, 0, 0};
int f[3] = {3, 0, 0};
int state;
bool lr;
bool gpio_data[8] = {0};
char route[20];
bool WIFI_CONNECTED;

int main(void) {
	board_init();

	lcd_main();
	hall_main();
	http_main();

	vTaskStartScheduler();

	while (1) {
	}
}
