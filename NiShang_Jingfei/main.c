#include "bflb_mtimer.h"
#include "board.h"
#include "rtos.h"

#define DBG_TAG "MAIN"
#include "audio.h"
#include "auto_connect.h"
#include "hall.h"
#include "http.h"
#include "i2c40.h"
#include "lcd_task.h"
#include "log.h"
#include "pwm_sg.h"

int j[3] = {3, 0, 0};
int f[3] = {3, 0, 0};
int state;
bool lr;
bool gpio_data[8] = {0};
char route[20];
bool WIFI_CONNECTED;

int main(void) {
	board_init();

	i2c_gpio_init();
	// while (1) {
	// read_hall_sensor();
	// while (1);
	//}
	// sg_target_up();
	// bflb_mtimer_delay_ms(500);
	// sg_target_down();
	// while (1);

	audio_main();
	http_main();

	auto_connect_init("HONOR", "8888888888");

	vTaskStartScheduler();

	while (1) {
	}
}
