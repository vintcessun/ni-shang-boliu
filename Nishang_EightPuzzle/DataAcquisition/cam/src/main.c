#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#define DBG_TAG "CAM"

#include "bflb_cam.h"
#include "bflb_dma.h"
#include "bflb_i2c.h"
#include "bflb_irq.h"
#include "bflb_mtimer.h"
#include "bflb_uart.h"
#include "bl616_glb.h"
#include "board.h"
#include "cam.h"
#include "image_processing.h"
#include "image_sensor.h"
#include "lcd.h"
#include "lcd_conf_user.h"
#include "log.h"
#include "rtos.h"

#define CROP_WQVGA_X (240)
#define CROP_WQVGA_Y (240)
#define CAM_BUFF_NUM (4)

extern bool WIFI_CONNECTED;
int send_yuv422_to_pc(uint8_t *yuv_buf, uint32_t yuv_size,
					  const char *server_ip, int server_port) {
	LOG_I("Create TCP socket\n");
	// 1. 创建TCP socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("socket create failed\n");
		return -1;
	}
	LOG_I("Create TCP socket End\n");

	LOG_I("Connect to PC\n");
	// 2. 连接上位机服务器
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
		printf("invalid server ip\n");
		close(sockfd);
		return -1;
	}
	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
		0) {
		printf("connect failed\n");
		close(sockfd);
		return -1;
	}
	LOG_I("Connect to PC End\n");

	LOG_I("Make HTTP POST HEADER\n");
	// 3. 构造HTTP POST请求头（关键：指定Content-Length为YUV数据大小）
	char http_header[1024];
	snprintf(http_header, sizeof(http_header),
			 "POST /receive_yuv HTTP/1.1\r\n"
			 "Host: %s:%d\r\n"
			 "Content-Type: application/octet-stream\r\n"  // 二进制数据类型
			 "Content-Length: %d\r\n"	   // 必须等于YUV数据大小
			 "Connection: close\r\n\r\n",  // 空行分隔头和数据
			 server_ip, server_port, yuv_size);
	LOG_I("Make HTTP POST End\n");

	LOG_I("Send data\n");
	// 4. 发送请求头 + YUV数据
	// 先发送HTTP头
	if (send(sockfd, http_header, strlen(http_header), 0) < 0) {
		printf("send header failed\n");
		close(sockfd);
		return -1;
	}
	LOG_I("Send header End\n");
	// 再发送YUV二进制数据（uint8_t* buf）
	uint32_t sent = 0;
	while (sent < yuv_size) {
		uint32_t chunk_size =
			(yuv_size - sent) > 4096 ? 4096 : (yuv_size - sent);
		ssize_t ret = send(sockfd, yuv_buf + sent, chunk_size, 0);
		if (ret < 0) {
			printf("send failed at chunk %d\n", sent);
			close(sockfd);
			return -1;
		}
		sent += ret;
	}
	LOG_I("Send data End\n");

	// 5. 关闭连接
	close(sockfd);
	return 0;
}

void cam_task(void *p) {
	while (!WIFI_CONNECTED) {
		LOG_I("Waiting for WIFI\n");
		bflb_mtimer_delay_ms(1000);
	}
	// cam
	uint8_t *pic;
	static uint8_t picture[CROP_WQVGA_X * CROP_WQVGA_Y *
						   CAM_BUFF_NUM] ATTR_NOINIT_PSRAM_SECTION
		__attribute__((aligned(64)));

	// Cam test
	static struct bflb_device_s *i2c0;
	static struct bflb_device_s *cam0;

	uint32_t i, j, pic_size;
	struct bflb_cam_config_s cam_config;
	struct image_sensor_config_s *sensor_config;

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

	lcd_set_dir(2, 0);	// 显示旋转180度。

	printf("cam lcd case\r\n");

	j = 0;

	int predict_nums[9] = {0};
	bool state = true;

	// if (neural_predict_init() != 0) {
	//	printf("模型初始化失败\n");
	//	while (1);
	// }
	while (state) {
		// CAM test
		if (bflb_cam_get_frame_count(cam0) > 0) {
			// bflb_cam_stop(cam0);
			pic_size = bflb_cam_get_frame_info(cam0, &pic);
			bflb_cam_pop_one_frame(cam0);
			j++;
			send_yuv422_to_pc(pic, 240 * 240 * 2, "192.168.91.89", 8888);
			/*
			predict_numbers(pic, &predict_nums);
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
			//*/
		}
	}

	vTaskDelete(NULL);
}

int cam_main(void) {
	lcd_init();
	board_dvp_gpio_init();
	board_i2c0_gpio_init();
	xTaskCreate(cam_task, (char *)"CAM_TASK", 4096, NULL, tskIDLE_PRIORITY + 1,
				NULL);
}
