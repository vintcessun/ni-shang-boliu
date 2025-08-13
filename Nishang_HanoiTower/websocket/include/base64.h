#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Base64解码函数
 * @param src 输入的base64字符串
 * @param len 输入字符串长度
 * @param dst 输出缓冲区
 * @return 解码后的数据长度，-1表示失败
 */
int base64_decode(uint8_t* src, size_t len, uint8_t* dst);

int base64_encode(const uint8_t* input, size_t input_len, char* output,
				  size_t output_max_len);

static const char b64_table[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

#ifndef B64_BUFFER_SIZE
#define B64_BUFFER_SIZE (1024 * 1)
#endif

#endif	// BASE64_H
