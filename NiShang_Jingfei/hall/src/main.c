#include "bflb_gpio.h"
#include "board.h"
#include "hall.h"
#include "http.h"
#define DBG_TAG "HALL"
#include "log.h"
#include "rtos.h"

#define j0 25
#define j1 26
#define j2 27
#define j3 28
#define f3 29
#define f2 30
#define f1 31
#define f0 32

struct bflb_device_s *gpio;
extern bool gpio_data[8];
bool last_gpio[8];
extern int j[3];
extern int f[3];
extern int state;
extern int lr;
extern char route[20];
extern bool WIFI_CONNECTED;

void read_hall_sensor() {
	for (int i = 25; i <= 32; i++) {
		gpio_data[get_gpio_pos(i)] = !bflb_gpio_read(gpio, i);
		// LOG_I("[HALL] GPIO %d state: %x\n", i, gpio_data[i-25]);
	}
}

static void hall_update_task(void *p) {
	while (!WIFI_CONNECTED) {
		LOG_I("[HTTP] Waiting for WIFI\n");
		bflb_mtimer_delay_ms(1000);
	}
	while (strlen(route) < 16) {
		bflb_mtimer_delay_ms(1000);
	}
	LOG_I("[HTTP] Hall Sync Task Start");
	char base_url[100] = BASE_URL(hall?route=);
	sprintf(base_url, "%s%s&data=", base_url, route);
	char url[100] = {0};
	strcpy(url, base_url);
	char response[500] = {0};
	bool gpio_data_last[8] = {0};
	while (1) {
		bool dirty = false;
		for (int i = 0; i < 8; i++) {
			dirty = dirty || gpio_data_last[i] != gpio_data[i];
		}
		if (dirty) {
			LOG_I("[SYS] Memory left is %d Bytes\n", kfree_size());
			strcpy(url, base_url);
			for (int i = 0; i < 8; i++) {
				sprintf(url, "%s%d,", url, gpio_data[i]);
			}
			LOG_D("[HALL] Hall Heart To %s\n", url);
			http_get(url, response, 500);
			LOG_D("[HALL] Hall Update Response: %s\n", response);
			if (j[2] + f[2] == 6) {
				LOG_I("[AUDIO] 游戏结束，请到平台上生成测评报告。\n");
				audio_play(
					"%E6%B8%B8%E6%88%8F%E7%BB%93%E6%9D%9F%EF%BC%8C%E8%AF%B7%E5%"
					"88%B0%E5%B9%B3%E5%8F%B0%E4%B8%8A%E7%94%9F%E6%88%90%E6%B5%"
					"8B%E8%AF%84%E6%8A%A5%E5%91%8A%E3%80%82");
				break;
			}
			memcpy(gpio_data_last, gpio_data, sizeof(gpio_data));
		}
		bflb_mtimer_delay_ms(50);
	}
	vTaskDelete(NULL);
}

static void hall_network_task(void *pvParameters) {
	LOG_I("[GPIO] Hall Update Start\n");
	while (1) {
		read_hall_sensor();
	}
	vTaskDelete(NULL);
}

static void hall_task(void *pvParameters) {
	bool j0_state = true, j3_state = true;
	int middle_state_j = 0;
	int current_nums_j = 0, last_nums_j = 0;
	bool click_j0 = true, click_j3 = true;
	bool double_j = false;

	bool f0_state = true, f3_state = true;
	int middle_state_f = 0;
	int current_nums_f = 0, last_nums_f = 0;
	bool click_f0 = true, click_f3 = true;
	while (1) {
		// printf("%d", get_gpio(j0));
		// LOG_I("Hall J0 is %x\n", get_gpio(j0));
		if (get_gpio(j0) == 1) {
			// LOG_I("J0 State is %x\n", j0_state);
			if (j0_state) {
				LOG_I("J0 is clicked\n");
				j0_state = false;
				middle_state_j = 1;
				click_j0 = false;
			} else if (click_j0) {
				LOG_I("J0 is clicked twice\n");
				j0_state = true;
				middle_state_j = 3;
			}
		} else {
			click_j0 = true;
		}
		if (get_gpio(j3) == 1) {
			if (j3_state) {
				LOG_I("J3 is clicked\n");
				j3_state = false;
				middle_state_j = 2;
			} else if (click_j3) {
				LOG_I("J3 is clicked twice\n");
				j3_state = true;
				middle_state_j = 4;
				click_j3 = false;
			}
		} else {
			click_j3 = true;
		}
		if (j[1] == 2) {
			double_j = true;
		}
		current_nums_j = get_gpio(j1) + get_gpio(j2);
		if (current_nums_j > last_nums_j) {
			if (middle_state_j != 0) {
				LOG_I("bigger middle_state=%d\n", middle_state_j);
			}
			if (middle_state_j == 1) {
				j[0]--;
				j[1]++;
				middle_state_j = 0;
				j0_state = true;
				last_nums_j = current_nums_j;
			} else if (middle_state_j == 2) {
				j[1]++;
				j[2]--;
				middle_state_j = 0;
				j3_state = true;
				last_nums_j = current_nums_j;
			} else if (middle_state_j == 3) {
				j[1]--;
				j[2]++;
				middle_state_j = 0;
				j0_state = true;
				last_nums_j = current_nums_j;
			}
		} else if (current_nums_j < last_nums_j) {
			if (middle_state_j != 0) {
				LOG_I("smaller middle_state=%d\n", middle_state_j);
			}
			if (middle_state_j == 1) {
				j[0]++;
				j[1]--;
				middle_state_j = 0;
				j0_state = true;
				last_nums_j = current_nums_j;
			} else if (middle_state_j == 2) {
				j[1]--;
				j[2]++;
				middle_state_j = 0;
				j3_state = true;
				last_nums_j = current_nums_j;
			} else if (middle_state_j == 3) {
				j[0]++;
				j[1]--;
				middle_state_j = 0;
				j0_state = true;
				last_nums_j = current_nums_j;
			}
		}

		if (get_gpio(f0) == 1) {
			// LOG_I("F0 State is %x\n", f0_state);
			if (f0_state) {
				LOG_I("F0 is clicked\n");
				f0_state = false;
				middle_state_f = 1;
				click_f0 = false;
			} else if (click_f0) {
				LOG_I("F0 is clicked twice\n");
				f0_state = true;
				middle_state_f = 3;
			}
		} else {
			click_f0 = true;
		}
		if (get_gpio(f3) == 1) {
			if (f3_state) {
				LOG_I("F3 is clicked\n");
				f3_state = false;
				middle_state_f = 2;
			} else if (click_f3) {
				LOG_I("F3 is clicked twice\n");
				f3_state = true;
				middle_state_f = 4;
				click_f3 = false;
			}
		} else {
			click_f3 = true;
		}
		current_nums_f = get_gpio(f1) + get_gpio(f2);
		if (current_nums_f > last_nums_f) {
			if (middle_state_f != 0) {
				LOG_I("bigger middle_state=%d\n", middle_state_f);
			}
			if (middle_state_f == 1) {
				f[0]--;
				f[1]++;
				middle_state_f = 0;
				f0_state = true;
				last_nums_f = current_nums_f;
			} else if (middle_state_f == 2) {
				f[1]++;
				f[2]--;
				middle_state_f = 0;
				f3_state = true;
				last_nums_f = current_nums_f;
			} else if (middle_state_f == 3) {
				f[1]--;
				f[2]++;
				middle_state_f = 0;
				f0_state = true;
				last_nums_f = current_nums_f;
			}
		} else if (current_nums_f < last_nums_f) {
			if (middle_state_f != 0) {
				LOG_I("smaller middle_state=%d\n", middle_state_f);
			}
			if (middle_state_f == 1) {
				f[0]++;
				f[1]--;
				middle_state_f = 0;
				f0_state = true;
				last_nums_f = current_nums_f;
			} else if (middle_state_f == 2) {
				f[1]--;
				f[2]++;
				middle_state_f = 0;
				f3_state = true;
				last_nums_f = current_nums_f;
			} else if (middle_state_f == 3) {
				f[0]++;
				f[1]--;
				middle_state_f = 0;
				f0_state = true;
				last_nums_f = current_nums_f;
			}
		}
		// bflb_mtimer_delay_ms(500);
		// LOG_I("[HALL] Send A Game\n");
		// LOG_I("[HALL] Police state: %d, %d, %d\n", j[0], j[1], j[2]);
		// LOG_I("[HALL] Thief state: %d, %d, %d\n", f[0], f[1], f[2]);
	}
	vTaskDelete(NULL);
}

int hall_main(void) {
	// 硬件初始化(只执行一次)
	gpio = bflb_device_get_by_name("gpio");
	LOG_I("[HALL] Initializing GPIO25-32 as input...\n");

	// 配置GPIO25-32为输入模式
	for (int i = 25; i <= 32; i++) {
		bflb_gpio_init(gpio, i,
					   GPIO_INPUT | GPIO_PULLDOWN | GPIO_SMT_EN | GPIO_DRV_0);
	}

	// 创建霍尔传感器任务

	TaskHandle_t hall_task_handle = create_hall_task(hall_task, NULL);
	if (hall_task_handle == NULL) {
		LOG_E("[HALL] Error: Failed to create hall task\n");
		return -1;
	}  //*/

	// 创建霍尔监听网络服务

	TaskHandle_t hall_task_network_handle =
		create_hall_network_task(hall_network_task, NULL);
	if (hall_task_network_handle == NULL) {
		LOG_E("[HALL] Error: Failed to create hall network task\n");
		return -1;
	}  //*/

	TaskHandle_t hall_task_sync =
		xTaskCreate(hall_update_task, "HALL_SYNC_TASK", 1024 * 4, NULL,
					tskIDLE_PRIORITY + 1, NULL);
	if (hall_task_sync == NULL) {
		LOG_E("[HALL] Error: Failed to create hall sync task\n");
		return -1;
	}  //*/

	return 0;
}
