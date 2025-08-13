/**********************************************************
 * 实验名称：思修SX-GPIO-KB模块测试实验（PAC9698）
 * 淘宝店铺：思修电子工作室
 * 店铺地址：https://520mcu.taobao.com/
 * 芯片型号：STC89C52RC
 * 时钟说明：芯片外部11.0592MHz
 * 接线说明：P3^7接到SX-GPIO-KB模块SDA引脚
			 P3^6接到SX-GPIO-KB模块SCL引脚
 * 模块地址：A0=0，A1=0，A2=0，OE=0（模块背面短接焊盘已配置）
 * 测试现象：连接A、B、C、D、E端口的40个LED灯同时闪烁
***********************************************************/
#include <intrins.h>
#include <reg52.h>	//头文件的包含
/********************常用数据类型定义**********************/
#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
/*************************宏定义***************************/
#define _Nop() _nop_()	// 定义空指令
#define PCA9698 0x40	// PCA9698地址
/*******************端口/引脚定义区域**********************/
sbit SDA = P3 ^ 7;	// 模拟I2C数据传送位
sbit SCL = P3 ^ 6;	// 模拟I2C时钟控制位
bit ack;			// 应答标志位
/*********************函数声明区域*************************/
void Delay_Ms(u16 ms);		  // 毫秒延迟函数
void Start_I2c(void);		  // IIC起动总线函数
void Stop_I2c(void);		  // IIC结束总线函数
void SendByte(u8 c);		  // IIC字节发送函数
u8 RcvByte(void);			  // IIC字节接收函数
void Ack_I2c(bit a);		  // IIC应答函数
bit ISendByte(u8 sla, u8 c);  // 向无子地址器件发送字节数据函数
bit ISendStr(u8 sla, u8 suba, u8 *s, u8 no);
// 向有子地址器件发送多字节数据函数
bit IRcvByte(u8 sla, u8 *c);  // 向无子地址器件读字节数据函数
bit IRcvStr(u8 sla, u8 suba, u8 *s, u8 no);
// 向有子地址器件接收多字节数据函数
//*********************主函数区域**************************/
void main(void) {
	u8 All_ctrl[1] = {0x80};
	// 设置全组控制寄存器数组（这里为默认值）
	u8 Select_Mode[1] = {0x12};
	// 设置模式选择寄存器（这里配置为应答时输出改变 使能中断响应）
	u8 GPIO_H[1] = {0xff};
	// 设置I/O端口状态数组
	u8 GPIO_L[1] = {0x00};
	// 设置I/O端口状态数组
	// u8 In_array[1]={0x7f};
	// 设置I/O某端口号为中断标志位
	Delay_Ms(10);
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
		Delay_Ms(100);
		ISendStr(PCA9698, 0X08, GPIO_L, 1);	 // A推挽输出高电平
		ISendStr(PCA9698, 0X09, GPIO_L, 1);	 // B推挽输出高电平
		ISendStr(PCA9698, 0X0a, GPIO_L, 1);	 // C推挽输出高电平
		ISendStr(PCA9698, 0X0b, GPIO_L, 1);	 // D推挽输出高电平
		ISendStr(PCA9698, 0X0c, GPIO_L, 1);	 // E推挽输出高电平
		Delay_Ms(100);						 // 延迟闪烁
		ISendStr(PCA9698, 0X08, GPIO_H, 1);	 // A推挽输出低电平
		ISendStr(PCA9698, 0X09, GPIO_H, 1);	 // B推挽输出低电平
		ISendStr(PCA9698, 0X0a, GPIO_H, 1);	 // C推挽输出低电平
		ISendStr(PCA9698, 0X0b, GPIO_H, 1);	 // D推挽输出低电平
		ISendStr(PCA9698, 0X0C, GPIO_H, 1);	 // E推挽输出低电平
	}
}
//***************************************************************/
// 起动总线函数Start_I2c(void)，无形参,无返回值
//***************************************************************/
void Start_I2c(void) {
	SDA = 1;  // 发送起始条件的数据信号
	_Nop();
	SCL = 1;
	_Nop();	 // 起始条件建立时间大于4.7us,延时
	_Nop();
	_Nop();
	_Nop();
	_Nop();
	SDA = 0;  // 发送起始信号
	_Nop();	  // 起始条件锁定时间大于4μs
	_Nop();
	_Nop();
	_Nop();
	_Nop();
	SCL = 0;  // 钳住I2C总线，准备发送或接收数据
	_Nop();
	_Nop();
}
//***************************************************************/
// 结束总线函数 Stop_I2c(void)，无形参,无返回值
//***************************************************************/
void Stop_I2c(void) {
	SDA = 0;  // 发送结束条件的数据信号
	_Nop();	  // 发送结束条件的时钟信号
	SCL = 1;  // 结束条件建立时间大于4μs
	_Nop();
	_Nop();
	_Nop();
	_Nop();
	_Nop();
	SDA = 1;  // 发送I2C总线结束信号
	_Nop();
	_Nop();
	_Nop();
	_Nop();
}
//***************************************************************/
// IIC字节数据传送函数SendByte(u8 c)，有形参c,无返回值
//***************************************************************/
void SendByte(u8 c) {
	u8 BitCnt;
	for (BitCnt = 0; BitCnt < 8; BitCnt++) {
		// 要传送的数据长度为8位
		if ((c << BitCnt) & 0x80)
			SDA = 1;  // 判断发送位
		else
			SDA = 0;
		_Nop();
		SCL = 1;  // 置时钟线为高，通知被控器开始接收数据位
		_Nop();
		_Nop();	 // 保证时钟高电平周期大于4μs
		_Nop();
		_Nop();
		_Nop();
		SCL = 0;
	}
	_Nop();
	_Nop();
	SDA = 1;  // 8位发送完后释放数据线，准备接收应答位
	_Nop();
	_Nop();
	SCL = 1;
	_Nop();
	_Nop();
	_Nop();
	if (SDA == 1)
		ack = 0;
	else
		ack = 1;  // 判断是否接收到应答信号
	SCL = 0;
	_Nop();
	_Nop();
}
//***************************************************************/
// IIC字节数据接收函数RcvByte(void)，无形参,有返回值retc
//***************************************************************/
u8 RcvByte(void) {
	u8 retc;
	u8 BitCnt;
	retc = 0;
	SDA = 1;  // 置数据线为输入方式
	for (BitCnt = 0; BitCnt < 8; BitCnt++) {
		_Nop();
		SCL = 0;  // 置时钟线为低，准备接收数据位
		_Nop();
		_Nop();	 // 时钟低电平周期大于4.7μs
		_Nop();
		_Nop();
		_Nop();
		SCL = 1;  // 置时钟线为高使数据线上数据有效
		_Nop();
		_Nop();
		retc = retc << 1;
		if (SDA == 1) retc = retc + 1;	// 读数据位,接收的数据位放入retc中
		_Nop();
		_Nop();
	}
	SCL = 0;
	_Nop();
	_Nop();
	return (retc);
}
//***************************************************************/
// IIC应答子函数Ack_I2c(bit a)，有形参a,无返回值
//***************************************************************/
void Ack_I2c(bit a) {
	if (a == 0)
		SDA = 0;  // 在此发出应答或非应答信号
	else
		SDA = 1;
	_Nop();
	_Nop();
	_Nop();
	SCL = 1;
	_Nop();
	_Nop();	 // 时钟低电平周期大于4μs
	_Nop();
	_Nop();
	_Nop();
	SCL = 0;  // 清时钟线，钳住I2C总线以便继续接收*/
	_Nop();
	_Nop();
}
//***************************************************************/
// 向无子地址器件发送字节数据函数 ISendByte(u8 sla,u8 c)
// 有形参sla:地址；c：发送数据；无返回值
//***************************************************************/
bit ISendByte(u8 sla, u8 c) {
	Start_I2c();	// 启动总线
	SendByte(sla);	// 发送器件地址
	if (ack == 0) return (0);
	SendByte(c);  // 发送数据
	if (ack == 0) return (0);
	Stop_I2c();	 // 结束总线
	return (1);
}
//***************************************************************/
// 向有子地址器件发送多字节数据函数ISendStr(u8 sla,u8 suba,u8 *s,u8 no)
// 有形参sla:地址；suba：子地址；*s：数据数组；no：字节个数；有返回值1
//***************************************************************/
bit ISendStr(u8 sla, u8 suba, u8 *s, u8 no) {
	u8 i;

	Start_I2c();  // 启动总线

	SendByte(sla);	// 发送器件地址
	if (ack == 0) return (0);

	SendByte(suba);	 // 发送器件子地址
	if (ack == 0) return (0);

	for (i = 0; i < no; i++) {
		SendByte(*s);  // 发送数据
		if (ack == 0) return (0);
		s++;
	}

	Stop_I2c();	 // 结束总线

	return (1);
}
//***************************************************************/
// 向无子地址器件读字节数据函数IRcvByte(u8 sla,u8 *c)
// 有形参sla:地址；suba：发送数据；*c：；有返回值1
//***************************************************************/
bit IRcvByte(u8 sla, u8 *c) {
	Start_I2c();  // 启动总线

	SendByte(sla + 1);	// 发送器件地址
	if (ack == 0) return (0);

	*c = RcvByte();	 // 读取数据
	Ack_I2c(1);		 // 发送非就答位
	Stop_I2c();		 // 结束总线
	return (1);
}
//***************************************************************/
// 向有子地址器件接收多字节数据函数IRcvStr(u8 sla,u8 suba,u8 *s,u8 no)
// 有形参sla:地址；suba：子地址；*s：数据数组；no：字节个数；有返回值1
//***************************************************************/
bit IRcvStr(u8 sla, u8 suba, u8 *s, u8 no) {
	u8 i;

	Start_I2c();	// 启动总线
	SendByte(sla);	// 发送器件地址
	if (ack == 0) return (0);
	SendByte(suba);	 // 发送器件子地址
	if (ack == 0) return (0);
	Start_I2c();
	SendByte(sla + 1);
	if (ack == 0) return (0);
	for (i = 0; i < no - 1; i++) {
		*s = RcvByte();	 // 发送数据
		Ack_I2c(0);		 // 发送就答位
		s++;
	}
	*s = RcvByte();
	Ack_I2c(1);	 // 发送非应位
	Stop_I2c();	 // 结束总线
	return (1);
}
//***************************************************************/
// 毫秒延迟函数Delay_Ms(u16 ms)
// 有形参ms:毫秒参数；无返回值
//***************************************************************/
void Delay_Ms(u16 ms) {
	unsigned int us;
	for (; ms > 0; ms--)
		for (us = 200; us > 0; us--);
}