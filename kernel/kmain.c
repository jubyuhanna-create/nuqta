#include "lib.h"
#include "vga.h"
#include "serial.h"
#include "idt.h"
#include "pmm.h"
#include "heap.h"
#include "timer.h"
#include "kbd.h"
#include "shell.h"
#include "io.h"

extern char __kernel_end[];

static void step(const char *what)
{
	vga_set_color(VGA_DGRAY, VGA_BLACK);
	kputs("  * ");
	vga_set_color(VGA_LGRAY, VGA_BLACK);
	kputs(what);
	kputc('\n');
}

void kmain(void)
{
	serial_init();
	vga_clear();

	vga_set_color(VGA_WHITE, VGA_BLACK);
	kputs("nuqta");
	vga_set_color(VGA_DGRAY, VGA_BLACK);
	kprintf(" 0.1, i386, kernel 0x10000-%p\n\n", __kernel_end);

	idt_init();
	step("idt loaded, pic remapped to 32-47");
	pmm_init();
	kprintf("  * pmm: %u KiB usable ram\n", pmm_total_pages() * 4);
	heap_init();
	step("heap: 1 MiB arena");
	timer_init();
	step("pit at 100 Hz");
	kbd_init();
	step("ps/2 keyboard");

	sti();
	kputc('\n');
	shell_run();
}
