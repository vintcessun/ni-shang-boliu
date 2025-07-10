#include "voice.h"
#include "bflb_i2s.h"
#include "bflb_dma.h"
#include "bflb_gpio.h"
#include "board.h"
#include "log.h"

#define DBG_TAG "VOICE"

static struct bflb_device_s *i2s_dev;
static struct bflb_device_s *dma_dev;
static voice_callback_t voice_cb = NULL;
static uint8_t audio_buffer[VOICE_BUFFER_SIZE];
static volatile uint8_t audio_ready = 0;

static void dma_callback(void *arg)
{
    audio_ready = 1;
}

void voice_init(voice_callback_t callback)
{
    /* Initialize I2S */
    i2s_dev = bflb_device_get_by_name("i2s0");
    bflb_i2s_init(i2s_dev, I2S_MODE_MASTER_RX, I2S_DATALENGTH_16BIT, I2S_FS_16000HZ);
    
    /* Initialize DMA */
    dma_dev = bflb_device_get_by_name("dma0_ch0");
    bflb_dma_init(dma_dev);
    bflb_dma_link_config(dma_dev, audio_buffer, VOICE_BUFFER_SIZE, DMA_MEMORY_TO_PERIPH, dma_callback, NULL);
    
    voice_cb = callback;
    LOG_I("Voice initialized");
}

void voice_start(void)
{
    /* Start I2S and DMA */
    bflb_i2s_rx_start(i2s_dev);
    bflb_dma_channel_start(dma_dev);
    LOG_I("Voice recording started");
}

void voice_stop(void)
{
    /* Stop I2S and DMA */
    bflb_dma_channel_stop(dma_dev);
    bflb_i2s_rx_stop(i2s_dev);
    LOG_I("Voice recording stopped");
}

void voice_process(void)
{
    if (audio_ready) {
        audio_ready = 0;
        
        // TODO: Implement actual speech recognition
        // For demo purposes, just return a fixed text
        if (voice_cb) {
            voice_cb("警察抓小偷游戏开始");
        }
        
        // Restart DMA for next buffer
        bflb_dma_channel_start(dma_dev);
    }
}

int text_to_speech(const char *text)
{
    LOG_I("TTS: %s", text);
    
    // TODO: Implement actual TTS synthesis
    // For demo, just play a beep sound
    uint16_t beep[16000];
    for(int i=0; i<16000; i++) {
        beep[i] = (i % 100 < 50) ? 0x7FFF : 0x8000;
    }
    
    // Play the beep via I2S
    bflb_i2s_tx_start(i2s_dev);
    bflb_i2s_send(i2s_dev, beep, sizeof(beep));
    bflb_i2s_tx_stop(i2s_dev);
    
    return 0;
}
