#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t v)
{
	__asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t v;
	__asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
	return v;
}

/* port 0x80 is the POST code port, writing to it takes ~1us. old PICs need that */
static inline void io_wait(void)
{
	outb(0x80, 0);
}

static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

#endif
