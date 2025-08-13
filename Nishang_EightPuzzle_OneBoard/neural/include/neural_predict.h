#ifndef NEURAL_PREDICT_H
#define NEURAL_PREDICT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化神经网络预测器
 * @return 0成功，其他失败
 */
int neural_predict_init(void);

/**
 * @brief 使用神经网络进行预测
 * @param yuv_data 输入的YUV422数据(80x80x2)
 * @return 预测结果(0-9)
 */
int neural_predict(const uint8_t* yuv_data);

#ifdef __cplusplus
}
#endif

#endif	// NEURAL_PREDICT_H
