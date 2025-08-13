#include "rtos.h"

#include "http.h"
#define DBG_TAG "RTOS"
#include "log.h"

TaskHandle_t rtos_create_lcd_task(rtos_task_func_t func, void *arg) {
	TaskHandle_t handle = NULL;
	UBaseType_t priority = tskIDLE_PRIORITY + 1;

	if (xTaskCreate(func, "LCD_Task", 2048, arg, priority, &handle) != pdPASS) {
		LOG_I("[RTOS] Failed to create LCD task with priority %d\n", priority);
		return NULL;
	}

	LOG_I("[RTOS] Created LCD task with priority %d\n", priority);
	return handle;
}

TaskHandle_t create_hall_task(rtos_task_func_t func, void *arg) {
	TaskHandle_t handle = NULL;
	UBaseType_t priority = tskIDLE_PRIORITY + 1;

	if (xTaskCreate(func, "HALL_Task", 1024 * 4, arg, priority, &handle) !=
		pdPASS) {
		LOG_I("[RTOS] Failed to create HALL task with priority %d\n", priority);
		return NULL;
	}

	LOG_I("[RTOS] Created HALL task with priority %d\n", priority);
	return handle;
}

TaskHandle_t create_hall_network_task(rtos_task_func_t func, void *arg) {
	TaskHandle_t handle = NULL;
	UBaseType_t priority = tskIDLE_PRIORITY + 1;

	if (xTaskCreate(func, "HALL_Network_Task", 1024, arg, priority, &handle) !=
		pdPASS) {
		LOG_I("[RTOS] Failed to create HALL task with priority %d\n", priority);
		return NULL;
	}

	LOG_I("[RTOS] Created HALL task with priority %d\n", priority);
	return handle;
}
