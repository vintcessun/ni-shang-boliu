#ifndef __SPI_H_
#define __SPI_H_

#define SPI_MASTER_MODE 0  // 1=主设备, 0=从设备

#define GPIO_SPI_SS GPIO_PIN_20
#define GPIO_SPI_SCLK GPIO_PIN_19
#define GPIO_SPI_MOSI GPIO_PIN_18
#define GPIO_SPI_MISO GPIO_PIN_17

#define GPIO_CFG GPIO_OUTPUT | GPIO_PULLUP | GPIO_DRV_1

#include "stdint.h"

int spi_main(void);
void receive_9_bytes(uint8_t *data);
void send_9_bytes(uint8_t *data);

#endif