#include "aligned_malloc.h"

#include <stddef.h>	 // 用于NULL定义

#define DBG_TAG "RTOS"
#include "log.h"

void *pvPortAlignedMalloc(size_t size, size_t alignment) {
	// 检查参数有效性：大小为0或对齐值不是2的幂则返回失败
	if (size == 0 || (alignment & (alignment - 1)) != 0) {
		return NULL;
	}

	// 计算实际需要分配的总内存：
	// 1. 目标内存大小 +
	// 2. 对齐所需的最大偏移量（alignment-1） +
	// 3. 存储原始地址的头部大小
	size_t total_size = size + alignment - 1 + sizeof(aligned_memory_header_t);

	// 用FreeRTOS的分配函数分配内存
	void *original_ptr = pvPortMalloc(total_size);
	if (original_ptr == NULL) {
		LOG_E("Aligned Malloc Failed due to The Malloc NULL\n");
		return NULL;  // 分配失败
	}

	// 计算对齐后的地址：
	// 从原始地址+头部大小开始，向上调整到对齐边界
	uintptr_t aligned_addr =
		(uintptr_t)original_ptr + sizeof(aligned_memory_header_t);
	// 计算需要偏移的字节数（确保是alignment的倍数）
	size_t offset = (alignment - (aligned_addr % alignment)) % alignment;
	aligned_addr += offset;

	// 在对齐地址的前面存储原始指针（用于释放）
	aligned_memory_header_t *header =
		(aligned_memory_header_t *)(aligned_addr -
									sizeof(aligned_memory_header_t));
	header->original_ptr = original_ptr;

	// 返回对齐后的地址
	return (void *)aligned_addr;
}

void vPortAlignedFree(void *ptr) {
	if (ptr == NULL) {
		return;	 // 空指针直接返回
	}

	// 从对齐地址反向找到头部，获取原始分配地址
	aligned_memory_header_t *header =
		(aligned_memory_header_t *)((uintptr_t)ptr -
									sizeof(aligned_memory_header_t));
	// 用FreeRTOS的释放函数释放原始地址
	vPortFree(header->original_ptr);
}
