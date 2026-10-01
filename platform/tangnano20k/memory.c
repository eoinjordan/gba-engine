#include "string.h"
void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    unsigned char *d=dest; const unsigned char *s=src;
    while (n--) *d++=*s++;
    return dest;
}
void *memset(void *dest, int value, size_t n) {
    unsigned char *d=dest;
    while (n--) *d++=(unsigned char)value;
    return dest;
}
size_t strlen(const char *s) {
    const char *end=s;
    while (*end) ++end;
    return (size_t)(end-s);
}
