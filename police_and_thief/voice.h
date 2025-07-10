#ifndef __VOICE_H__
#define __VOICE_H__

#include "stdint.h"
#include "bflb_i2s.h"
#include "bflb_dma.h"

#define VOICE_SAMPLE_RATE 16000
#define VOICE_BUFFER_SIZE 1024

typedef void (*voice_callback_t)(const char *text);

void voice_init(voice_callback_t callback);
void voice_start(void);
void voice_stop(void);
int text_to_speech(const char *text);

#endif
