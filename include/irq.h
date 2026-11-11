#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

/* Critical section helper: simpan EFLAGS lalu matikan interrupt (CLI). */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile ("pushf\n\tpop %0\n\tcli" : "=r"(flags) : : "memory");
    return flags;
}

/* Pulihkan EFLAGS (termasuk bit IF) hasil irq_save(). */
static inline void irq_restore(uint32_t flags) {
    __asm__ volatile ("push %0\n\tpopf" : : "r"(flags) : "memory", "cc");
}

static inline void irq_enable(void)  { __asm__ volatile ("sti" : : : "memory"); }
static inline void irq_disable(void) { __asm__ volatile ("cli" : : : "memory"); }

#endif /* IRQ_H */
