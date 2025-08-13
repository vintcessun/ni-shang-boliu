#include "bflb_mtimer.h"
#include "i2c40.h"
#include "pwm_sg.h"
#define DBG_TAG "PWM"
#include "log.h"

typedef struct {
	uint8_t start_value;
	uint32_t start_delay_us;
	uint8_t end_value;
	uint32_t end_delay_us;
} SgData;

SgData sg1_data_up = {.start_value = 0xf0,
					  .start_delay_us = 760,
					  .end_value = 0x00,
					  .end_delay_us = 4040};
SgData sg1_data_down = {.start_value = 0xf0,
						.start_delay_us = 1600,
						.end_value = 0x00,
						.end_delay_us = 3100};
SgData sg2_data_down = {.start_value = 0x0f,
						.start_delay_us = 1515,
						.end_value = 0x00,
						.end_delay_us = 3285};
SgData sg2_data_up = {.start_value = 0x0f,
					  .start_delay_us = 886,
					  .end_value = 0x00,
					  .end_delay_us = 3914};

static inline void pwm_send(SgData *data) {
	for (int i = 0; i < 100; i++) {
		set_i2c_gpio_bank_output(GPIO_BANK4, data->start_value);
		bflb_mtimer_delay_us(data->start_delay_us);
		set_i2c_gpio_bank_output(GPIO_BANK4, data->end_value);
		bflb_mtimer_delay_us(data->end_delay_us);
	}
}

void sg_src_up() {
	LOG_I("Start to SRC UP\n");
	pwm_send(&sg1_data_up);
}

void sg_src_down() {
	LOG_I("Start to SRC DOWN\n");
	pwm_send(&sg1_data_down);
}

void sg_target_up() {
	LOG_I("Start to TGT UP\n");
	pwm_send(&sg2_data_up);
}

void sg_target_down() {
	LOG_I("Start to TGT DOWN\n");
	pwm_send(&sg2_data_down);
}