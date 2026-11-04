#include "lib.h"
#include "vga.h"
#include "serial.h"
#include "io.h"

/* gcc is allowed to emit calls to these even with -ffreestanding */

void *memset(void *dst, int c, size_t n)
{
	uint8_t *d = dst;
	while (n--)
		*d++ = (uint8_t)c;
	return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
	uint8_t *d = dst;
	const uint8_t *s = src;
	while (n--)
		*d++ = *s++;
	return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
	uint8_t *d = dst;
	const uint8_t *s = src;
	if (d < s) {
		while (n--)
			*d++ = *s++;
	} else {
		d += n;
		s += n;
		while (n--)
			*--d = *--s;
	}
	return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const uint8_t *x = a, *y = b;
	for (; n; n--, x++, y++)
		if (*x != *y)
			return *x - *y;
	return 0;
}

size_t strlen(const char *s)
{
	size_t n = 0;
	while (s[n])
		n++;
	return n;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) {
		if (*a != *b)
			return (uint8_t)*a - (uint8_t)*b;
		if (!*a)
			return 0;
	}
	return 0;
}

int parse_uint(const char *s, uint32_t *out)
{
	uint32_t base = 10, v = 0;

	if (!s || !*s)
		return -1;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		base = 16;
		s += 2;
		if (!*s)
			return -1;
	}
	for (; *s; s++) {
		uint32_t d;
		if (*s >= '0' && *s <= '9')
			d = *s - '0';
		else if (base == 16 && *s >= 'a' && *s <= 'f')
			d = *s - 'a' + 10;
		else if (base == 16 && *s >= 'A' && *s <= 'F')
			d = *s - 'A' + 10;
		else
			return -1;
		v = v * base + d;
	}
	*out = v;
	return 0;
}

/* ---- console: everything goes to the screen and COM1 ---- */

void kputc(char c)
{
	vga_putc(c);
	serial_putc(c);
}

void kputs(const char *s)
{
	while (*s)
		kputc(*s++);
}

static int utoa(uint32_t v, unsigned base, int upper, char *out)
{
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	char tmp[12];
	int n = 0, i;

	do {
		tmp[n++] = digits[v % base];
		v /= base;
	} while (v);
	for (i = 0; i < n; i++)
		out[i] = tmp[n - 1 - i];
	return n;
}

static void pad(int n, char c)
{
	while (n-- > 0)
		kputc(c);
}

/* small printf: %d %i %u %x %X %p %s %c %%, with 0 / - flags and width */
void kvprintf(const char *fmt, va_list ap)
{
	for (; *fmt; fmt++) {
		char buf[12];
		const char *s = buf;
		int left = 0, zero = 0, width = 0, len = 0, neg = 0, total, i;

		if (*fmt != '%') {
			kputc(*fmt);
			continue;
		}
		fmt++;
		for (;; fmt++) {
			if (*fmt == '-')
				left = 1;
			else if (*fmt == '0')
				zero = 1;
			else
				break;
		}
		while (*fmt >= '0' && *fmt <= '9')
			width = width * 10 + (*fmt++ - '0');
		while (*fmt == 'l')
			fmt++;

		switch (*fmt) {
		case 'c':
			buf[0] = (char)va_arg(ap, int);
			len = 1;
			break;
		case 's':
			s = va_arg(ap, const char *);
			if (!s)
				s = "(null)";
			len = strlen(s);
			break;
		case 'd':
		case 'i': {
			int v = va_arg(ap, int);
			uint32_t u = (uint32_t)v;
			if (v < 0) {
				neg = 1;
				u = -u;
			}
			len = utoa(u, 10, 0, buf);
			break;
		}
		case 'u':
			len = utoa(va_arg(ap, uint32_t), 10, 0, buf);
			break;
		case 'x':
		case 'X':
			len = utoa(va_arg(ap, uint32_t), 16, *fmt == 'X', buf);
			break;
		case 'p':
			kputs("0x");
			len = utoa((uint32_t)(uintptr_t)va_arg(ap, void *), 16, 0, buf);
			zero = 1;
			width = 8;
			break;
		case '%':
			buf[0] = '%';
			len = 1;
			break;
		case '\0':
			return;
		default:
			kputc('%');
			buf[0] = *fmt;
			len = 1;
			break;
		}

		total = len + neg;
		if (!left && !zero)
			pad(width - total, ' ');
		if (neg)
			kputc('-');
		if (!left && zero)
			pad(width - total, '0');
		for (i = 0; i < len; i++)
			kputc(s[i]);
		if (left)
			pad(width - total, ' ');
	}
}

void kprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
}

void panic(const char *fmt, ...)
{
	va_list ap;

	cli();
	vga_set_color(VGA_WHITE, VGA_RED);
	kputs("\npanic: ");
	va_start(ap, fmt);
	kvprintf(fmt, ap);
	va_end(ap);
	kputs("\nsystem halted.");
	for (;;)
		hlt();
}
