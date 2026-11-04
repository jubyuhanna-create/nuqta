/* PS/2 keyboard, scancode set 1 (the controller translates for us).
 * a key press sends its code, release sends code | 0x80.
 * arrows and friends come as 0xe0 then the code. */

#include <stdint.h>
#include "kbd.h"
#include "idt.h"
#include "io.h"

#define KBD_DATA   0x60
#define KBD_STATUS 0x64

static const char normal[58] = {
	0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
	0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
	0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
	'*', 0, ' ',
};

static const char shifted[58] = {
	0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
	0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
	0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
	'*', 0, ' ',
};

#define SC_LSHIFT 0x2a
#define SC_RSHIFT 0x36
#define SC_CTRL   0x1d
#define SC_CAPS   0x3a

/* ring buffer, written from the irq, read from the shell */
#define BUFSZ 64
static volatile uint16_t buf[BUFSZ];
static volatile uint8_t head, tail;

static int shift, ctrl, caps, ext;

static void push(int c)
{
	uint8_t next = (head + 1) % BUFSZ;
	if (next == tail)
		return;		/* full, drop it */
	buf[head] = c;
	head = next;
}

static void on_key(struct regs *r)
{
	uint8_t sc = inb(KBD_DATA);
	int up = sc & 0x80;
	char c;

	(void)r;

	if (sc == 0xe0) {
		ext = 1;
		return;
	}
	if (ext) {
		ext = 0;
		if (sc == 0x1d || sc == 0x9d) {	/* right ctrl */
			ctrl = !up;
			return;
		}
		if (up)
			return;
		switch (sc) {
		case 0x48: push(KEY_UP); break;
		case 0x50: push(KEY_DOWN); break;
		case 0x4b: push(KEY_LEFT); break;
		case 0x4d: push(KEY_RIGHT); break;
		}
		return;
	}

	sc &= 0x7f;
	if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
		shift = !up;
		return;
	}
	if (sc == SC_CTRL) {
		ctrl = !up;
		return;
	}
	if (up)
		return;
	if (sc == SC_CAPS) {
		caps = !caps;
		return;
	}
	if (sc >= sizeof(normal))
		return;

	c = shift ? shifted[sc] : normal[sc];
	if (caps && c >= 'a' && c <= 'z')
		c -= 32;
	else if (caps && c >= 'A' && c <= 'Z')
		c += 32;
	if (!c)
		return;
	if (ctrl && ((c | 32) >= 'a' && (c | 32) <= 'z'))
		c = (c | 32) & 0x1f;	/* ctrl+c = 3, ctrl+l = 12 ... */
	push((uint8_t)c);
}

void kbd_init(void)
{
	/* throw away anything the BIOS left in the output buffer */
	while (inb(KBD_STATUS) & 1)
		inb(KBD_DATA);
	irq_install(1, on_key);
}

int kbd_getc(void)
{
	int c;

	for (;;) {
		cli();
		if (head != tail)
			break;
		/* sti; hlt back to back, so an irq between them cant be missed */
		__asm__ volatile("sti; hlt");
	}
	c = buf[tail];
	tail = (tail + 1) % BUFSZ;
	sti();
	return c;
}
