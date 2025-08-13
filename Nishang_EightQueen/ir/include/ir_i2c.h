#ifndef __IR_I2C_H_
#define __IR_I2C_H_

#include "stdint.h"

void ir_i2c_init(void);
int i2c_gpio_read_all(uint8_t *states);

#endif /* __IR_I2C_H_ */