/* 80x25 text mode. the screen is just memory at 0xb8000,
 * two bytes per cell: low = ascii, high = color (bg << 4 | fg) */

#include "vga.h"
#include "io.h"

#define COLS 80
#define ROWS 25

static volatile uint16_t *const vram = (volatile uint16_t *)0xb8000;
static int row, col;
static uint8_t attr = (VGA_BLACK << 4) | VGA_LGRAY;

static inline uint16_t cell(char c)
{
	return (uint8_t)c | ((uint16_t)attr << 8);
}

/* the blinking cursor is owned by the CRT controller, not by us */
static void sync_cursor(void)
{
	uint16_t pos = row * COLS + col;
	outb(0x3d4, 0x0e);
	outb(0x3d5, pos >> 8);
	outb(0x3d4, 0x0f);
	outb(0x3d5, pos & 0xff);
}

void vga_set_color(uint8_t fg, uint8_t bg)
{
	attr = (bg << 4) | (fg & 0x0f);
}

void vga_clear(void)
{
	for (int i = 0; i < COLS * ROWS; i++)
		vram[i] = cell(' ');
	row = col = 0;
	sync_cursor();
}

static void scroll(void)
{
	int i;

	if (row < ROWS)
		return;
	for (i = 0; i < COLS * (ROWS - 1); i++)
		vram[i] = vram[i + COLS];
	for (; i < COLS * ROWS; i++)
		vram[i] = cell(' ');
	row = ROWS - 1;
}

void vga_putc(char c)
{
	switch (c) {
	case '\n':
		col = 0;
		row++;
		break;
	case '\r':
		col = 0;
		break;
	case '\t':
		col = (col + 8) & ~7;
		break;
	case '\b':
		if (col > 0) {
			col--;
		} else if (row > 0) {
			row--;
			col = COLS - 1;
		}
		vram[row * COLS + col] = cell(' ');
		break;
	default:
		vram[row * COLS + col] = cell(c);
		col++;
		break;
	}
	if (col >= COLS) {
		col = 0;
		row++;
	}
	scroll();
	sync_cursor();
}
