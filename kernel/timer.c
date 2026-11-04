/* 8253/8254 PIT. channel 0 is wired to IRQ0 and counts down from a
 * divisor of its 1.193182 MHz input clock */

#include "timer.h"
#include "idt.h"
#include "io.h"

#define PIT_FREQ 1193182

static volatile uint32_t ticks;

static void on_tick(struct regs *r)
{
	(void)r;
	ticks++;
}

void timer_init(void)
{
	uint32_t div = PIT_FREQ / HZ;

	outb(0x43, 0x36);	/* channel 0, lo/hi byte, mode 3 square wave */
	outb(0x40, div & 0xff);
	outb(0x40, div >> 8);
	irq_install(0, on_tick);
}

uint32_t timer_ticks(void)
{
	return ticks;
}

void sleep_ms(uint32_t ms)
{
	uint32_t end = ticks + (ms * HZ + 999) / 1000;
	while ((int32_t)(end - ticks) > 0)
		hlt();
}
