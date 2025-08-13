#ifndef __IR_I2C_H_
#define __IR_I2C_H_

#include "stdbool.h"
#include "stdint.h"

// 40路GPIO分组（5组×8路）
typedef enum {
	GPIO_BANK0 = 0,	 // 0-7
	GPIO_BANK1,		 // 8-15
	GPIO_BANK2,		 // 16-23
	GPIO_BANK3,		 // 24-31
	GPIO_BANK4,		 // 32-39
	GPIO_BANK_MAX
} gpio_bank_t;

void i2c_gpio_init(void);
int i2c_gpio_read_all(uint8_t states[40]);
bool set_i2c_gpio_bank_output(gpio_bank_t bank, uint8_t value);

#endif /* __IR_I2C_H_ */