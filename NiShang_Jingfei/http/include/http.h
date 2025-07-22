#ifndef __HTTP_H_
#define __HTTP_H_

#define SERVER "192.168.202.89"
#define SSID "HONOR"
#define PASSWD "8888888888"

#define BASE_URL(path) "http://" SERVER "/api/" #path
#define fast_api(x) http_get(BASE_URL(x), response, 500)
#define audio_play(x) fast_api(audio?data=x)

#define WIFI                                                  \
	uint8_t cmd[] = "wifi_sta_connect " SSID " " PASSWD "\n"; \
	shell_exe_cmd(cmd, sizeof(cmd) / sizeof(uint8_t));

int http_main(void);
int http_get(const char *url, uint8_t *response, size_t max_len);

#endif	//__HTTP_H_
