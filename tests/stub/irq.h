#ifndef IRQ_H
#define IRQ_H
/* Versi uji: di luar kernel tidak ada interrupt, jadi semuanya no-op. */
#include <stdint.h>
static inline uint32_t irq_save(void) { return 0; }
static inline void irq_restore(uint32_t f) { (void)f; }
static inline void irq_enable(void) {}
static inline void irq_disable(void) {}
#endif
