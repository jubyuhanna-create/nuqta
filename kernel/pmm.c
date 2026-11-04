/*
 * physical page allocator. one bit per 4K page, 1 = used.
 * the bootloader saved the BIOS e820 map at 0x8000, we start with
 * everything marked used and only free what the BIOS says is usable RAM.
 */

#include "pmm.h"
#include "lib.h"

#define MMAP_ADDR 0x8000
#define LOW_RESERVED 0x100000	/* never hand out the first 1M */

extern char __kernel_end[];

static uint32_t *bitmap;
static uint32_t npages;		/* pages covered by the bitmap */
static uint32_t usable;		/* pages the BIOS called usable */
static uint32_t nfree;
static uint32_t hint;		/* where the last search ended */

static inline void set_used(uint32_t p)  { bitmap[p / 32] |= 1u << (p % 32); }
static inline void set_free(uint32_t p)  { bitmap[p / 32] &= ~(1u << (p % 32)); }
static inline int is_used(uint32_t p)    { return bitmap[p / 32] >> (p % 32) & 1; }

static uint32_t mmap_count(void)
{
	return *(volatile uint32_t *)MMAP_ADDR;
}

static struct e820_entry *mmap_entries(void)
{
	return (struct e820_entry *)(MMAP_ADDR + 4);
}

static const char *type_name(uint32_t t)
{
	switch (t) {
	case 1: return "usable";
	case 2: return "reserved";
	case 3: return "acpi reclaim";
	case 4: return "acpi nvs";
	case 5: return "bad";
	default: return "?";
	}
}

void pmm_init(void)
{
	struct e820_entry *e = mmap_entries();
	uint32_t n = mmap_count(), i, top = 0;

	if (n == 0)
		panic("pmm: bios gave no memory map");

	/* highest usable address below 4G decides the bitmap size */
	for (i = 0; i < n; i++) {
		uint64_t end = e[i].base + e[i].len;
		if (e[i].type != 1 || e[i].base >= 0x100000000ull)
			continue;
		if (end > 0x100000000ull)
			end = 0x100000000ull;
		if (end > top)
			top = (uint32_t)(end - 1);
	}
	npages = top / PAGE_SIZE + 1;

	/* bitmap lives right after the kernel image, still in low memory */
	bitmap = (uint32_t *)ALIGN_UP((uint32_t)__kernel_end, 16);
	if ((uint32_t)bitmap + npages / 8 > 0x80000)
		panic("pmm: bitmap does not fit in low memory");
	memset(bitmap, 0xff, ALIGN_UP(npages, 32) / 8);

	for (i = 0; i < n; i++) {
		uint64_t start, end;
		uint32_t p;

		if (e[i].type != 1 || e[i].base >= 0x100000000ull)
			continue;
		start = ALIGN_UP(e[i].base, PAGE_SIZE);
		end = e[i].base + e[i].len;
		if (end > 0x100000000ull)
			end = 0x100000000ull;
		end = ALIGN_DOWN(end, PAGE_SIZE);

		for (p = start / PAGE_SIZE; p < end / PAGE_SIZE; p++) {
			set_free(p);
			usable++;
		}
	}

	/* low 1M: bios data, our kernel, the bitmap, vga memory. hands off */
	for (i = 0; i < LOW_RESERVED / PAGE_SIZE && i < npages; i++)
		set_used(i);

	nfree = 0;
	for (i = 0; i < npages; i++)
		if (!is_used(i))
			nfree++;
	hint = LOW_RESERVED / PAGE_SIZE;
}

uint32_t pmm_alloc_range(uint32_t count)
{
	uint32_t start, run, p, tries;

	if (count == 0 || count > nfree)
		return 0;

	/* wrap around once from the hint */
	start = hint;
	run = 0;
	for (tries = 0, p = hint; tries < npages; tries++, p++) {
		if (p >= npages) {
			p = 0;
			run = 0;
		}
		if (is_used(p)) {
			run = 0;
			continue;
		}
		if (run == 0)
			start = p;
		if (++run == count) {
			for (p = start; p < start + count; p++)
				set_used(p);
			nfree -= count;
			hint = start + count;
			return start * PAGE_SIZE;
		}
	}
	return 0;
}

uint32_t pmm_alloc(void)
{
	return pmm_alloc_range(1);
}

void pmm_free(uint32_t addr)
{
	uint32_t p = addr / PAGE_SIZE;

	if (addr % PAGE_SIZE || p >= npages || addr < LOW_RESERVED)
		panic("pmm_free: bad address %p", (void *)addr);
	if (!is_used(p))
		panic("pmm_free: double free of %p", (void *)addr);
	set_free(p);
	nfree++;
	if (p < hint)
		hint = p;
}

uint32_t pmm_total_pages(void) { return usable; }
uint32_t pmm_free_pages(void)  { return nfree; }

void pmm_dump_map(void)
{
	struct e820_entry *e = mmap_entries();
	uint32_t n = mmap_count();

	kprintf("  %-18s %-18s %s\n", "base", "end", "type");
	for (uint32_t i = 0; i < n; i++) {
		uint64_t end = e[i].base + e[i].len - 1;
		kprintf("  %08x_%08x  %08x_%08x  %s\n",
			(uint32_t)(e[i].base >> 32), (uint32_t)e[i].base,
			(uint32_t)(end >> 32), (uint32_t)end, type_name(e[i].type));
	}
}
