#include "idt.h"
#include "io.h"
#include "lib.h"
#include "vga.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xa0
#define PIC2_DATA 0xa1
#define PIC_EOI   0x20
#define IRQ_BASE  32

struct idt_entry {
	uint16_t off_lo;
	uint16_t sel;
	uint8_t zero;
	uint8_t flags;
	uint16_t off_hi;
} __attribute__((packed));

struct idt_ptr {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static irq_handler_t handlers[16];
extern uint32_t isr_table[48];

static const char *exc_names[32] = {
	"divide error", "debug", "nmi", "breakpoint", "overflow",
	"bound range", "invalid opcode", "device not available",
	"double fault", "coprocessor overrun", "invalid tss",
	"segment not present", "stack fault", "general protection",
	"page fault", "reserved", "x87 fpu", "alignment check",
	"machine check", "simd fpu", "virtualization", "control protection",
	"reserved", "reserved", "reserved", "reserved", "reserved",
	"reserved", "hypervisor injection", "vmm communication",
	"security", "reserved",
};

static void set_gate(int n, uint32_t handler)
{
	idt[n].off_lo = handler & 0xffff;
	idt[n].off_hi = handler >> 16;
	idt[n].sel = 0x08;
	idt[n].zero = 0;
	idt[n].flags = 0x8e;	/* present, ring 0, 32 bit interrupt gate */
}

/*
 * by default the PIC sends IRQ 0-7 to vectors 8-15, right on top of the
 * cpu exceptions. so a timer tick looks like a double fault. move them to 32+.
 */
static void pic_remap(void)
{
	outb(PIC1_CMD, 0x11);	/* ICW1: init, expect ICW4 */
	io_wait();
	outb(PIC2_CMD, 0x11);
	io_wait();
	outb(PIC1_DATA, IRQ_BASE);	/* ICW2: vector offset */
	io_wait();
	outb(PIC2_DATA, IRQ_BASE + 8);
	io_wait();
	outb(PIC1_DATA, 0x04);	/* ICW3: slave sits on IRQ2 */
	io_wait();
	outb(PIC2_DATA, 0x02);
	io_wait();
	outb(PIC1_DATA, 0x01);	/* ICW4: 8086 mode */
	io_wait();
	outb(PIC2_DATA, 0x01);
	io_wait();

	/* mask everything, drivers unmask what they need */
	outb(PIC1_DATA, 0xff);
	outb(PIC2_DATA, 0xff);
}

static void irq_unmask(int irq)
{
	uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
	outb(port, inb(port) & ~(1 << (irq & 7)));
	if (irq >= 8)
		irq_unmask(2);
}

void irq_install(int irq, irq_handler_t h)
{
	handlers[irq] = h;
	irq_unmask(irq);
}

static uint8_t pic_isr(uint16_t cmd)
{
	outb(cmd, 0x0b);	/* OCW3: read in-service register */
	return inb(cmd);
}

void idt_init(void)
{
	struct idt_ptr p;

	pic_remap();
	for (int i = 0; i < 48; i++)
		set_gate(i, isr_table[i]);

	p.limit = sizeof(idt) - 1;
	p.base = (uint32_t)idt;
	__asm__ volatile("lidt %0" : : "m"(p));
}

static void exception(struct regs *r)
{
	uint32_t cr2;

	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
	cli();
	vga_set_color(VGA_WHITE, VGA_RED);
	kprintf("\n cpu exception %u: %s  err=%x \n", r->int_no,
		exc_names[r->int_no], r->err_code);
	vga_set_color(VGA_LGRAY, VGA_BLACK);
	kprintf("  eip=%08x  cs=%04x  eflags=%08x  cr2=%08x\n",
		r->eip, r->cs, r->eflags, cr2);
	kprintf("  eax=%08x  ebx=%08x  ecx=%08x  edx=%08x\n",
		r->eax, r->ebx, r->ecx, r->edx);
	kprintf("  esi=%08x  edi=%08x  ebp=%08x\n", r->esi, r->edi, r->ebp);
	kprintf("  ds=%04x es=%04x fs=%04x gs=%04x\n", r->ds, r->es, r->fs, r->gs);
	panic("unhandled exception");
}

void isr_dispatch(struct regs *r)
{
	int irq;

	if (r->int_no < IRQ_BASE) {
		exception(r);
		return;
	}

	irq = r->int_no - IRQ_BASE;

	/*
	 * spurious IRQs: the PIC raises 7 (or 15) when a line drops before
	 * the cpu acks it. the in-service bit wont be set. no EOI for these,
	 * except a spurious 15 still needs one on the master for the cascade.
	 */
	if (irq == 7 && !(pic_isr(PIC1_CMD) & 0x80))
		return;
	if (irq == 15 && !(pic_isr(PIC2_CMD) & 0x80)) {
		outb(PIC1_CMD, PIC_EOI);
		return;
	}

	if (handlers[irq])
		handlers[irq](r);

	if (irq >= 8)
		outb(PIC2_CMD, PIC_EOI);
	outb(PIC1_CMD, PIC_EOI);
}
