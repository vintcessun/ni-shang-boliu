#include "bflb_cam.h"
#include "bflb_i2c.h"
#include "bflb_uart.h"
#include "board.h"
#include "http.h"
#include "image_sensor.h"
#include "neural_predict.h"
#include "rtos.h"

#define DEBUG 0

#define CROP_WQVGA_X (240)
#define CROP_WQVGA_Y (240)
#define CAM_BUFF_NUM (4)

extern char route[20];

#if (DEBUG == 1)
uint32_t j;
static struct bflb_device_s *uart0;
#endif

static struct bflb_device_s *cam0;
uint8_t *pic;
uint8_t predict_nums[9] = {0};
uint32_t pic_size;

void predict_numbers(const uint8_t *yuv_data, uint8_t *predict) {
	// 初始化神经网络
	if (neural_predict_init() != 0) {
		memset(predict, -1, 9 * sizeof(int));
		printf("初始化错误\n");
		return;
	}

	const uint32_t pic_size = 80 * 80 * 2;

	// 分割240x240图像为9个80x80区域
	uint8_t *cropped = pvPortMalloc((pic_size + 10) * sizeof(uint8_t));
	// 分割为3x3网格
	for (int grid_row = 0; grid_row < 3; grid_row++) {
		for (int grid_col = 0; grid_col < 3; grid_col++) {
			int sub_idx = grid_row * 3 + grid_col;

			// 计算当前子图在原图中的起始坐标
			int y_start = grid_row * 80;
			int x_start = grid_col * 80;

			// 逐行复制数据
			for (int y = 0; y < 80; y++) {
				// 原图中当前行的起始索引
				// 每行有240个像素，每个像素对应2字节(YUV422)
				int src_row_start = (y_start + y) * 240 * 2;
				// 子图中当前行的起始索引
				int sub_row_start = y * 80 * 2;

				// 复制当前行的80个像素
				for (int x = 0; x < 80; x++) {
					// 计算在原图中的偏移
					int src_offset = src_row_start + (x_start + x) * 2;
					// 计算在子图中的偏移
					int sub_offset = sub_row_start + x * 2;

					// 复制Y和UV分量
					cropped[sub_offset] = yuv_data[src_offset];	 // Y分量
					cropped[sub_offset + 1] =
						yuv_data[src_offset + 1];  // U或V分量
				}
			}

#if (DEBUG == 1)
			printf(
				"pop picture %d: 0x%08x, len: %d CROP_WQVGA_X: %d "
				"CROP_WQVGA_Y: %d\n",
				j, (uint32_t)cropped, pic_size, 80, 80);

			for (uint32_t c = 0; c < pic_size; c++)
				bflb_uart_putchar(uart0, cropped[c]);  // 串口发送视频图像到pc
#else
			// printf(
			//	"pop picture : 0x%08x, len: %d CROP_WQVGA_X: %d "
			//	"CROP_WQVGA_Y: %d\n",
			//	(uint32_t)cropped, pic_size, 80, 80);
#endif
			predict[grid_row * 3 + grid_col] = neural_predict(cropped);
		}
	}

	// 预测当前区域
	vPortFree(cropped);
}

void tflite_task(void *p) {
#if (DEBUG == 1)
	j = 0;
#endif
	char base_url[500] = {0};
	uint8_t response[500] = {0};
	sprintf(base_url, BASE_URL(cam) "?route=%s&data=", route);
	while (1) {
		if (bflb_cam_get_frame_count(cam0) > 0) {
			pic_size = bflb_cam_get_frame_info(cam0, &pic);
			bflb_cam_pop_one_frame(cam0);

			predict_numbers(pic, predict_nums);
			// printf("预测结果：\n");
			// printf("%d %d %d\n", predict_nums[0], predict_nums[1],
			//	   predict_nums[2]);
			// printf("%d %d %d\n", predict_nums[3], predict_nums[4],
			//	   predict_nums[5]);
			// printf("%d %d %d\n", predict_nums[6], predict_nums[7],
			//	   predict_nums[8]);
			char url[500] = {0};
			sprintf(url, "%s%d,%d,%d,%d,%d,%d,%d,%d,%d", base_url,
					predict_nums[0], predict_nums[1], predict_nums[2],
					predict_nums[3], predict_nums[4], predict_nums[5],
					predict_nums[6], predict_nums[7], predict_nums[8]);
			http_get(url, response, 500);
#if (DEBUG == 1)
			j++;
#endif
		}
	}
	vTaskDelete(NULL);
}

void tflite_main(void) {
	if (neural_predict_init() != 0) {
		printf("模型初始化失败\n");
		return;
	}

	static uint8_t picture[CROP_WQVGA_X * CROP_WQVGA_Y *
						   CAM_BUFF_NUM] ATTR_NOINIT_PSRAM_SECTION
		__attribute__((aligned(64)));

	static struct bflb_device_s *i2c0;
#if (DEBUG == 1)
	uart0 = bflb_device_get_by_name("uart0");
#endif

	struct bflb_cam_config_s cam_config;
	struct image_sensor_config_s *sensor_config;
	board_dvp_gpio_init();
	board_i2c0_gpio_init();

	i2c0 = bflb_device_get_by_name("i2c0");
	cam0 = bflb_device_get_by_name("cam0");

	if (image_sensor_scan(i2c0, &sensor_config)) {
		printf("\r\nSensor name: %s\r\n", sensor_config->name);
	} else {
		printf("\r\nError! Can't identify sensor!\r\n");
		while (1) {
		}
	}

	/* Crop resolution_x, should be set before init */
	bflb_cam_crop_hsync(cam0, 112, 112 + CROP_WQVGA_X);
	/* Crop resolution_y, should be set before init */
	bflb_cam_crop_vsync(cam0, 120, 120 + CROP_WQVGA_Y);

	memcpy(&cam_config, sensor_config, IMAGE_SENSOR_INFO_COPY_SIZE);
	cam_config.with_mjpeg = false;
	cam_config.input_source = CAM_INPUT_SOURCE_DVP;
	cam_config.output_format = CAM_OUTPUT_FORMAT_AUTO;
	cam_config.output_bufaddr = (uint32_t)picture;
	cam_config.output_bufsize =
		CROP_WQVGA_X * CROP_WQVGA_Y * (CAM_BUFF_NUM / 2);

	bflb_cam_init(cam0, &cam_config);
	bflb_cam_start(cam0);

	printf("cam lcd case\r\n");

	xTaskCreate(tflite_task, "TFLITE", 8192, NULL, tskIDLE_PRIORITY + 1, NULL);
}
