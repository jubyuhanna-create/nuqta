#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

struct heap_stats {
	uint32_t used, free, blocks, free_blocks;
};

void heap_init(void);
void *kmalloc(size_t n);
void kfree(void *p);
void heap_get_stats(struct heap_stats *s);

#endif
