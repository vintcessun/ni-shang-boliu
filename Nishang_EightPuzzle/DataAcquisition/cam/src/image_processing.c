#include "image_processing.h"

#include <string.h>

// #include "neural_predict.h"

void predict_numbers(const uint8_t* yuv_data, int* predict) {
	// 初始化神经网络
	if (neural_predict_init() != 0) {
		memset(predict, -1, 9 * sizeof(int));
		printf("初始化错误\n");
		return;
	}

	// 分割240x240图像为9个80x80区域
	uint8_t cropped[80 * 80 * 2];
	for (int grid = 0; grid < 9; grid++) {
		int row = (grid / 3) * 80;
		int col = (grid % 3) * 80;

		// 提取每个80x80区域
		for (int y = 0; y < 80; y++) {
			for (int x = 0; x < 80; x++) {
				int src_idx = ((row + y) * 240 + (col + x)) * 2;
				int dst_idx = (y * 80 + x) * 2;
				cropped[dst_idx] = yuv_data[src_idx];
				cropped[dst_idx + 1] = yuv_data[src_idx + 1];
			}
		}

		// 预测当前区域
		predict[grid] = neural_predict(cropped);
	}
}
