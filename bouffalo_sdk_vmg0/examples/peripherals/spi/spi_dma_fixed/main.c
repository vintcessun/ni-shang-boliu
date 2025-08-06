#include "board.h"
#include "spi.h"

int main(void) {
	board_init();
	spi_main();

#if SPI_MASTER_MODE
	uint8_t tx_buf[9] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
	send_9_bytes(tx_buf);

#else
	uint8_t rx_buf[9] = {0};
	uint8_t start_byte;
	while (1) {
		receive_9_bytes(rx_buf);
	}
#endif

	while (1) {
		bflb_mtimer_delay_ms(1000);
	}
}
