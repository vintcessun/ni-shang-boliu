// es8388_driver.c - REBORN AND BATTLE-HARDENED

#include "es8388_driver.h"

#include <semphr.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bflb_dma.h"
#include "bflb_gpio.h"
#include "bflb_i2c.h"
#include "bflb_i2s.h"
#include "bflb_l1c.h"
#include "bflb_mtimer.h"
#include "bl616_glb.h"
#include "board.h"
#include "bsp_es8388.h"
#include "log.h"
#include "task.h"

// --- 模块内部变量 ---
static struct bflb_device_s *i2s0_dev;
static struct bflb_device_s *dma0_ch0_dev;	// TX
static struct bflb_device_s *dma0_ch1_dev;	// RX

// RX (录音) 相关的定义 (保持不变，工作得很好)
#define RX_BUFFER_SIZE 12800
static uint8_t rx_audio_buffer[RX_BUFFER_SIZE] __attribute__((aligned(4)));
static SemaphoreHandle_t rx_buffer_ready_sem = NULL;

// ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
//              ★★★ 播放逻辑 - 全新、硬核、防抖动的多块缓冲机制 ★★★
// ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★

// 定义每个DMA传输块的大小，这个值应该等于或大于网络上收到的单个音频包大小
#define TX_DMA_BLOCK_SIZE (4096)
// 定义我们使用多少个块来做缓冲。越多的块能提供越好的抗网络抖动能力，但会增加延迟和内存消耗。4个是很好的起点。
#define TX_DMA_BLOCK_NUM (8)
// 总的环形缓冲区大小
#define TX_BUFFER_SIZE (TX_DMA_BLOCK_SIZE * TX_DMA_BLOCK_NUM)

// 这是我们真正的物理环形缓冲区，DMA将从这里循环读取数据
static uint8_t tx_audio_buffer[TX_BUFFER_SIZE] __attribute__((aligned(4)));

// ★★★ 核心武器：计数信号量 ★★★
// 这个信号量代表了应用程序“可以填充”的空闲块的数量。
// 初始化时有 TX_DMA_BLOCK_NUM
// 个，当应用填充一个块，就消耗一个。当DMA播放完一个块，就在中断里释放一个。
// 这就是我们的“流控”机制！
static SemaphoreHandle_t tx_blocks_available_sem = NULL;

// ★★★ 写指针（按块索引）★★★
// 用来标记下一个要被应用层写入的块是哪一个
static volatile uint32_t tx_write_block_idx = 0;

static ES8388_Cfg_Type es8388_codec_cfg = {
	.work_mode = ES8388_CODEC_MDOE,
	.role = ES8388_SLAVE,
	.mic_input_mode = ES8388_SINGLE_ENDED_MIC,
	.mic_pga = ES8388_MIC_PGA_6DB,
	.i2s_frame = ES8388_LEFT_JUSTIFY_FRAME,
	.data_width = ES8388_DATA_LEN_16,
};
static audio_module_state_t current_audio_state = AUDIO_STATE_UNINITIALIZED;

// --- DMA 中断服务函数 ---

// ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
//              ★★★ 这是新架构的脉搏！ ★★★
// ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
static void dma0_ch0_tx_isr_callback(void *arg) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	// DMA 刚刚播放完一个块，这意味着这个块现在“空闲”了。
	// 我们释放一个信号量，告诉在 es8388_audio_play 中等待的播放任务：
	// “嘿，哥们，你可以再填充一个新的数据块了！”
	if (tx_blocks_available_sem != NULL) {
		xSemaphoreGiveFromISR(tx_blocks_available_sem,
							  &xHigherPriorityTaskWoken);
		// 如果释放信号量唤醒了一个更高优先级的任务，我们立刻进行任务切换，保证音频数据被最快填充。
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

static void dma0_ch1_rx_isr_callback(void *arg) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if (rx_buffer_ready_sem != NULL) {
		xSemaphoreGiveFromISR(rx_buffer_ready_sem, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

// --- 内部辅助函数 ---
static int audio_i2s_gpio_init(void) {
	struct bflb_device_s *gpio_dev;
	gpio_dev = bflb_device_get_by_name("gpio");
	bflb_gpio_init(gpio_dev, GPIO_PIN_1,
				   GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN |
					   GPIO_DRV_1);
	bflb_gpio_init(gpio_dev, GPIO_PIN_10,
				   GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN |
					   GPIO_DRV_1);
	bflb_gpio_init(gpio_dev, GPIO_PIN_3,
				   GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN |
					   GPIO_DRV_1);
	bflb_gpio_init(gpio_dev, GPIO_PIN_0,
				   GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN |
					   GPIO_DRV_1);
	bflb_gpio_init(gpio_dev, GPIO_PIN_2,
				   GPIO_FUNC_CLKOUT | GPIO_ALTERNATE | GPIO_PULLUP |
					   GPIO_SMT_EN | GPIO_DRV_1);
	return 0;
}

static void audio_mclk_out_init(void) {
	GLB_Set_I2S_CLK(ENABLE, 2, GLB_I2S_DI_SEL_I2S_DI_INPUT,
					GLB_I2S_DO_SEL_I2S_DO_OUTPT);
	GLB_Set_Chip_Clock_Out2_Sel(GLB_CHIP_CLK_OUT_2_I2S_REF_CLK);
}

static int audio_i2s_dma_init(void) {
	// ★★★ DMA LLI (链表) 设置需要改变，以支持块状循环传输 ★★★
	static struct bflb_dma_channel_lli_pool_s tx_llipool[TX_DMA_BLOCK_NUM];
	static struct bflb_dma_channel_lli_transfer_s
		tx_transfers[TX_DMA_BLOCK_NUM];

	static struct bflb_dma_channel_lli_pool_s rx_llipool[10];  // RX部分保持不变
	static struct bflb_dma_channel_lli_transfer_s rx_transfers[1];

	struct bflb_i2s_config_s i2s_cfg = {
		.bclk_freq_hz = 16000 * 16 * 2,	 // 确保与TTS服务生成的采样率一致
		.role = I2S_ROLE_MASTER,
		.format_mode = I2S_MODE_LEFT_JUSTIFIED,
		.channel_mode = I2S_CHANNEL_MODE_NUM_2,
		.frame_width = I2S_SLOT_WIDTH_16,
		.data_width = I2S_SLOT_WIDTH_16,
		.fs_offset_cycle = 0,
		.tx_fifo_threshold = 4,
		.rx_fifo_threshold = 4,
	};
	struct bflb_dma_channel_config_s rx_dma_cfg = {
		.direction = DMA_PERIPH_TO_MEMORY,
		.src_req = DMA_REQUEST_I2S_RX,
		.dst_req = DMA_REQUEST_NONE,
		.src_addr_inc = DMA_ADDR_INCREMENT_DISABLE,
		.dst_addr_inc = DMA_ADDR_INCREMENT_ENABLE,
		.src_burst_count = DMA_BURST_INCR1,
		.dst_burst_count = DMA_BURST_INCR1,
		.src_width = DMA_DATA_WIDTH_16BIT,
		.dst_width = DMA_DATA_WIDTH_16BIT};
	struct bflb_dma_channel_config_s tx_dma_cfg = {
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

	i2s0_dev = bflb_device_get_by_name("i2s0");
	bflb_i2s_init(i2s0_dev, &i2s_cfg);
	bflb_i2s_link_txdma(i2s0_dev, true);
	bflb_i2s_link_rxdma(i2s0_dev, true);

	dma0_ch0_dev = bflb_device_get_by_name("dma0_ch0");
	dma0_ch1_dev = bflb_device_get_by_name("dma0_ch1");
	bflb_dma_channel_init(dma0_ch0_dev, &tx_dma_cfg);
	bflb_dma_channel_init(dma0_ch1_dev, &rx_dma_cfg);

	bflb_dma_channel_irq_attach(dma0_ch0_dev, dma0_ch0_tx_isr_callback, NULL);
	bflb_dma_channel_irq_attach(dma0_ch1_dev, dma0_ch1_rx_isr_callback, NULL);

	// RX DMA 设置 (保持不变)
	rx_transfers[0].src_addr = (uint32_t)DMA_ADDR_I2S_RDR;
	rx_transfers[0].dst_addr = (uint32_t)rx_audio_buffer;
	rx_transfers[0].nbytes = sizeof(rx_audio_buffer);
	uint32_t num_lli_rx = bflb_dma_channel_lli_reload(dma0_ch1_dev, rx_llipool,
													  10, rx_transfers, 1);
	bflb_dma_channel_lli_link_head(dma0_ch1_dev, rx_llipool, num_lli_rx);

	// ★★★ TX DMA 设置 (全新，核心！) ★★★
	// 我们不再是一次性传输整个大缓冲区，而是创建 N
	// 个传输任务，每个任务传输一个块。
	// 然后把这些任务链接成一个环，DMA就会永无止境地、一块一块地循环播放。
	for (int i = 0; i < TX_DMA_BLOCK_NUM; i++) {
		tx_transfers[i].src_addr =
			(uint32_t)(tx_audio_buffer + (i * TX_DMA_BLOCK_SIZE));
		tx_transfers[i].dst_addr = (uint32_t)DMA_ADDR_I2S_TDR;
		tx_transfers[i].nbytes = TX_DMA_BLOCK_SIZE;
	}
	uint32_t num_lli_tx =
		bflb_dma_channel_lli_reload(dma0_ch0_dev, tx_llipool, TX_DMA_BLOCK_NUM,
									tx_transfers, TX_DMA_BLOCK_NUM);
	bflb_dma_channel_lli_link_head(dma0_ch0_dev, tx_llipool, num_lli_tx);

	return 0;
}

// --- 公共接口函数实现 ---
int es8388_audio_init(void) {
	if (current_audio_state != AUDIO_STATE_UNINITIALIZED) {
		return 0;
	}
	LOG_I("Initializing ES8388 audio module (Battle-Hardened V3)...\r\n");
	board_i2c0_gpio_init();
	ES8388_Init(&es8388_codec_cfg);
	ES8388_Set_Voice_Volume(300);
	audio_i2s_gpio_init();
	audio_mclk_out_init();

	// ★★★ 把DMA初始化放在所有状态初始化之前 ★★★
	if (audio_i2s_dma_init() != 0) {
		LOG_E("I2S and DMA init failed!\r\n");
		return -1;
	}

	// ★★★ 确保在最开始时，信号量是全新的 ★★★
	if (tx_blocks_available_sem != NULL) {
		vSemaphoreDelete(tx_blocks_available_sem);
	}
	tx_blocks_available_sem =
		xSemaphoreCreateCounting(TX_DMA_BLOCK_NUM, TX_DMA_BLOCK_NUM);
	if (tx_blocks_available_sem == NULL) {
		LOG_E("Failed to create tx_blocks_available_sem! PANIC!\r\n");
		return -1;
	}
	tx_write_block_idx = 0;

	if (rx_buffer_ready_sem == NULL) {
		rx_buffer_ready_sem = xSemaphoreCreateBinary();
	}

	memset(tx_audio_buffer, 0, TX_BUFFER_SIZE);
	bflb_l1c_dcache_clean_range((void *)tx_audio_buffer, TX_BUFFER_SIZE);

	current_audio_state = AUDIO_STATE_INITIALIZED;
	LOG_I(
		"ES8388 audio module initialized and ready for the final battle.\r\n");
	return 0;
}

int es8388_audio_start_capture(void) {
	if (current_audio_state == AUDIO_STATE_UNINITIALIZED) {
		return -1;
	}
	if (current_audio_state == AUDIO_STATE_CAPTURING) {
		return 0;
	}
	LOG_I("Starting audio capture and playback engine...\r\n");
	bflb_dma_channel_start(dma0_ch0_dev);
	bflb_dma_channel_start(dma0_ch1_dev);
	bflb_i2s_feature_control(i2s0_dev, I2S_CMD_DATA_ENABLE,
							 I2S_CMD_DATA_ENABLE_RX | I2S_CMD_DATA_ENABLE_TX);
	current_audio_state = AUDIO_STATE_CAPTURING;
	return 0;
}

int es8388_audio_stop_capture(void) {
	if (current_audio_state != AUDIO_STATE_CAPTURING) {
		return 0;
	}
	LOG_I("Stopping audio capture...\r\n");
	bflb_i2s_feature_control(i2s0_dev, I2S_CMD_DATA_ENABLE, 0);
	bflb_dma_channel_stop(dma0_ch1_dev);
	bflb_dma_channel_stop(dma0_ch0_dev);
	current_audio_state = AUDIO_STATE_INITIALIZED;
	return 0;
}

void es8388_audio_deinit(void) {
	if (current_audio_state == AUDIO_STATE_UNINITIALIZED) {
		return;
	}
	if (current_audio_state == AUDIO_STATE_CAPTURING) {
		es8388_audio_stop_capture();
	}
	LOG_I("Deinitializing ES8388 audio module...\r\n");
	if (dma0_ch0_dev) {
		bflb_dma_channel_irq_detach(dma0_ch0_dev);
	}
	if (dma0_ch1_dev) {
		bflb_dma_channel_irq_detach(dma0_ch1_dev);
	}
	if (tx_blocks_available_sem) {
		vSemaphoreDelete(tx_blocks_available_sem);
		tx_blocks_available_sem = NULL;
	}

	i2s0_dev = NULL;
	dma0_ch0_dev = NULL;
	dma0_ch1_dev = NULL;
	current_audio_state = AUDIO_STATE_UNINITIALIZED;
}

audio_module_state_t es8388_audio_get_state(void) {
	return current_audio_state;
}

int es8388_audio_get_data(uint8_t *buffer, uint32_t buffer_size,
						  uint32_t *out_len, TickType_t timeout_ticks) {
	if (out_len) {
		*out_len = 0;
	}
	if (current_audio_state != AUDIO_STATE_CAPTURING) {
		return -1;
	}
	if (!buffer || buffer_size == 0) {
		return -2;
	}
	if (xSemaphoreTake(rx_buffer_ready_sem, timeout_ticks) == pdTRUE) {
		uint32_t len_to_copy =
			(buffer_size < RX_BUFFER_SIZE) ? buffer_size : RX_BUFFER_SIZE;
		bflb_l1c_dcache_invalidate_range((void *)rx_audio_buffer,
										 RX_BUFFER_SIZE);
		memcpy(buffer, rx_audio_buffer, len_to_copy);
		if (out_len) {
			*out_len = len_to_copy;
		}
		return 0;
	} else {
		return -3;
	}
}

// ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
//              ★★★ 新的、绝对可靠的、永不卡顿的音频播放函数 ★★★
// ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
int es8388_audio_play(const uint8_t *data, uint32_t len) {
	if (current_audio_state != AUDIO_STATE_CAPTURING) {
		LOG_E("Play failed: Audio engine not in capturing/playing state.\r\n");
		return -1;
	}
	if (!data || len == 0) {
		return -2;	// 无效参数
	}

	const uint8_t *p_data = data;
	uint32_t remaining_len = len;

	// 这个循环会把传入的任意长度的数据，切分成标准大小的块，然后一个一个地送入DMA缓冲区
	while (remaining_len > 0) {
		// 1. 获取一个空闲块的“许可”。如果所有块都在等待DMA播放，这里会阻塞，
		//    从而给网络和DMA追上来的时间。这就是反压和流控！
		if (xSemaphoreTake(tx_blocks_available_sem, portMAX_DELAY) != pdTRUE) {
			LOG_E(
				"Play failed: Timeout waiting for a free DMA block. The "
				"pipeline is likely stuck.\r\n");
			return -4;	// 获取许可失败
		}

		// 2. 计算目标地址和本次要拷贝的长度
		uint8_t *dest_addr =
			tx_audio_buffer + (tx_write_block_idx * TX_DMA_BLOCK_SIZE);
		uint32_t len_to_copy = (remaining_len > TX_DMA_BLOCK_SIZE)
								   ? TX_DMA_BLOCK_SIZE
								   : remaining_len;

		// 3. 把数据拷贝到DMA缓冲区
		memcpy(dest_addr, p_data, len_to_copy);

		// 如果传入的数据不足以填满一个块，用静音数据(0)补齐。
		// 这至关重要，可以防止播放上一个缓冲区的残留数据。
		if (len_to_copy < TX_DMA_BLOCK_SIZE) {
			memset(dest_addr + len_to_copy, 0, TX_DMA_BLOCK_SIZE - len_to_copy);
		}

		// 4.
		// 【关键】通知Cache，这块内存已经被我们修改了，快把它写回到物理RAM，否则DMA读到的还是旧数据！
		bflb_l1c_dcache_clean_range((void *)dest_addr, TX_DMA_BLOCK_SIZE);

		// 5. 更新数据指针和剩余长度，并将写指针移动到下一个块
		p_data += len_to_copy;
		remaining_len -= len_to_copy;
		tx_write_block_idx = (tx_write_block_idx + 1) % TX_DMA_BLOCK_NUM;
	}

	return 0;
}

int es8388_audio_fill_silence(void) {
	if (current_audio_state != AUDIO_STATE_CAPTURING) {
		return -1;
	}

	// 这个函数的哲学是：我不知道DMA现在读到哪里了，我也不知道缓冲区里有什么。
	// 我只知道，我要用最快的速度，把整个环形缓冲区，用静音数据覆盖一遍。
	// 这样做，可以确保在下一次播放开始前，无论DMA读到哪里，读到的都是静音。

	LOG_I(
		"[Driver] Filling entire TX buffer with silence to prevent looping "
		"noise.\r\n");

	// ★★★ 我们不再停止DMA，也不再操作信号量，因为这太危险了 ★★★
	// 我们只是默默地、安全地修改内存内容。

	// 1. 把整个物理缓冲区清零
	memset(tx_audio_buffer, 0, TX_BUFFER_SIZE);

	// 2. 刷新整个缓冲区的D-Cache，确保DMA能看到我们的修改
	bflb_l1c_dcache_clean_range((void *)tx_audio_buffer, TX_BUFFER_SIZE);

	// 我们甚至不需要重置 tx_write_block_idx。
	// 因为下一次 es8388_audio_play 被调用时，它会从 tx_write_block_idx
	// 当前的位置继续写入。
	// 而它前面的所有块，都已经被我们清零了，所以是绝对安全的。

	return 0;
}