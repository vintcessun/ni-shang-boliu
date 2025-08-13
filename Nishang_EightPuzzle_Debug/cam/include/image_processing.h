#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <stdint.h>

/**
 * @brief 预测九宫格数字
 * @param yuv_data 240x240x2 YUV422图像数据
 * @param predict 输出预测结果数组(9个元素)
 */
void predict_numbers(const uint8_t* yuv_data, int* predict);

#endif	// IMAGE_PROCESSING_H
