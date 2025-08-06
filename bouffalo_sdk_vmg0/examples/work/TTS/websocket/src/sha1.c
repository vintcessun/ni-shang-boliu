#include "sha1.h"

#include "mbedtls/sha1.h"

/**
 * 计算SHA-1哈希值
 * @param data 输入的二进制数据（可以包含任意字节，包括\0）
 * @param len  输入数据的长度（字节数，必须显式指定，不能依赖字符串结束符）
 * @param hash 输出的哈希结果缓冲区（必须预先分配至少20字节的空间）
 */
void sha1(const uint8_t *data, size_t len, uint8_t *hash) {
	mbedtls_sha1_context ctx;
	mbedtls_sha1_init(&ctx);
	mbedtls_sha1_starts(&ctx);
	mbedtls_sha1_update(&ctx, data, len);
	mbedtls_sha1_finish(&ctx, hash);
	mbedtls_sha1_free(&ctx);
}
