#include "bflb_mtimer.h"
#include "board.h"
#include "lcd.h"
#include "lcd_conf_user.h"
#include "lcd_task.h"
#include "rtos.h"
#include "touch.h"
#include "touch_conf_user.h"
#define DBG_TAG "LCD"
#include "hall.h"
#include "http.h"
#include "log.h"

extern bool lr;
extern int j[3];
extern int f[3];
extern struct bflb_device_s *gpio;
extern bool gpio_data[8];
extern char route[20];
extern bool WIFI_CONNECTED;

touch_coord_t touch_max_point = {
	.coord_x = 240,
	.coord_y = 320,
};

uint8_t response[10240];

static void src2tgt_task(void *p) {
	sg_src_down();
	bflb_mtimer_delay_ms(500);
	sg_target_up();
	vTaskDelete(NULL);
}

static void tgt2src_task(void *p) {
	sg_target_down();
	bflb_mtimer_delay_ms(500);
	sg_src_up();
	vTaskDelete(NULL);
}

static void lcd_task(void *pvParameters) {
	lcd_draw_rectangle(85, 145, 155, 175, 0xFFFF);
	lcd_draw_str_ascii16(95, 150, 0xFFFF, 0x0000, (uint8_t *)"cross", 7);
	lcd_draw_str_ascii16(0, 0, 0xFFFF, 0X0000,
						 (uint8_t *)"       LEFT   RIVER    RIGHT", 30);
	static int16_t last_x = 0;
	static int16_t last_y = 0;
	uint8_t point_num = 0;
	touch_coord_t touch_coord;

	while (!WIFI_CONNECTED) {
		LOG_I("[LCD] Waiting for WIFI\n");
		bflb_mtimer_delay_ms(1000);
	}
	while (1) {
		touch_read(&point_num, &touch_coord, 1);
		if (point_num) {
			last_x = touch_coord.coord_x;
			last_y = touch_coord.coord_y <= 160 ? touch_coord.coord_y + 160
												: touch_coord.coord_y - 160;
			LOG_I("[LCD] Touched x = %d y= %d\n", last_x, last_y);
			if (last_x > 100 && last_x < 400 && last_y > 100 && last_y < 400) {
				if (j[1] + f[1] > 2) {
					LOG_I("[VOICE] 小朋友，每次只能两个人同时过河哦\n");
					audio_play(
						"%E5%B0%8F%E6%9C%8B%E5%8F%8B%EF%BC%8C%E6%AF%8F%E6%AC%"
						"A1%E5%8F%AA%E8%83%BD%E4%B8%A4%E4%B8%AA%E4%BA%BA%E5%90%"
						"8C%E6%97%B6%E8%BF%87%E6%B2%B3%E5%93%A6");
				} else {
					LOG_I("[VOICE] 开始过河\n");
					audio_play("%E5%BC%80%E5%A7%8B%E8%BF%87%E6%B2%B3");
					char url[100] = BASE_URL(game?route=);
					if (!lr) {
						sprintf(url, "%s%s&data=%d,%d,%d,%d,%d,%d,%d", url,
								route, j[2], f[2], j[0] + j[1], f[0] + f[1],
								j[1], f[1], lr);
					} else {
						sprintf(url, "%s%s&data=%d,%d,%d,%d,%d,%d,%d", url,
								route, j[2] + j[1], f[2] + f[1], j[0], f[0],
								j[1], f[1], lr);
					}
					http_get(url, response, 10240);
					sprintf(url,"%s%s&data=%s",BASE_URL(direction?route=),route,response);
					http_get(url, response, 10240);
					audio_play(response);
					if (!lr) {
						xTaskCreate(src2tgt_task, (char *)"src2tgt", 1024, NULL,
									tskIDLE_PRIORITY + 5, NULL);
					} else {
						xTaskCreate(tgt2src_task, (char *)"tgt2src", 1024, NULL,
									tskIDLE_PRIORITY + 5, NULL);
					}
					lr = !lr;
				}
				int cnt = 0;
				while (cnt < 3) {
					while (point_num) {
						touch_read(&point_num, &touch_coord, 1);
					}
					cnt++;
					while (!point_num) {
						touch_read(&point_num, &touch_coord, 1);
					}
				}
			}
		}
		char buf[40] = {0};
		// lcd_clear(0x0000);
		sprintf(buf, "Police  %d       %d        %d  ", j[0], j[1], j[2]);
		lcd_draw_str_ascii16(0, 30, 0xFFFF, 0x0000, (uint8_t *)buf, 32);
		sprintf(buf, "Thief   %d       %d        %d  ", f[0], f[1], f[2]);
		lcd_draw_str_ascii16(0, 50, 0xFFFF, 0x0000, (uint8_t *)buf, 32);
		if (lr)
			sprintf(buf, "State:      To Left      ");
		else
			sprintf(buf, "State:      To Right     ");
		lcd_draw_str_ascii16(0, 80, 0xFFFF, 0x0000, (uint8_t *)buf, 32);

		bflb_mtimer_delay_ms(100);
	}

	vTaskDelete(NULL);
}

extern bool WIFI_CONNECTED;

int lcd_main(void) {
	printf("Start LCD MAIN\n");
	touch_init(&touch_max_point);
	lcd_init();
	lcd_clear(0x0000);

	// 确保任务优先级不超过configMAX_PRIORITIES
	TaskHandle_t lcd_task_handle = rtos_create_lcd_task(lcd_task, NULL);

	if (lcd_task_handle == NULL) {
		printf("Error: LCD_TASK_CREATE");
	}

	return 0;
}
