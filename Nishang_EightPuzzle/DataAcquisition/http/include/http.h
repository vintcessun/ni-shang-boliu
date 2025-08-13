#ifndef __HTTP_H_
#define __HTTP_H_

#define SERVER "192.168.202.89"

#define BASE_URL(path) "http://" SERVER "/api/" #path
#define fast_api(x) http_get(BASE_URL(x), response, 500)
#define audio_play(x) fast_api(audio?data=x)

int http_main(void);
int http_get(const char *url, uint8_t *response, size_t max_len);

#endif	//__HTTP_H_
