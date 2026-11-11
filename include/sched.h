#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

typedef struct {
    uint32_t switches;     /* total context switch (semua penyebab) */
    uint32_t preemptions;  /* context switch yang dipicu timer (quantum habis) */
    uint32_t blocks;       /* thread yang diblokir mutex/semaphore */
} sched_stats_t;

/* Dipanggil dari ISR timer (IRQ0) setiap 1 tick */
void     sched_tick(void);
void     sched_note_switch(void);
int      sched_preempt_active(void);

/* Kendali scheduler preemptive Round Robin */
void     sched_start(uint32_t quantum_ticks, int trace);
void     sched_stop(void);
uint32_t sched_ticks(void);
void     sched_stats_reset(void);
sched_stats_t sched_stats(void);

/* Blokir thread saat ini (WAITING) / bangunkan thread. Pemanggil harus IF=0. */
void     sched_block(void *obj, const char *reason);
void     sched_wake(int tid, const char *reason);

/* Demo Tahap 3 */
void     sched_run_demo(void);

#endif /* SCHED_H */
