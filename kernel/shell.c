#include <stdint.h>
#include "shell.h"
#include "kbd.h"
#include "lib.h"
#include "vga.h"
#include "timer.h"
#include "pmm.h"
#include "heap.h"
#include "io.h"

#define LINE_MAX 128
#define ARGS_MAX 8
#define HIST_MAX 8

static char hist[HIST_MAX][LINE_MAX];
static int hist_len, hist_next;

/* ---------- line editing ---------- */

static void erase(int n)
{
	while (n--)
		kputc('\b');
}

static void hist_push(const char *line)
{
	if (!*line)
		return;
	if (hist_len && !strcmp(hist[(hist_next + HIST_MAX - 1) % HIST_MAX], line))
		return;		/* same as last one, dont store twice */
	memcpy(hist[hist_next], line, strlen(line) + 1);
	hist_next = (hist_next + 1) % HIST_MAX;
	if (hist_len < HIST_MAX)
		hist_len++;
}

static int readline(char *out)
{
	int len = 0, back = 0;	/* back = how far up the history we are */

	for (;;) {
		int c = kbd_getc();

		if (c == '\n') {
			kputc('\n');
			out[len] = 0;
			return len;
		}
		if (c == '\b') {
			if (len > 0) {
				len--;
				kputc('\b');
			}
			continue;
		}
		if (c == 3) {		/* ctrl+c */
			kputs("^C\n");
			out[0] = 0;
			return 0;
		}
		if (c == 12) {		/* ctrl+l */
			vga_clear();
			out[0] = 0;
			return -1;
		}
		if (c == KEY_UP || c == KEY_DOWN) {
			const char *h;
			if (c == KEY_UP && back < hist_len)
				back++;
			else if (c == KEY_DOWN && back > 0)
				back--;
			else
				continue;
			erase(len);
			h = back ? hist[(hist_next + HIST_MAX - back) % HIST_MAX] : "";
			len = strlen(h);
			memcpy(out, h, len);
			out[len] = 0;
			kputs(out);
			continue;
		}
		if (c < 32 || c > 126 || len >= LINE_MAX - 1)
			continue;
		out[len++] = c;
		kputc(c);
	}
}

static int split(char *s, char **argv)
{
	int argc = 0;

	while (*s && argc < ARGS_MAX) {
		while (*s == ' ')
			*s++ = 0;
		if (!*s)
			break;
		argv[argc++] = s;
		while (*s && *s != ' ')
			s++;
	}
	return argc;
}

/* ---------- commands ---------- */

static void cmd_help(int argc, char **argv);

static void cmd_clear(int argc, char **argv)
{
	(void)argc, (void)argv;
	vga_clear();
}

static void cmd_echo(int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
		kprintf("%s%s", argv[i], i + 1 < argc ? " " : "");
	kputc('\n');
}

static void cmd_uptime(int argc, char **argv)
{
	uint32_t t = timer_ticks();
	uint32_t s = t / HZ;

	(void)argc, (void)argv;
	kprintf("up %u:%02u:%02u.%02u  (%u ticks at %u Hz)\n",
		s / 3600, s / 60 % 60, s % 60, t % HZ, t, HZ);
}

static void cmd_mem(int argc, char **argv)
{
	struct heap_stats hs;
	uint32_t total = pmm_total_pages(), free = pmm_free_pages();

	(void)argc, (void)argv;
	heap_get_stats(&hs);
	kprintf("physical: %u KiB usable, %u KiB free, %u pages in use\n",
		total * 4, free * 4, total - free);
	kprintf("heap:     %u bytes used, %u bytes free, %u blocks (%u free)\n",
		hs.used, hs.free, hs.blocks, hs.free_blocks);
}

static void cmd_mmap(int argc, char **argv)
{
	(void)argc, (void)argv;
	pmm_dump_map();
}

static void cpuid(uint32_t leaf, uint32_t r[4])
{
	__asm__ volatile("cpuid"
			 : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3])
			 : "a"(leaf), "c"(0));
}

static void cmd_cpu(int argc, char **argv)
{
	uint32_t r[4], max_ext;
	char vendor[13], brand[49];

	(void)argc, (void)argv;
	cpuid(0, r);
	memcpy(vendor, &r[1], 4);	/* ebx edx ecx, in that order */
	memcpy(vendor + 4, &r[3], 4);
	memcpy(vendor + 8, &r[2], 4);
	vendor[12] = 0;
	kprintf("vendor:   %s\n", vendor);

	cpuid(0x80000000, r);
	max_ext = r[0];
	if (max_ext >= 0x80000004) {
		for (uint32_t i = 0; i < 3; i++) {
			cpuid(0x80000002 + i, r);
			memcpy(brand + i * 16, r, 16);
		}
		brand[48] = 0;
		kprintf("model:    %s\n", brand);
	}

	cpuid(1, r);
	kprintf("family %u model %u stepping %u\n",
		(r[0] >> 8) & 0xf, (r[0] >> 4) & 0xf, r[0] & 0xf);
	kprintf("features:%s%s%s%s%s%s\n",
		r[3] & (1 << 0) ? " fpu" : "",
		r[3] & (1 << 4) ? " tsc" : "",
		r[3] & (1 << 6) ? " pae" : "",
		r[3] & (1 << 9) ? " apic" : "",
		r[3] & (1 << 25) ? " sse" : "",
		r[3] & (1 << 26) ? " sse2" : "");
}

static uint32_t rng = 2463534242u;

static uint32_t xorshift(void)
{
	rng ^= rng << 13;
	rng ^= rng >> 17;
	rng ^= rng << 5;
	return rng;
}

/* hammer the allocator, fill every block with a pattern, check nothing
 * stepped on anything else, free in random order, expect a clean heap */
static void cmd_heaptest(int argc, char **argv)
{
	enum { SLOTS = 64, ROUNDS = 2000 };
	static uint8_t *ptr[SLOTS];
	static uint32_t size[SLOTS];
	struct heap_stats before, after;
	uint32_t i, r;

	(void)argc, (void)argv;
	heap_get_stats(&before);

	for (r = 0; r < ROUNDS; r++) {
		i = xorshift() % SLOTS;
		if (ptr[i]) {
			for (uint32_t k = 0; k < size[i]; k++) {
				if (ptr[i][k] != (uint8_t)(i ^ k)) {
					kprintf("FAIL: slot %u byte %u was overwritten\n", i, k);
					return;
				}
			}
			kfree(ptr[i]);
			ptr[i] = NULL;
		} else {
			size[i] = 1 + xorshift() % 2048;
			ptr[i] = kmalloc(size[i]);
			if (!ptr[i]) {
				kprintf("FAIL: kmalloc(%u) returned NULL\n", size[i]);
				return;
			}
			for (uint32_t k = 0; k < size[i]; k++)
				ptr[i][k] = (uint8_t)(i ^ k);
		}
	}
	for (i = 0; i < SLOTS; i++) {
		kfree(ptr[i]);
		ptr[i] = NULL;
	}

	heap_get_stats(&after);
	if (after.blocks != 1 || after.free != before.free + before.used) {
		kprintf("FAIL: heap did not merge back (%u blocks left)\n", after.blocks);
		return;
	}
	kprintf("ok: %u rounds, heap back to one free block of %u bytes\n",
		ROUNDS, after.free);
}

static void cmd_alloc(int argc, char **argv)
{
	uint32_t n;
	void *p;

	if (argc < 2 || parse_uint(argv[1], &n)) {
		kputs("usage: alloc <bytes>\n");
		return;
	}
	p = kmalloc(n);
	if (p)
		kprintf("%u bytes at %p\n", n, p);
	else
		kputs("out of memory\n");
}

static void cmd_free(int argc, char **argv)
{
	uint32_t a;

	if (argc < 2 || parse_uint(argv[1], &a)) {
		kputs("usage: free <addr>\n");
		return;
	}
	kfree((void *)a);
	kputs("freed\n");
}

static void cmd_peek(int argc, char **argv)
{
	uint32_t a, n = 64;

	if (argc < 2 || parse_uint(argv[1], &a)) {
		kputs("usage: peek <addr> [bytes]\n");
		return;
	}
	if (argc > 2)
		parse_uint(argv[2], &n);
	for (uint32_t off = 0; off < n; off += 16) {
		const uint8_t *p = (const uint8_t *)(a + off);
		kprintf("%08x  ", a + off);
		for (int i = 0; i < 16; i++)
			kprintf("%02x ", p[i]);
		kputs(" ");
		for (int i = 0; i < 16; i++)
			kputc(p[i] >= 32 && p[i] < 127 ? p[i] : '.');
		kputc('\n');
	}
}

static void cmd_crash(int argc, char **argv)
{
	(void)argc, (void)argv;
	/* in C, gcc turns 1/x into a compare and never divides. so do it by hand */
	__asm__ volatile("xor %%edx, %%edx\n\t"
			 "mov $1, %%eax\n\t"
			 "xor %%ecx, %%ecx\n\t"
			 "div %%ecx" : : : "eax", "ecx", "edx");
}

static void cmd_reboot(int argc, char **argv)
{
	struct { uint16_t limit; uint32_t base; } __attribute__((packed)) none = { 0, 0 };

	(void)argc, (void)argv;
	/* pulse the reset line through the keyboard controller */
	while (inb(0x64) & 2)
		;
	outb(0x64, 0xfe);
	/* didnt work? triple fault then */
	__asm__ volatile("lidt %0; int3" : : "m"(none));
}

struct cmd {
	const char *name;
	void (*fn)(int, char **);
	const char *help;
};

static const struct cmd cmds[] = {
	{ "help",     cmd_help,     "this list" },
	{ "clear",    cmd_clear,    "clear the screen (or ctrl+l)" },
	{ "echo",     cmd_echo,     "print arguments" },
	{ "uptime",   cmd_uptime,   "time since boot" },
	{ "cpu",      cmd_cpu,      "what cpu are we on (cpuid)" },
	{ "mem",      cmd_mem,      "physical memory and heap usage" },
	{ "mmap",     cmd_mmap,     "the e820 map the bios gave us" },
	{ "alloc",    cmd_alloc,    "alloc <n>: kmalloc n bytes" },
	{ "free",     cmd_free,     "free <addr>: kfree it" },
	{ "peek",     cmd_peek,     "peek <addr> [n]: hexdump memory" },
	{ "heaptest", cmd_heaptest, "stress test the allocator" },
	{ "crash",    cmd_crash,    "divide by zero on purpose" },
	{ "reboot",   cmd_reboot,   "reset the machine" },
};

#define NCMDS (sizeof(cmds) / sizeof(cmds[0]))

static void cmd_help(int argc, char **argv)
{
	(void)argc, (void)argv;
	for (unsigned i = 0; i < NCMDS; i++)
		kprintf("  %-10s %s\n", cmds[i].name, cmds[i].help);
}

static void prompt(void)
{
	vga_set_color(VGA_LGREEN, VGA_BLACK);
	kputs("nuqta");
	vga_set_color(VGA_LGRAY, VGA_BLACK);
	kputs("> ");
}

void shell_run(void)
{
	static char line[LINE_MAX];
	char *argv[ARGS_MAX];

	kputs("type 'help' to see what it can do.\n\n");
	for (;;) {
		int argc;
		unsigned i;

		prompt();
		if (readline(line) <= 0)
			continue;
		hist_push(line);
		argc = split(line, argv);
		if (!argc)
			continue;
		for (i = 0; i < NCMDS; i++) {
			if (!strcmp(argv[0], cmds[i].name)) {
				cmds[i].fn(argc, argv);
				break;
			}
		}
		if (i == NCMDS)
			kprintf("%s: no such command\n", argv[0]);
	}
}
