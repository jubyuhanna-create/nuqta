#ifndef LIB_H
#define LIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
/* parses decimal or 0x hex. returns -1 on garbage */
int parse_uint(const char *s, uint32_t *out);

void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list ap);

__attribute__((noreturn)) void panic(const char *fmt, ...);

#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

#endif
