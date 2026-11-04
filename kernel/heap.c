/*
 * kmalloc / kfree. one contiguous arena from the page allocator, carved
 * into blocks that sit back to back:
 *
 *   [hdr|payload....][hdr|payload..][hdr|payload.......]
 *
 * first fit, split big blocks, merge free neighbours on kfree.
 * the magic in each header catches double frees and most overruns.
 */

#include "heap.h"
#include "pmm.h"
#include "lib.h"

#define ARENA_PAGES 256		/* 1M */
#define MAGIC_USED  0xa110c8ed
#define MAGIC_FREE  0xf4eeb10c
#define MIN_SPLIT   16

struct hdr {
	uint32_t size;		/* payload bytes, multiple of 8 */
	uint32_t magic;
};

static uint8_t *arena, *arena_end;

static inline struct hdr *next_of(struct hdr *h)
{
	return (struct hdr *)((uint8_t *)(h + 1) + h->size);
}

void heap_init(void)
{
	uint32_t base = pmm_alloc_range(ARENA_PAGES);
	struct hdr *h;

	if (!base)
		panic("heap: no room for a %u page arena", ARENA_PAGES);
	arena = (uint8_t *)base;
	arena_end = arena + ARENA_PAGES * PAGE_SIZE;

	h = (struct hdr *)arena;
	h->size = arena_end - arena - sizeof(*h);
	h->magic = MAGIC_FREE;
}

static void check(struct hdr *h)
{
	if (h->magic != MAGIC_USED && h->magic != MAGIC_FREE)
		panic("heap: corrupt block header at %p", h);
}

void *kmalloc(size_t n)
{
	struct hdr *h;

	if (n == 0)
		return NULL;
	n = ALIGN_UP(n, 8);

	for (h = (struct hdr *)arena; (uint8_t *)h < arena_end; h = next_of(h)) {
		check(h);
		if (h->magic != MAGIC_FREE || h->size < n)
			continue;

		if (h->size >= n + sizeof(struct hdr) + MIN_SPLIT) {
			struct hdr *rest = (struct hdr *)((uint8_t *)(h + 1) + n);
			rest->size = h->size - n - sizeof(struct hdr);
			rest->magic = MAGIC_FREE;
			h->size = n;
		}
		h->magic = MAGIC_USED;
		return h + 1;
	}
	return NULL;
}

void kfree(void *p)
{
	struct hdr *h, *next;

	if (!p)
		return;
	h = (struct hdr *)p - 1;
	if ((uint8_t *)h < arena || (uint8_t *)h >= arena_end)
		panic("kfree: %p is not from the heap", p);
	if (h->magic == MAGIC_FREE)
		panic("kfree: double free of %p", p);
	if (h->magic != MAGIC_USED)
		panic("kfree: bad pointer %p", p);
	h->magic = MAGIC_FREE;

	/* one pass merging every run of free blocks. O(n) but n is small */
	for (h = (struct hdr *)arena; (uint8_t *)h < arena_end; h = next_of(h)) {
		if (h->magic != MAGIC_FREE)
			continue;
		for (;;) {
			next = next_of(h);
			if ((uint8_t *)next >= arena_end || next->magic != MAGIC_FREE)
				break;
			h->size += sizeof(struct hdr) + next->size;
		}
	}
}

void heap_get_stats(struct heap_stats *s)
{
	struct hdr *h;

	memset(s, 0, sizeof(*s));
	for (h = (struct hdr *)arena; (uint8_t *)h < arena_end; h = next_of(h)) {
		check(h);
		s->blocks++;
		if (h->magic == MAGIC_FREE) {
			s->free += h->size;
			s->free_blocks++;
		} else {
			s->used += h->size;
		}
	}
}
