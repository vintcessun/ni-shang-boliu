#ifndef __RTOS_H__
#define __RTOS_H__

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "aligned_malloc.h"
#include "mem.h"
#include "task.h"
#include "timers.h"

typedef void (*rtos_task_func_t)(void *);

TaskHandle_t rtos_create_lcd_task(rtos_task_func_t func, void *arg);
TaskHandle_t create_hall_task(rtos_task_func_t func, void *arg);
TaskHandle_t create_hall_network_task(rtos_task_func_t func, void *arg);

#endif
