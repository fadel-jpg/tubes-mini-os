#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define TIMER_HZ 1000  /* Frekuensi PIT: 1 tick = 1 ms */

/* Layout stack yang dibentuk isr_common (boot.asm): pushad + int_no + err_code + frame CPU */
typedef struct {
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} regs_t;

/* Inisialisasi GDT flat, IDT (exception 0-31 + IRQ0), remap PIC, dan PIT. */
void interrupts_init(uint32_t hz);

#endif /* IDT_H */
