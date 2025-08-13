#include "bflb_clock.h"
#include "bflb_gpio.h"
#include "bflb_mtimer.h"
#include "board.h"

int main(void) {
	board_init();
	struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");
	bflb_gpio_init(gpio, GPIO_PIN_24, GPIO_OUTPUT | GPIO_PULLUP | GPIO_DRV_1);
	for (int i = 0; i < 150; i++) {
		bflb_gpio_set(gpio, GPIO_PIN_24);
		bflb_mtimer_delay_us(1700);
		bflb_gpio_reset(gpio, GPIO_PIN_24);
		bflb_mtimer_delay_us(3300);
	}
	bflb_mtimer_delay_ms(500);
	for (int i = 0; i < 150; i++) {
		bflb_gpio_set(gpio, GPIO_PIN_24);
		bflb_mtimer_delay_us(1300);
		bflb_gpio_reset(gpio, GPIO_PIN_24);
		bflb_mtimer_delay_us(3700);
	}
	while (1) {
	}
}
