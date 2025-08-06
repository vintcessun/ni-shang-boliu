#ifndef ES8388_DRIVER_H
#define ES8388_DRIVER_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "portmacro.h"	// 包含 FreeRTOS 的 portmacro.h 以使用 TickType_t 和其他类型

/**
 * @brief 音频模块初始化状态
 */
typedef enum {
	AUDIO_STATE_UNINITIALIZED,
	AUDIO_STATE_INITIALIZED,
	AUDIO_STATE_CAPTURING
} audio_module_state_t;

/**
 * @brief 初始化 ES8388 音频编解码器及相关I2S和DMA。
 *
 * @return int 0 表示成功, 非 0 表示失败。
 */
int es8388_audio_init(void);

/**
 * @brief 启动音频采集。
 *
 * @return int 0 表示成功, 非 0 表示失败。
 */
int es8388_audio_start_capture(void);

/**
 * @brief 停止音频采集。
 *
 * @return int 0 表示成功, 非 0 表示失败。
 */
int es8388_audio_stop_capture(void);  // 确保此名称与 .c 文件中的定义一致

/**
 * @brief 反初始化音频模块，释放资源。
 */
void es8388_audio_deinit(void);

/**
 * @brief 获取当前音频模块状态。
 *
 * @return audio_module_state_t 当前状态。
 */
audio_module_state_t es8388_audio_get_state(void);

/**
 * @brief 获取最近捕获的音频数据。
 *
 * @param buffer 指向目标缓冲区的指针，用于复制音频数据。
 * @param buffer_size 目标缓冲区的最大长度。
 * @param out_len [OUT] 实际复制到目标缓冲区的音频数据长度。
 * @param timeout_ticks 超时等待的时间（单位：ticks）。
 * @return int 0 表示成功获取数据，非 0 表示失败。
 */
int es8388_audio_get_data(uint8_t *buffer, uint32_t buffer_size,
						  uint32_t *out_len,
						  TickType_t timeout_ticks);  // 增加超时参数

/**
 * @brief Plays a chunk of audio data.
 * @param data Pointer to the audio data buffer.
 * @param len Length of the audio data in bytes.
 * @return 0 on success, negative error code on failure.
 */
int es8388_audio_play(const uint8_t *data, uint32_t len);

/**
 * @brief Fills the upcoming DMA audio buffers with silence.
 *        This function should be called when transitioning from playback back
 * to recording to prevent old audio data from being looped.
 * @return 0 on success, negative error code on failure.
 */
int es8388_audio_fill_silence(void);

#endif	// ES8388_DRIVER_H
