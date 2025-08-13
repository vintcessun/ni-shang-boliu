#include "bflb_gpio.h"
#include "bflb_i2c.h"
#include "bflb_mtimer.h"
#include "board.h"

// 思修电子工作室数据类型定义
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef bool bit;

// 硬件配置（根据BL616G0板子实际布线修改）
#define GPIO_EXP_ADDR 0x40	// PCA9555默认地址

// 寄存器定义（遵循芯片手册）
#define REG_INPUT_BASE 0x00	  // 输入寄存器起始地址
#define REG_CONFIG_BASE 0x06  // 配置寄存器起始地址
#define REG_OUTPUT_BASE 0x02  // 输出寄存器起始地址

// 错误码定义（与BL616错误处理风格一致）
#define ETIMEDOUT 116
#define EINVAL 22

// 16路GPIO分组（2组×8路）
typedef enum {
	GPIO_BANK0 = 0,	 // 0-7
	GPIO_BANK1,		 // 8-15
	GPIO_BANK_MAX
} gpio_bank_t;

static struct bflb_device_s *i2c0;
static struct bflb_device_s *gpio;

// 思修电子工作室I2C模拟实现
#define SDA_PIN GPIO_PIN_15	 // 用户确认SDA=15
#define SCL_PIN GPIO_PIN_14	 // 用户确认SCL=14
#define nack gpio_read(SDA_PIN)
#define Delay_5us() bflb_mtimer_delay_us(5)
#define GPIO_CFG GPIO_OUTPUT | GPIO_PULLUP | GPIO_DRV_1

bool gpio_read(uint8_t pin) {
	bflb_gpio_deinit(gpio, pin);
	bool ret = bflb_gpio_read(gpio, pin);
	bflb_gpio_init(gpio, pin, GPIO_CFG);
	return ret;
}

void Start_I2c(void) {
	bflb_gpio_set(gpio, SCL_PIN);
	bflb_gpio_set(gpio, SDA_PIN);
	Delay_5us();
	bflb_gpio_reset(gpio, SDA_PIN);
	Delay_5us();
	bflb_gpio_reset(gpio, SCL_PIN);
}

void Stop_I2c(void) {
	bflb_gpio_reset(gpio, SCL_PIN);
	bflb_gpio_reset(gpio, SDA_PIN);
	Delay_5us();
	bflb_gpio_set(gpio, SCL_PIN);
	Delay_5us();
	bflb_gpio_set(gpio, SDA_PIN);
}

void SendByte(u8 c) {
	u8 i;
	for (i = 0; i < 8; i++) {
		if (c & 0x80)
			bflb_gpio_set(gpio, SDA_PIN);
		else
			bflb_gpio_reset(gpio, SDA_PIN);
		c <<= 1;
		Delay_5us();
		bflb_gpio_set(gpio, SCL_PIN);
		Delay_5us();
		bflb_gpio_reset(gpio, SCL_PIN);
	}
	bflb_gpio_set(gpio, SDA_PIN);
	Delay_5us();
	bflb_gpio_set(gpio, SCL_PIN);
	bflb_gpio_reset(gpio, SDA_PIN);
	bflb_gpio_reset(gpio, SCL_PIN);
}

u8 RcvByte(void) {
	u8 i, c = 0;
	bflb_gpio_set(gpio, SDA_PIN);
	for (i = 0; i < 8; i++) {
		bflb_gpio_reset(gpio, SCL_PIN);
		Delay_5us();
		bflb_gpio_set(gpio, SCL_PIN);
		c = c << 1;
		if (gpio_read(SDA_PIN)) c = c + 1;
	}
	bflb_gpio_reset(gpio, SCL_PIN);
	return c;
}

void Ack_I2c(bit a) {
	if (a)
		bflb_gpio_set(gpio, SDA_PIN);
	else
		bflb_gpio_reset(gpio, SDA_PIN);
	Delay_5us();
	bflb_gpio_set(gpio, SCL_PIN);
	Delay_5us();
	bflb_gpio_reset(gpio, SCL_PIN);
}

bit ISendByte(u8 sla, u8 c) {
	Start_I2c();
	SendByte(sla);
	if (nack) return 0;
	SendByte(c);
	if (nack) return 0;
	Stop_I2c();
	return 1;
}

bit ISendStr(u8 sla, u8 suba, u8 *s, u8 no) {
	u8 i;
	Start_I2c();
	SendByte(sla);
	if (nack) return 0;
	SendByte(suba);
	if (nack) return 0;
	for (i = 0; i < no; i++) {
		SendByte(*s++);
		if (nack) return 0;
	}
	Stop_I2c();
	return 1;
}

bit IRcvByte(u8 sla, u8 *c) {
	Start_I2c();
	SendByte(sla + 1);
	if (nack) return 0;
	*c = RcvByte();
	Ack_I2c(1);
	Stop_I2c();
	return 1;
}

bit IRcvStr(u8 sla, u8 suba, u8 *s, u8 no) {
	u8 i;
	Start_I2c();
	SendByte(sla);
	if (nack) return 0;
	SendByte(suba);
	if (nack) return 0;
	Start_I2c();
	SendByte(sla + 1);
	if (nack) return 0;
	for (i = 0; i < no - 1; i++) {
		*s++ = RcvByte();
		Ack_I2c(0);
	}
	*s = RcvByte();
	Ack_I2c(1);
	Stop_I2c();
	return 1;
}

/**
 * 初始化I2C控制器和GPIO扩展芯片
 * @return 0成功，-EINVAL设备错误
 */
int i2c_gpio_expander_init() {
	board_init();
	printf("Board init OK\r\n");

	gpio = bflb_device_get_by_name("gpio");
	if (!gpio) {
		printf("GPIO device init failed!\r\n");
		return -EINVAL;
	}

	// 配置GPIO为输出模式
	printf("Configuring I2C pins - SDA:GPIO%d, SCL:GPIO%d\r\n",
		   SDA_PIN - GPIO_PIN_0, SCL_PIN - GPIO_PIN_0);
	bflb_gpio_init(gpio, SDA_PIN, GPIO_CFG);
	bflb_gpio_init(gpio, SCL_PIN, GPIO_CFG);

	bflb_gpio_set(gpio, SDA_PIN);
	bflb_gpio_set(gpio, SCL_PIN);
	printf("I2C GPIO init OK\r\n");
	//*/
	return 0;
}

/**
 * 读取所有GPIO口状态(改进版)
 * @param states 输出参数，16个GPIO的状态数组(0/1)
 * @return 0成功，其他为错误码
 */
int i2c_gpio_read_all(uint8_t states[16]) {
	uint8_t input_data[2] = {0};  // 存储2个bank的输入数据

	for (int i = 0; i < GPIO_BANK_MAX; i++) {
		if (!IRcvStr(GPIO_EXP_ADDR, REG_INPUT_BASE + i, input_data + i, 1))
			printf("Failed to Read GPIO %d\r\n", i);
	}
	printf("\n");

	// 解析每个GPIO状态
	for (int bank = 0; bank < GPIO_BANK_MAX; bank++) {
		for (int pin = 0; pin < 8; pin++) {
			states[bank * 8 + pin] = (input_data[bank] >> pin) & 0x01;
		}
	}

	// 调试输出
	printf("GPIO状态: ");
	for (int i = 0; i < 16; i++) {
		printf("%d", states[i]);
		if ((i + 1) % 8 == 0) printf(" ");	// 每8位加空格分隔
	}
	printf("\r\n");

	return 0;
}

// 思修电子工作室功能复刻
int main(void) {
	u8 buff1[1] = {0x00};  // 设置输出结构端口状态数组
	u8 buff2[1] = {0xFF};  // 设置I/O端口状态数组2
	u8 buff3[1] = {0x00};  // 设置I/O端口状态数组3

	if (i2c_gpio_expander_init() != 0) {
		printf("初始化失败\r\n");
		while (1);
	}
	while (!ISendStr(GPIO_EXP_ADDR, 0x06, buff1, 1))  // 配置输出结构端口寄存器0
		printf("Send data to 0x06 Failed\n");
	while (!ISendStr(GPIO_EXP_ADDR, 0x07, buff1, 1))  // 配置输出结构端口寄存器1
		printf("Send data to 0x07 Failed\n");
	while (1) {
		printf("Set all gpio to high\n");
		if (!ISendStr(GPIO_EXP_ADDR, 0x02, buff2, 1))  // A端口推挽输出高电平
			printf("Send data to 0x02 Failed\n");
		if (!ISendStr(GPIO_EXP_ADDR, 0x03, buff2, 1))  // B端口推挽输出高电平
			printf("Send data to 0x03 Failed\n");
		bflb_mtimer_delay_ms(1000);	 // 延迟100ms
		printf("Set all gpio to low\n");
		if (!ISendStr(GPIO_EXP_ADDR, 0x02, buff3, 1))  // A端口推挽输出高电平
			printf("Send data to 0x02 Failed\n");
		if (!ISendStr(GPIO_EXP_ADDR, 0x03, buff3, 1))  // B端口推挽输出高电平
			printf("Send data to 0x03 Failed\n");
		bflb_mtimer_delay_ms(1000);	 // 延迟100ms
	}
}
