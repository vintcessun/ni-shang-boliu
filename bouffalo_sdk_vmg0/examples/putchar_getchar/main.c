#include "bflb_mtimer.h"
#include "bflb_uart.h"
#include "board.h"

#define DBG_TAG "MAIN"
#include "log.h"

int main(void) {
	board_init();

	struct bflb_device_s *uart0;
	struct bflb_device_s *uart1;
	uart0 = bflb_device_get_by_name("uart0");
	uart1 = bflb_device_get_by_name("uart1");
	struct bflb_uart_config_s cfg;
	cfg.baudrate = 2000000;
	cfg.data_bits = UART_DATA_BITS_8;
	cfg.stop_bits = UART_STOP_BITS_1;
	cfg.parity = UART_PARITY_NONE;
	cfg.flow_ctrl = 0;
	cfg.tx_fifo_threshold = 15;
	cfg.rx_fifo_threshold = 15;
	bflb_uart_init(uart1, &cfg);
	board_uartx_gpio_init();

	while (1) {
		int ch = bflb_uart_getchar(uart1);
		if (ch >= 0 && ch != 0xff) bflb_uart_putchar(uart0, ch);
	}
}
