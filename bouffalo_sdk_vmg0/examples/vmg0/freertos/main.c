#include <FreeRTOS.h>
#include "semphr.h"
#include "board.h"
#include "bflb_gpio.h"

#define DBG_TAG "MAIN"
#include "log.h"

static TaskHandle_t blink_handle;
struct bflb_device_s *gpio;
uint16_t delayTime = 1000;


SemaphoreHandle_t xSemaphore = NULL;

static void blink_task(void *pvParameters)
{
    uint16_t delay = 1000 ;
    while (1)
    {
        if (xSemaphoreTake(xSemaphore, 0) == pdTRUE)//获取二值信号量
        {
            delay = delayTime;
            LOG_I("update delay!\r\n");
            LOG_I("delay = %d\r\n",delay);
        }
        
        bflb_gpio_set(gpio, GPIO_PIN_32);
        vTaskDelay(delay);
        bflb_gpio_reset(gpio, GPIO_PIN_32);
        vTaskDelay(delay);
        LOG_I("blink LED\r\n");
    }
     vTaskDelete(NULL);
}

void gpio_isr(int irq, void *arg)
{
    static int i = 0;
    BaseType_t xHigherPriorityTaskWoken;
    bool intstatus = bflb_gpio_get_intstatus(gpio, GPIO_PIN_33);
    if (intstatus) {
        delayTime +=100;
        LOG_I("delayTime=%d\r\n", delayTime);
        xSemaphoreGiveFromISR(xSemaphore,&xHigherPriorityTaskWoken);	//释放二值信号量
        // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);//如果需要的话进行一次任务切换
        bflb_gpio_int_clear(gpio, GPIO_PIN_33);
    }
}

int main(void)
{
    board_init();


    gpio = bflb_device_get_by_name("gpio");
    bflb_gpio_init(gpio, GPIO_PIN_32, GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_0);

    bflb_gpio_int_init(gpio, GPIO_PIN_33,  GPIO_INPUT | GPIO_PULLUP | GPIO_INT_TRIG_MODE_SYNC_FALLING_EDGE);
    bflb_gpio_int_mask(gpio, GPIO_PIN_33, false);

    bflb_irq_attach(gpio->irq_num, gpio_isr, gpio);
    bflb_irq_enable(gpio->irq_num);

    configASSERT((configMAX_PRIORITIES > 4));

    vSemaphoreCreateBinary(xSemaphore);

    if (xSemaphore == NULL) {
        LOG_I("Create sem fail\r\n");
    }

    xTaskCreate(blink_task, (char *)"blink_task", 512, NULL, configMAX_PRIORITIES - 2, &blink_handle);

    vTaskStartScheduler();

    while (1) {
    }
}
