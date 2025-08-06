#include "bflb_gpio.h"
#include "board.h"

struct bflb_device_s *gpio;

int main(void)
{
    board_init();

    gpio = bflb_device_get_by_name("gpio");
    printf("gpio output\r\n");
    bflb_gpio_init(gpio, GPIO_PIN_32, GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_0);
    bflb_gpio_init(gpio, GPIO_PIN_31, GPIO_INPUT | GPIO_PULLDOWN | GPIO_SMT_EN | GPIO_DRV_0);

    while (1) {
        bflb_gpio_set(gpio, GPIO_PIN_32);
        printf("GPIO_PIN_31=%x\r\n", bflb_gpio_read(gpio, GPIO_PIN_31));
        bflb_mtimer_delay_ms(2000);

        bflb_gpio_reset(gpio, GPIO_PIN_32);
        printf("GPIO_PIN_31=%x\r\n", bflb_gpio_read(gpio, GPIO_PIN_31));
        bflb_mtimer_delay_ms(2000);
    }
}
