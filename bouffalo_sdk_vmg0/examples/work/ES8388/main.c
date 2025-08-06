#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "bflb_dma.h"
#include "bflb_gpio.h"
#include "bflb_i2c.h"
#include "bflb_i2s.h"
#include "bflb_l1c.h"
#include "bflb_mtimer.h"
#include "bl616_glb.h"
#include "board.h"
#include "bsp_es8388.h"
struct bflb_device_s *i2s0;
struct bflb_device_s *dma0_ch0;
struct bflb_device_s *dma0_ch1;

#define BUFFER_SIZE 6400
uint8_t rx_buffer[BUFFER_SIZE];
uint8_t tx_buffer[BUFFER_SIZE];

static ES8388_Cfg_Type ES8388Cfg = {
	.work_mode = ES8388_CODEC_MDOE,
	.role = ES8388_SLAVE,
	.mic_input_mode = ES8388_SINGLE_ENDED_MIC,
	.mic_pga = ES8388_MIC_PGA_6DB,
	.i2s_frame = ES8388_LEFT_JUSTIFY_FRAME,
	.data_width = ES8388_DATA_LEN_16,
};

#define PI 3.1415926535
#define SAMPLE_RATE 16000
#define SINE_FREQUENCY 440.0
#define AMPLITUDE 20000.0

bool play_state;
int cnt;

void dma0_ch0_isr(void *arg) {
	play_state = 0;
	cnt++;
	if (cnt > 10) {
		printf("播放结束\n");
		memset(tx_buffer, 0, sizeof(tx_buffer));
		bflb_l1c_dcache_clean_range((void *)tx_buffer, sizeof(tx_buffer));
	}
}

void dma0_ch1_isr(void *arg) {}

void generate_sine_wave(void) {
	printf("Generating %.1f Hz sine wave...\r\n", SINE_FREQUENCY);
	int16_t *p = (int16_t *)tx_buffer;
	int num_samples = BUFFER_SIZE / 4;

	for (int i = 0; i < num_samples; i++) {
		double sine_value = sin(2 * PI * SINE_FREQUENCY * i / SAMPLE_RATE);
		int16_t sample_value = (int16_t)(AMPLITUDE * sine_value);

		p[i * 2] = sample_value;
		p[i * 2 + 1] = sample_value;
	}
	printf("Sine wave generated.\r\n");
}

void i2s_gpio_init() {
	struct bflb_device_s *gpio;
	gpio = bflb_device_get_by_name("gpio");
	bflb_gpio_init(gpio, GPIO_PIN_1,
				   GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN |
					   GPIO_DRV_1);
	bflb_gpio_init(gpio, GPIO_PIN_10,
				   GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN |
					   GPIO_DRV_1);
	bflb_gpio_init(gpio, GPIO_PIN_3,
				   GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN |
					   GPIO_DRV_1);
	bflb_gpio_init(gpio, GPIO_PIN_0,
				   GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN |
					   GPIO_DRV_1);
	bflb_gpio_init(gpio, GPIO_PIN_2,
				   GPIO_FUNC_CLKOUT | GPIO_ALTERNATE | GPIO_PULLUP |
					   GPIO_SMT_EN | GPIO_DRV_1);
}

void i2s_dma_init() {
	static struct bflb_dma_channel_lli_pool_s tx_llipool[100];
	static struct bflb_dma_channel_lli_transfer_s tx_transfers[1];
	static struct bflb_dma_channel_lli_pool_s rx_llipool[100];
	static struct bflb_dma_channel_lli_transfer_s rx_transfers[1];

	struct bflb_i2s_config_s i2s_cfg = {
		.bclk_freq_hz = SAMPLE_RATE * 16 * 2,
		.role = I2S_ROLE_MASTER,
		.format_mode = I2S_MODE_LEFT_JUSTIFIED,
		.channel_mode = I2S_CHANNEL_MODE_NUM_2,
		.frame_width = I2S_SLOT_WIDTH_16,
		.data_width = I2S_SLOT_WIDTH_16,
		.fs_offset_cycle = 0,
		.tx_fifo_threshold = 4,
		.rx_fifo_threshold = 4,
	};

	struct bflb_dma_channel_config_s tx_config = {
		.direction = DMA_MEMORY_TO_PERIPH,
		.src_req = DMA_REQUEST_NONE,
		.dst_req = DMA_REQUEST_I2S_TX,
		.src_addr_inc = DMA_ADDR_INCREMENT_ENABLE,
		.dst_addr_inc = DMA_ADDR_INCREMENT_DISABLE,
		.src_burst_count = DMA_BURST_INCR1,
		.dst_burst_count = DMA_BURST_INCR1,
		.src_width = DMA_DATA_WIDTH_16BIT,
		.dst_width = DMA_DATA_WIDTH_16BIT,
	};

	struct bflb_dma_channel_config_s rx_config = {
		.direction = DMA_PERIPH_TO_MEMORY,
		.src_req = DMA_REQUEST_I2S_RX,
		.dst_req = DMA_REQUEST_NONE,
		.src_addr_inc = DMA_ADDR_INCREMENT_DISABLE,
		.dst_addr_inc = DMA_ADDR_INCREMENT_ENABLE,
		.src_burst_count = DMA_BURST_INCR1,
		.dst_burst_count = DMA_BURST_INCR1,
		.src_width = DMA_DATA_WIDTH_16BIT,
		.dst_width = DMA_DATA_WIDTH_16BIT};

	i2s0 = bflb_device_get_by_name("i2s0");
	bflb_i2s_init(i2s0, &i2s_cfg);
	bflb_i2s_link_txdma(i2s0, true);
	bflb_i2s_link_rxdma(i2s0, true);

	dma0_ch0 = bflb_device_get_by_name("dma0_ch0");
	dma0_ch1 = bflb_device_get_by_name("dma0_ch1");

	bflb_dma_channel_init(dma0_ch0, &tx_config);
	bflb_dma_channel_init(dma0_ch1, &rx_config);

	bflb_dma_channel_irq_attach(dma0_ch0, dma0_ch0_isr, NULL);
	bflb_dma_channel_irq_attach(dma0_ch1, dma0_ch1_isr, NULL);

	tx_transfers[0].src_addr = (uint32_t)tx_buffer;
	tx_transfers[0].dst_addr = (uint32_t)DMA_ADDR_I2S_TDR;
	tx_transfers[0].nbytes = sizeof(tx_buffer);

	rx_transfers[0].src_addr = (uint32_t)DMA_ADDR_I2S_RDR;
	rx_transfers[0].dst_addr = (uint32_t)rx_buffer;
	rx_transfers[0].nbytes = sizeof(rx_buffer);

	uint32_t num =
		bflb_dma_channel_lli_reload(dma0_ch0, tx_llipool, 100, tx_transfers, 1);
	bflb_dma_channel_lli_link_head(dma0_ch0, tx_llipool, num);

	num =
		bflb_dma_channel_lli_reload(dma0_ch1, rx_llipool, 100, rx_transfers, 1);
	bflb_dma_channel_lli_link_head(dma0_ch1, rx_llipool, num);

	bflb_dma_channel_start(dma0_ch0);
	bflb_dma_channel_start(dma0_ch1);
}

void mclk_out_init() {
	GLB_Set_I2S_CLK(ENABLE, 2, GLB_I2S_DI_SEL_I2S_DI_INPUT,
					GLB_I2S_DO_SEL_I2S_DO_OUTPT);
	GLB_Set_Chip_Clock_Out2_Sel(GLB_CHIP_CLK_OUT_2_I2S_REF_CLK);
}

int main(void) {
	board_init();
	board_i2c0_gpio_init();

	printf("Audio Playback Test: Sine Wave\r\n");

	printf("ES8388 init...\n\r");
	ES8388_Init(&ES8388Cfg);
	ES8388_Set_Voice_Volume(300);

	i2s_gpio_init();
	mclk_out_init();

	generate_sine_wave();

	bflb_l1c_dcache_clean_range((void *)tx_buffer, sizeof(tx_buffer));

	i2s_dma_init();

	bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE,
							 I2S_CMD_DATA_ENABLE_TX | I2S_CMD_DATA_ENABLE_RX);

	printf("Playback started. You should hear a pure sine wave tone.\r\n");
	printf("Recording is also active in the background.\r\n");

	while (1) {
		bflb_mtimer_delay_ms(1000);
	}
}