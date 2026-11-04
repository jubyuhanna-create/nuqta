#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096

struct e820_entry {
	uint64_t base;
	uint64_t len;
	uint32_t type;
	uint32_t acpi;
} __attribute__((packed));

void pmm_init(void);
uint32_t pmm_alloc(void);		/* one page, 0 if out of memory */
uint32_t pmm_alloc_range(uint32_t n);	/* n contiguous pages */
void pmm_free(uint32_t addr);
uint32_t pmm_total_pages(void);
uint32_t pmm_free_pages(void);
void pmm_dump_map(void);

#endif
