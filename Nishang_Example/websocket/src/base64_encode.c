#include <stdio.h>
#include <stdlib.h>

#include "base64.h"

int base64_encode(const uint8_t *input, size_t input_len, char *output,
				  size_t output_max_len) {
	int i = 0;
	int j = 0;
	size_t out_len = 0;
	uint8_t buf[4];
	uint8_t tmp[3];

	// 检查输出缓冲区是否足够大
	size_t required_size = ((input_len + 2) / 3) * 4 + 1;
	if (output_max_len < required_size) {
		return -1;
	}

	// 处理输入数据
	while (input_len--) {
		tmp[i++] = *(input++);

		// 每3个字节编码为4个base64字符
		if (i == 3) {
			buf[0] = (tmp[0] & 0xfc) >> 2;
			buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
			buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
			buf[3] = tmp[2] & 0x3f;

			for (i = 0; i < 4; i++) {
				output[out_len++] = b64_table[buf[i]];
			}
			i = 0;
		}
	}

	// 处理剩余字节
	if (i > 0) {
		// 填充剩余字节
		for (j = i; j < 3; j++) {
			tmp[j] = '\0';
		}

		buf[0] = (tmp[0] & 0xfc) >> 2;
		buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
		buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
		buf[3] = tmp[2] & 0x3f;

		for (j = 0; j < i + 1; j++) {
			output[out_len++] = b64_table[buf[j]];
		}

		// 填充'='
		while (i++ < 3) {
			output[out_len++] = '=';
		}
	}

	// 添加字符串结束符
	output[out_len] = '\0';
	return out_len;
}
