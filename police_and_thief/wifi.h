#ifndef __WIFI_H__
#define __WIFI_H__

#include "FreeRTOS.h"
#include "task.h"
#include "lwip/tcpip.h"
#include "lwip/sockets.h"
#include "wifi_mgmr.h"

#define WIFI_STACK_SIZE  (1536)
#define WIFI_TASK_PRIORITY (16)

typedef void (*wifi_event_cb_t)(uint32_t code);

int wifi_init(wifi_event_cb_t event_cb);
int wifi_connect(const char *ssid, const char *passwd);
int wifi_disconnect(void);
int wifi_http_request(const char *url, char *response, int max_len);

#endif
