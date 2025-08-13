#include "bflb_mtimer.h"
#include "game.h"
#include "http.h"
#include "stdbool.h"
#include "stdint.h"

#define DBG_TAG "GAME"
#include "log.h"
#include "rtos.h"

bool map[8][8];
int ir[8][8];
int add_queen[8][2];
int add_queen_cnt;
int remove_queen[8][2];
int remove_queen_cnt;
int queen[16][2];
int queen_cnt;
bool queen_mask[16];
bool task_flag;

extern char route[20];
extern bool WIFI_CONNECTED;

/*
 01234567
X........8
W........9
V........A
U........B
T........C
S........D
R........E
Q........F
P........G
 ONMLKJIH
从 0 - 9 代表 0号 - 9号霍尔
从 A - X 代表 10号 - 31号霍尔
*/

static inline void reset_map() {
	memset(ir, 0, sizeof(ir));
	memset(add_queen, 0, sizeof(add_queen));
	add_queen_cnt = 0;
	memset(remove_queen, 0, sizeof(remove_queen));
	remove_queen_cnt = 0;
	memset(queen, 0, sizeof(queen));
	queen_cnt = 0;
}

static inline void queen_add(int x, int y) {
	if (add_queen_cnt >= 8) {
		return;
	}
	add_queen[add_queen_cnt][0] = x;
	add_queen[add_queen_cnt][1] = y;
	add_queen_cnt++;
}

static inline void queen_remove(int x, int y) {
	if (remove_queen_cnt >= 8) {
		return;
	}
	remove_queen[remove_queen_cnt][0] = x;
	remove_queen[remove_queen_cnt][1] = y;
	remove_queen_cnt++;
}

static inline void queen_set(int x, int y) {
	if (queen_cnt >= 16) {
		return;
	}
	queen[queen_cnt][0] = x;
	queen[queen_cnt][1] = y;
	queen_mask[queen_cnt] = true;
	queen_cnt++;
}

static inline void ir2map() {
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			if (ir[i][j] == 2) {
				if (!map[i][j]) {
					queen_add(i, j);
				}
				queen_set(i, j);
			} else {
				if (map[i][j]) {
					queen_remove(i, j);
				}
			}
		}
	}
}

static inline void update_ir(bool value, int irx) {
	if (irx < 0) {
		return;
	}
	if (irx < 8) {
		for (int i = 0; i < 8; i++) {
			ir[i][irx] += value;
		}
		return;
	}
	if (irx < 16) {
		for (int i = 0; i < 8; i++) {
			ir[irx - 8][i] += value;
		}
		return;
	}
	if (irx < 24) {
		for (int i = 0; i < 8; i++) {
			ir[7 - (irx - 16)][i] += value;
		}
		return;
	}
	if (irx < 32) {
		for (int i = 0; i < 8; i++) {
			ir[i][7 - (irx - 24)] += value;
		}
		return;
	}
	return;
}
char response[10240];

static inline void send_queen_data() {
	char base_url[500];
	char url[1000];
	char data[50];
	char action[50];
	sprintf(base_url, BASE_URL(game) "?route=%s&data=", route);
	for (int i = 0; i < remove_queen_cnt; i++) {
		char path[500] = {0};
		int cnt = 0;
		for (int j = 0; j < queen_cnt; j++) {
			if (!queen_mask[j]) continue;
			if (queen[j][0] == remove_queen[i][0] &&
				queen[j][1] == remove_queen[i][1]) {
				queen_mask[j] = false;
				map[remove_queen[i][0]][remove_queen[i][1]] = false;
				sprintf(action, "%d,%d,1", remove_queen[i][0],
						remove_queen[i][1]);
			} else {
				sprintf(data, "%d,%d,", queen[j][0], queen[j][1]);
				strcat(path, data);
				cnt++;
			}
		}
		sprintf(url, "%s%d,%s%s", base_url, cnt, path, action);
		http_get(url, response, 10240);
		audio_play(response);
		bflb_mtimer_delay_ms(500);
	}
	for (int i = 0; i < add_queen_cnt; i++) {
		char path[500] = {0};
		int cnt = 0;
		for (int j = 0; j < queen_cnt; j++) {
			if (!queen_mask[j]) continue;
			if (queen[j][0] == add_queen[i][0] &&
				queen[j][1] == add_queen[i][1]) {
				sprintf(action, "%d,%d,0", add_queen[i][0], add_queen[i][1]);
				queen[queen_cnt][0] = add_queen[i][0];
				queen[queen_cnt][1] = add_queen[i][1];
				queen_mask[queen_cnt] = true;
				map[add_queen[i][0]][add_queen[i][1]] = true;
			} else {
				sprintf(data, "%d,%d,", queen[j][0], queen[j][1]);
				strcat(path, data);
				cnt++;
			}
		}
		queen_cnt++;
		sprintf(url, "%s%d,%s%s", base_url, cnt, path, action);
		http_get(url, response, 500);
		bflb_mtimer_delay_ms(500);
	}
}

static inline void send_ir_data(bool *data) {
	char response[500];
	char url[500];
	sprintf(url, BASE_URL(infrared) "?route=%s&data="
    "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
    route,data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15],
            data[16], data[17], data[18], data[19], data[20], data[21], data[22], data[23],
            data[24], data[25], data[26], data[27], data[28], data[29], data[30], data[31]);
	http_get(url, response, 500);
	bflb_mtimer_delay_ms(500);
	LOG_I("[GAME] Sent IR data Response: %s\n", response);
}

void send_data(bool *init_data) {
	if (!WIFI_CONNECTED) return;
	send_ir_data(init_data);
	reset_map();
	for (int i = 0; i < 32; i++) {
		update_ir(init_data[i], i);
	}
	ir2map();
	send_queen_data();
}

static void send_data_handler(void *data) {
	bool *init_data = (bool *)data;
	send_data(init_data);
	vTaskDelete(NULL);
}

void send_data_task(bool *data) {
	xTaskCreate(send_data_handler, (char *)"GAME_SEND_DATA", 1024, data,
				tskIDLE_PRIORITY + 1, NULL);
}