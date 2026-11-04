#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* layout matches what isr_common pushes, lowest address first */
struct regs {
	uint32_t gs, fs, es, ds;
	uint32_t edi, esi, ebp, esp_unused, ebx, edx, ecx, eax;
	uint32_t int_no, err_code;
	uint32_t eip, cs, eflags;	/* pushed by the cpu */
};

typedef void (*irq_handler_t)(struct regs *r);

void idt_init(void);
void irq_install(int irq, irq_handler_t h);

#endif
