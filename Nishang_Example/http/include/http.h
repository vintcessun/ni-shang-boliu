#ifndef __HTTP_H_
#define __HTTP_H_

#include "audio.h"
#include "rtos.h"

#define SERVER "192.168.219.89"

#define BASE_URL(path) "http://" SERVER "/api/" #path
#define fast_api(x) \
	http_get(BASE_URL(x), response, sizeof(response) / sizeof(response[0]))
#define audio_play(x) play_audio(x)
#define BASE_WS(path) "ws://" SERVER "/api/" #path "/ws"

int http_main(void);
int http_get(const char *url, uint8_t *response, size_t max_len);

#endif	//__HTTP_H_
