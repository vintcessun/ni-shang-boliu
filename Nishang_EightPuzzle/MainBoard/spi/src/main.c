#include "bflb_dma.h"
#include "bflb_gpio.h"
#include "bflb_mtimer.h"
#include "bflb_spi.h"
#include "board.h"
#include "spi.h"
#define DBG_TAG "SPI"
#include "log.h"

struct bflb_device_s *gpio;

static inline void write_gpio(bool value, uint8_t pin) {
	if (value) {
		bflb_gpio_set(gpio, pin);
	} else {
		bflb_gpio_reset(gpio, pin);
	}
}

static inline bool read_gpio(uint8_t pin) {
	bflb_gpio_deinit(gpio, pin);
	bool ret = bflb_gpio_read(gpio, pin);
	bflb_gpio_init(gpio, pin, GPIO_CFG);
	return ret;
}

static inline void SPI_SS_Write(bool value) { write_gpio(value, GPIO_SPI_SS); }
static inline void SPI_SCLK_Write(bool value) {
	write_gpio(value, GPIO_SPI_SCLK);
}
static inline void SPI_MOSI_Write(bool value) {
	write_gpio(value, GPIO_SPI_MOSI);
}
static inline void SPI_MISO_Write(bool value) {
	write_gpio(value, GPIO_SPI_MISO);
}

static inline bool SPI_SS_Read(void) { return read_gpio(GPIO_SPI_SS); }
static inline bool SPI_SCLK_Read(void) { return read_gpio(GPIO_SPI_SCLK); }
static inline bool SPI_MISO_Read(void) { return read_gpio(GPIO_SPI_MISO); }
static inline bool SPI_MOSI_Read(void) { return read_gpio(GPIO_SPI_MOSI); }

static inline void SPI_Start(void) {
	SPI_SCLK_Write(1);	// 拉高开始输出第一个位
	SPI_SS_Write(0);	// 拉低片选
}

static inline void SPI_Stop(void) {
	SPI_SCLK_Write(0);	// 确保时钟线低电平
	SPI_SS_Write(1);	// 拉高片选
}

static inline uint8_t MasterExchangeBit(uint8_t data) {
	uint8_t received = 0;
	SPI_Start();
	for (int i = 0; i < 8; i++) {
		SPI_SCLK_Write(1);					 // 上升沿
		SPI_MOSI_Write(data & (0x80 >> i));	 // 发送数据位
		bflb_mtimer_delay_us(88);
		SPI_SCLK_Write(0);	// 下降沿
		bflb_mtimer_delay_us(88);
		if (SPI_MISO_Read()) received |= (0x80 >> i);  // 读取数据位
	}
	SPI_Stop();

	return received;
}

static inline void SlavePassABit() {
	uint8_t received = 0;
	int flag = 0;
	int i = 0;
	while (SPI_SCLK_Read() == 0) {
	}

	while (i < 1) {
		if (SPI_SCLK_Read() == 1) {
			flag = 1;
			SPI_MISO_Write(0);
			if (SPI_MOSI_Read() == 1) {
				received |= 0x01;
			} else {
				received |= 0x00;
			}

		} else {
			if (flag) {
				i++;
				flag = 0;
				received <<= 1;
			}
		}
	}
}

static inline uint8_t SlaveExchangeBit(uint8_t data) {
	uint8_t received = 0;
	int flag = 0;
	int i = 0;

	while (SPI_SCLK_Read() == 0) {
	}
	while (i < 8) {
		if (SPI_SCLK_Read() == 1) {
			flag = 1;
			SPI_MISO_Write(data & (0x80 >> i));
			if (SPI_MOSI_Read() == 1) {
				received |= 0x01;
			} else {
				received |= 0x00;
			}

		} else {
			if (flag) {
				i++;
				flag = 0;
				received <<= 1;
			}
		}
	}
	received >>= 1;
	return received;
}

static inline uint8_t SlaveReceiveByte() { return SlaveExchangeBit(0x7f); }

static inline void spi_gpio_init(void) {
	gpio = bflb_device_get_by_name("gpio");
	bflb_gpio_init(gpio, GPIO_SPI_SS, GPIO_CFG);
	bflb_gpio_init(gpio, GPIO_SPI_SCLK, GPIO_CFG);
	bflb_gpio_init(gpio, GPIO_SPI_MOSI, GPIO_CFG);
	bflb_gpio_init(gpio, GPIO_SPI_MISO, GPIO_CFG);
	bflb_gpio_set(gpio, GPIO_SPI_SS);
	bflb_gpio_reset(gpio, GPIO_SPI_SCLK);
}

void send_9_bytes(uint8_t *data) {
	uint8_t tx_buf[10] = {0};
	for (int i = 0; i < 9; i++) {
		tx_buf[i] = data[i];
		tx_buf[9] += data[i];
	}
	for (int k = 0; k < 3; k++) {
		MasterExchangeBit(0x7f);
		LOG_I("Master sent start byte: 0x7F\r\n");

		for (int i = 0; i < 10; i++) {
			MasterExchangeBit(tx_buf[i]);
		}
		LOG_I(
			"Master sent: %02X %02X %02X %02X %02X %02X %02X %02X %02X "
			"%02X\r\n",
			tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3], tx_buf[4], tx_buf[5],
			tx_buf[6], tx_buf[7], tx_buf[8], tx_buf[9]);
		bflb_mtimer_delay_ms(300);
	}
}

void receive_9_bytes(uint8_t *data) {
	uint8_t rx_buf[10] = {0};
	uint8_t start_byte;
	while (1) {
		while (SPI_SS_Read()) {
		}

		if (!SPI_SS_Read()) {
			start_byte = SlaveReceiveByte();

			while (start_byte != 0x7f) {
				SlavePassABit();
				start_byte = SlaveReceiveByte();
				LOG_I("Start byte received: %02X\r\n", start_byte);
			}

			for (int i = 0; i < 10; i++) {
				rx_buf[i] = SlaveReceiveByte();	 // 从设备接收数据
			}

			uint8_t check = 0;
			for (int i = 0; i < 9; i++) {
				check += rx_buf[i];
			}

			LOG_I(
				"Slave received: %02X %02X %02X %02X %02X %02X %02X %02X "
				"%02X %02X Valid State: %d\r\n",
				rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3], rx_buf[4],
				rx_buf[5], rx_buf[6], rx_buf[7], rx_buf[8], rx_buf[9],
				check == rx_buf[9]);
			if (check == rx_buf[9]) {
				break;
			}
		}
	}
	memcpy(data, rx_buf, 9);
}

int spi_main(void) {
	spi_gpio_init();

#if SPI_MASTER_MODE
	LOG_I("SPI Master Mode\r\n");
#else
	LOG_I("SPI Slave Mode\r\n");
#endif

#if SPI_MASTER_MODE
	uint8_t tx_buf[10] = {0x01, 0x02, 0x03, 0x04, 0x05,
						  0x06, 0x07, 0x08, 0x09, 0x2d};

	for (int i = 0; i < 3; i++) {
		MasterExchangeBit(0x7f);
		LOG_I("Master sent start byte: 0x7F\r\n");

		for (int i = 0; i < 10; i++) {
			MasterExchangeBit(tx_buf[i]);
		}

		LOG_I(
			"Master sent: %02X %02X %02X %02X %02X %02X %02X %02X %02X "
			"%02X\r\n",
			tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3], tx_buf[4], tx_buf[5],
			tx_buf[6], tx_buf[7], tx_buf[8], tx_buf[9]);
	}

#else
	uint8_t rx_buf[10] = {0};
	uint8_t start_byte;
	bool state = true;
	while (state) {
		while (SPI_SS_Read());

		if (!SPI_SS_Read()) {
			start_byte = SlaveReceiveByte();

			while (start_byte != 0x7f) {
				SlavePassABit();
				start_byte = SlaveReceiveByte();
				printf("Start byte received: %02X\r\n", start_byte);
			}

			for (int i = 0; i < 10; i++) {
				rx_buf[i] = SlaveReceiveByte();	 // 从设备接收数据
			}

			uint8_t check = 0;
			for (int i = 0; i < 9; i++) {
				check += rx_buf[i];
			}

			printf(
				"Slave received: %02X %02X %02X %02X %02X %02X %02X %02X "
				"%02X %02X Valid State: %d\r\n",
				rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3], rx_buf[4],
				rx_buf[5], rx_buf[6], rx_buf[7], rx_buf[8], rx_buf[9],
				check == rx_buf[9]);
			state = !(check == rx_buf[9]);	// 如果校验和正确，退出初始化
		}
	}
#endif
}
