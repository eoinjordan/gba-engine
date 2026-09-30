#ifndef TANG_STRING_H
#define TANG_STRING_H
#include <stddef.h>
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *dest, int value, size_t n);
size_t strlen(const char *s);
#endif
