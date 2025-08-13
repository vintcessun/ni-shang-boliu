#ifndef __RTOS_H__
#define __RTOS_H__

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "mem.h"
#include "task.h"
#include "timers.h"

typedef void (*rtos_task_func_t)(void *);

/**
 * @brief 创建LCD显示任务
 * @param func 任务函数
 * @param arg 任务参数
 * @return 任务句柄，创建失败返回NULL
 */
TaskHandle_t rtos_create_lcd_task(rtos_task_func_t func, void *arg);
TaskHandle_t create_hall_task(rtos_task_func_t func, void *arg);
TaskHandle_t create_hall_network_task(rtos_task_func_t func, void *arg);

#endif
