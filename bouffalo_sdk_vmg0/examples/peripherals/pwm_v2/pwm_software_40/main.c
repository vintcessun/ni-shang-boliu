#include "bflb_clock.h"
#include "bflb_gpio.h"
#include "bflb_mtimer.h"
#include "board.h"
#include "i2c40.h"

int main(void) {
	board_init();
	i2c_gpio_init();

	// struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");
	// bflb_gpio_init(gpio, GPIO_PIN_24, GPIO_OUTPUT | GPIO_PULLUP |
	// GPIO_DRV_1);
	for (int i = 0; i < 100; i++) {
		set_i2c_gpio_bank_output(GPIO_BANK4, 0xf0);
		bflb_mtimer_delay_us(1036);
		set_i2c_gpio_bank_output(GPIO_BANK4, 0x00);
		bflb_mtimer_delay_us(3564);
	}
	bflb_mtimer_delay_ms(500);
	for (int i = 0; i < 100; i++) {
		set_i2c_gpio_bank_output(GPIO_BANK4, 0xf0);
		bflb_mtimer_delay_us(1400);
		set_i2c_gpio_bank_output(GPIO_BANK4, 0x00);
		bflb_mtimer_delay_us(3400);
	}

	for (int i = 0; i < 100; i++) {
		set_i2c_gpio_bank_output(GPIO_BANK4, 0x0f);
		bflb_mtimer_delay_us(1450);
		set_i2c_gpio_bank_output(GPIO_BANK4, 0x00);
		bflb_mtimer_delay_us(3350);
	}
	bflb_mtimer_delay_ms(500);
	for (int i = 0; i < 100; i++) {
		set_i2c_gpio_bank_output(GPIO_BANK4, 0x0f);
		bflb_mtimer_delay_us(985);
		set_i2c_gpio_bank_output(GPIO_BANK4, 0x00);
		bflb_mtimer_delay_us(3815);
	}
}
