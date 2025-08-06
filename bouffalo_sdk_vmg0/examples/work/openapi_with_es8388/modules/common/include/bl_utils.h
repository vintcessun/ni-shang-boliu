// requests_demo/modules/common/include/bl_utils.h
#ifndef BL_UTILS_H
#define BL_UTILS_H

#include <stddef.h> // For size_t

/**
 * @brief Find the first occurrence of the byte string needle in the byte string haystack.
 *
 * @param haystack The buffer to search in.
 * @param n The size of the haystack buffer.
 * @param needle The buffer to search for.
 * @param m The size of the needle buffer.
 * @return void* A pointer to the beginning of the substring, or NULL if the substring is not found.
 */
void *memmem(const void *haystack, size_t n, const void *needle, size_t m);

// Add declarations for other common utility functions here

#endif // BL_UTILS_H