#include "bflb_mtimer.h"
#include "board.h"

#define DBG_TAG "MAIN"
#include "auto_connect.h"
#include "cam.h"
#include "http.h"
#include "image_processing.h"
#include "log.h"
#include "rtos.h"

char route[20];
bool WIFI_CONNECTED;
extern uint8_t *pic_cut;

static void main_task(void *p) {
	int predict_nums[9] = {0};

	bool state = true;
	while (state) {
		if (pic_cut != NULL) {
			predict_numbers(pic_cut, &predict_nums);
			printf("预测结果：\n");
			printf("%d %d %d\n", predict_nums[0], predict_nums[1],
				   predict_nums[2]);
			printf("%d %d %d\n", predict_nums[3], predict_nums[4],
				   predict_nums[5]);
			printf("%d %d %d\n", predict_nums[6], predict_nums[7],
				   predict_nums[8]);
			for (int i = 0; i < 9; i++) {
				if (predict_nums[i] == -1) {
					state = false;
				}
			}
			// break;
		} else {
			LOG_I("Waiting for WIFI\n");
			bflb_mtimer_delay_ms(1000);
		}
	}
}

int main(void) {
	board_init();
	cam_main();
	http_main();
	auto_connect_init("HONOR", "8888888888");

	// xTaskCreate(main_task, "MAIN_TASK", 1024 * 4, NULL, tskIDLE_PRIORITY + 1,
	//			NULL);

	vTaskStartScheduler();

	while (1) {
	}
}
