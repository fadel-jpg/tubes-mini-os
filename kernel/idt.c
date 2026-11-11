/* =============================================================================
 * kernel/idt.c - GDT, IDT, PIC, PIT (infrastruktur interrupt untuk Tahap 3)
 * IRQ0 (timer PIT) menjadi sumber preemption bagi Round Robin scheduler.
 * ============================================================================= */

#include "idt.h"
#include "vga.h"
#include "sched.h"
#include "irq.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr { uint16_t limit; uint32_t base; } __attribute__((packed));

struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr { uint16_t limit; uint32_t base; } __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdt_p;
static struct idt_entry idt[256];
static struct idt_ptr   idt_p;

extern uint32_t isr_stub_table[32]; /* boot.asm */
extern void irq0_stub(void);        /* boot.asm */

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low   = base & 0xFFFF;
    gdt[i].base_mid   = (base >> 16) & 0xFF;
    gdt[i].base_high  = (base >> 24) & 0xFF;
    gdt[i].limit_low  = limit & 0xFFFF;
    gdt[i].gran       = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access     = access;
}

static void gdt_init(void) {
    gdt_set(0, 0, 0, 0, 0);                  /* null descriptor */
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);   /* 0x08: kernel code, flat 4GB */
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);   /* 0x10: kernel data, flat 4GB */
    gdt_p.limit = sizeof(gdt) - 1;
    gdt_p.base  = (uint32_t)&gdt;
    __asm__ volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:"
        : : "m"(gdt_p) : "eax", "memory");
}

static void idt_set_gate(int n, uint32_t handler) {
    idt[n].base_lo = handler & 0xFFFF;
    idt[n].sel     = 0x08;
    idt[n].zero    = 0;
    idt[n].flags   = 0x8E;            /* present, ring0, 32-bit interrupt gate (IF=0 saat masuk) */
    idt[n].base_hi = (handler >> 16) & 0xFFFF;
}

static void pic_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);   /* ICW1: init + ICW4 */
    outb(0x21, 0x20); outb(0xA1, 0x28);   /* ICW2: remap IRQ0-7 -> 0x20, IRQ8-15 -> 0x28 */
    outb(0x21, 0x04); outb(0xA1, 0x02);   /* ICW3: cascade */
    outb(0x21, 0x01); outb(0xA1, 0x01);   /* ICW4: mode 8086 */
    outb(0x21, 0xFE);                     /* mask semua IRQ kecuali IRQ0 (timer) */
    outb(0xA1, 0xFF);
}

static void pit_init(uint32_t hz) {
    uint32_t divisor = 1193182 / hz;
    outb(0x43, 0x36);                     /* channel 0, lo/hi byte, mode 3 (square wave) */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void interrupts_init(uint32_t hz) {
    gdt_init();
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, isr_stub_table[i]);
    }
    idt_set_gate(0x20, (uint32_t)irq0_stub);
    idt_p.limit = sizeof(idt) - 1;
    idt_p.base  = (uint32_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idt_p));
    pic_init();
    pit_init(hz);
}

/* Dipanggil dari irq0_stub (boot.asm). EOI dikirim SEBELUM kemungkinan context
 * switch, kalau tidak PIC tidak akan mengirim IRQ0 lagi ke thread berikutnya. */
void timer_irq_handler(void) {
    outb(0x20, 0x20);
    sched_tick();
}

static const char *exc_names[32] = {
    "Divide Error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound Range",
    "Invalid Opcode", "Device N/A", "Double Fault", "Coproc Overrun", "Invalid TSS",
    "Segment Not Present", "Stack Fault", "General Protection", "Page Fault",
    "Reserved", "x87 FPU", "Alignment Check", "Machine Check", "SIMD FP",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Security", "Reserved"
};

static void print_hex(uint32_t v) {
    const char *hex = "0123456789ABCDEF";
    vga_print("0x");
    for (int s = 28; s >= 0; s -= 4) vga_putchar(hex[(v >> s) & 0xF]);
}

void exception_handler(regs_t *r) {
    vga_print_colored("\n*** CPU EXCEPTION #", VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_print_int(r->int_no);
    vga_print_colored(" (", VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_print_colored(r->int_no < 32 ? exc_names[r->int_no] : "?", VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_print_colored(") ***\n", VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_print("  EIP="); print_hex(r->eip);
    vga_print("  ERR="); print_hex(r->err_code);
    vga_print("  EFLAGS="); print_hex(r->eflags);
    vga_print("\nSistem dihentikan.\n");
    for (;;) { __asm__ volatile ("cli; hlt"); }
}
