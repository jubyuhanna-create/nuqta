/* COM1 at 38400 8N1. mostly for debugging: qemu -serial stdio shows
 * the same output as the screen, which is nice when the screen is gone */

#include "serial.h"
#include "io.h"

#define COM1 0x3f8

static int ok;

void serial_init(void)
{
	outb(COM1 + 1, 0x00);	/* no interrupts */
	outb(COM1 + 3, 0x80);	/* DLAB on, next two writes are the divisor */
	outb(COM1 + 0, 0x03);	/* 115200 / 3 = 38400 */
	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x03);	/* 8 bits, no parity, 1 stop, DLAB off */
	outb(COM1 + 2, 0xc7);	/* fifo on, cleared, 14 byte threshold */
	outb(COM1 + 4, 0x0b);

	/* loopback test so we dont spin forever on a machine with no uart */
	outb(COM1 + 4, 0x1e);
	outb(COM1 + 0, 0xae);
	ok = inb(COM1 + 0) == 0xae;
	outb(COM1 + 4, 0x0f);
}

void serial_putc(char c)
{
	if (!ok)
		return;
	if (c == '\n')
		serial_putc('\r');
	while (!(inb(COM1 + 5) & 0x20))
		;
	outb(COM1, c);
}
