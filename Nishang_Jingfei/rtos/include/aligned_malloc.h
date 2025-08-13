#ifndef ALIGNED_MALLOC_H
#define ALIGNED_MALLOC_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

// 内部结构体：用于存储原始分配地址（在对齐后的内存块前）
typedef struct {
	void *original_ptr;	 // 保存pvPortMalloc返回的原始地址
} aligned_memory_header_t;

/**
 * @brief 分配指定大小和对齐要求的内存（封装FreeRTOS的pvPortMalloc）
 * @param size 要分配的内存大小（字节）
 * @param alignment 对齐要求（必须是2的幂，如4、8、16等）
 * @return 对齐后的内存指针（失败返回NULL）
 */
void *pvPortAlignedMalloc(size_t size, size_t alignment);

/**
 * @brief 释放由pvPortAlignedMalloc分配的内存
 * @param ptr 由pvPortAlignedMalloc返回的内存指针
 */
void vPortAlignedFree(void *ptr);

#endif	// ALIGNED_MALLOC_H
