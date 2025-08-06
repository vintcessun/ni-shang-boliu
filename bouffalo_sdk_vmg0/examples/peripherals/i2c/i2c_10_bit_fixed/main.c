#include "bflb_i2c.h"
#include "bflb_mtimer.h"
#include "board.h"
#define PCA9698 0x80
#define I2C_10BIT_TRANSFER_LENGTH 32

static struct bflb_device_s *i2c0;

int ISendStr(uint8_t sla, uint8_t suba, uint8_t *s, uint8_t no) {
	bflb_i2c_disable(i2c0);
	bflb_i2c_addr_config(i2c0, sla, suba, 1, false);

	bflb_i2c_set_datalen(i2c0, no);
	bflb_i2c_set_dir(i2c0, 0);
	int ret = bflb_i2c_write_bytes(i2c0, s, no);
	// bflb_i2c_disable(i2c0);
	//  int ret = bflb_i2c_transfer(i2c0, msgs, 1);
	printf("SLA: %x SUBADDR:%x data:%x len:%d ret:%d\n", sla, suba, s[0], no,
		   ret);
	bflb_mtimer_delay_ms(10);
	return ret;
}

int main(void) {
	board_init();
	printf("board init finished\n");
	board_i2c0_gpio_init();
	i2c0 = bflb_device_get_by_name("i2c0");
	bflb_i2c_init(i2c0, 400000);
	printf("i2c0 init finished\n");

	uint8_t All_ctrl[1] = {0x80};
	// 设置全组控制寄存器数组（这里为默认值）
	uint8_t Select_Mode[1] = {0x12};
	// 设置模式选择寄存器（这里配置为应答时输出改变 使能中断响应）
	uint8_t GPIO_H[1] = {0xff};
	// 设置I/O端口状态数组
	uint8_t GPIO_L[1] = {0x00};
	// 设置I/O端口状态数组
	// uint8_t In_array[1]={0x7f};
	// 设置I/O某端口号为中断标志位
	bflb_mtimer_delay_ms(10);
	ISendStr(PCA9698, 0X2A, Select_Mode, 1);
	// 配置模式选择寄存器地址0x2A
	ISendStr(PCA9698, 0X29, All_ctrl, 1);
	// 配置全组控制寄存器地址0X29
	ISendStr(PCA9698, 0X28, GPIO_H, 1);
	// 配置输出结构寄存器地址0X28(写入1为默认推挽，0为开漏)，这里配置为推挽
	// ISendStr(PCA9698,0X23,In_array,1);
	// 0X23为中断屏蔽地址寄存器,配置为使用PD8作为中断标志位
	// 当PD8端口状态改变时中断发生,中断输出引脚In变低电平
	// 配置I/0口寄存器(设置为输出/输入，A~D组地址为0X18~0X1C)
	ISendStr(PCA9698, 0X18, GPIO_L, 1);	 // 配置A组I/O为输出
	ISendStr(PCA9698, 0X19, GPIO_L, 1);	 // 配置B组I/O为输出
	ISendStr(PCA9698, 0X1A, GPIO_L, 1);	 // 配置C组I/O为输出
	ISendStr(PCA9698, 0X1B, GPIO_L, 1);	 // 配置D组I/O为输出
	ISendStr(PCA9698, 0X1C, GPIO_L, 1);	 // 配置E组I/O为输出
	while (1) {
		// 配置输出端口寄存器(设置输出相应的状态，A~E组端口的地址为0X08~0X0C）
		bflb_mtimer_delay_ms(1000);
		ISendStr(PCA9698, 0X08, GPIO_L, 1);	 // A推挽输出高电平
		ISendStr(PCA9698, 0X09, GPIO_L, 1);	 // B推挽输出高电平
		ISendStr(PCA9698, 0X0a, GPIO_L, 1);	 // C推挽输出高电平
		ISendStr(PCA9698, 0X0b, GPIO_L, 1);	 // D推挽输出高电平
		ISendStr(PCA9698, 0X0c, GPIO_L, 1);	 // E推挽输出高电平
		bflb_mtimer_delay_ms(1000);			 // 延迟闪烁
		ISendStr(PCA9698, 0X08, GPIO_H, 1);	 // A推挽输出低电平
		ISendStr(PCA9698, 0X09, GPIO_H, 1);	 // B推挽输出低电平
		ISendStr(PCA9698, 0X0a, GPIO_H, 1);	 // C推挽输出低电平
		ISendStr(PCA9698, 0X0b, GPIO_H, 1);	 // D推挽输出低电平
		ISendStr(PCA9698, 0X0C, GPIO_H, 1);	 // E推挽输出低电平
	}

	while (1) {
	}
}
