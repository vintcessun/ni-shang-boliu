#define DBG_TAG "IR"
#include "bflb_mtimer.h"
#include "game.h"
#include "ir.h"
#include "log.h"
#include "rtos.h"

uint8_t data[40] = {0};
bool init_data[32] = {0};

static void ir_task(void *arg) {
	LOG_I("Start to read data\n");
	while (1) {
		i2c_gpio_read_all(data);

		for (int i = 0; i < 32; i += 2) {
			init_data[i] = data[i];
		}
		send_data(init_data);
		bflb_mtimer_delay_ms(100);
	}

	vTaskDelete(NULL);
}

void ir_main() {
	// LOG_I("Start init IR\n");
	ir_i2c_init();
	LOG_I("IR init Finished\n");
	xTaskCreate(ir_task, "IR_Task", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
}