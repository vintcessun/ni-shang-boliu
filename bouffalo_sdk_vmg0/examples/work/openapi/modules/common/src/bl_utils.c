// requests_demo/modules/common/src/bl_utils.c
#include "bl_utils.h"
#include <stddef.h> // 用于 size_t
#include <string.h> // 用于 memcmp
#include <stdarg.h> // 用于 va_list, va_start, va_end
#include <stdio.h>  // 使用 vsnprintf
#include <log.h> // 使用 SDK 的日志库

/**
 * @brief 在字节串 haystack 中查找字节串 needle 的第一次出现位置。
 *
 * @param haystack 要搜索的缓冲区。
 * @param n haystack 缓冲区的大小。
 * @param needle 要查找的缓冲区。
 * @param m needle 缓冲区的大小。
 * @return void* 指向子串起始位置的指针，如果未找到则返回 NULL。
 */
void *memmem(const void *haystack, size_t n, const void *needle, size_t m)
{
    const unsigned char *y = (const unsigned char *)haystack;
    const unsigned char *x = (const unsigned char *)needle;
    size_t j, k, l;

    if (m > n || !m || !n)
        return NULL;

    if (1 != m) {
        // 优化搜索步长
        if (x[0] == x[1]) {
            k = 2;
            l = 1;
        } else {
            k = 1;
            l = 2;
        }

        j = 0;
        while (j <= n - m) {
            if (x[1] != y[j + 1]) {
                j += k;
            } else {
                if (!memcmp(x + 2, y + j + 2, m - 2) && x[0] == y[j])
                    return (void *)&y[j];
                j += l;
            }
        }
    } else
        // 处理 needle 长度为 1 的情况
        do {
            if (*y == *x)
                return (void *)y;
            y++;
        } while (--n);

    return NULL;
}