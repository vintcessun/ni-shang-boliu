#ifndef NEURAL_MODEL_H
#define NEURAL_MODEL_H

#include <stddef.h>
#include <stdint.h>

/* 数字识别模型数据 */
extern const uint8_t neural_model_data[];
extern const size_t neural_model_size;

/* 获取模型数据和大小的函数 */
const uint8_t* get_neural_model_data(void);
size_t get_neural_model_size(void);

#endif	// NEURAL_MODEL_H
