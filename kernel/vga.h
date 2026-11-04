#ifndef VGA_H
#define VGA_H

#include <stdint.h>

enum {
	VGA_BLACK, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA,
	VGA_BROWN, VGA_LGRAY, VGA_DGRAY, VGA_LBLUE, VGA_LGREEN, VGA_LCYAN,
	VGA_LRED, VGA_LMAGENTA, VGA_YELLOW, VGA_WHITE
};

void vga_clear(void);
void vga_putc(char c);
void vga_set_color(uint8_t fg, uint8_t bg);

#endif
