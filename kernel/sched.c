/* =============================================================================
 * kernel/sched.c - Preemptive Round Robin Scheduler (Tahap 3)
 *
 * Sumber preemption : IRQ0 dari PIT (1000 Hz, 1 tick = 1 ms).
 * Kebijakan         : Round Robin dengan time quantum (default 50 tick = 50 ms).
 * Mekanisme         : sched_tick() dipanggil dari ISR timer. Jika quantum thread yang
 *                     berjalan habis, ISR memanggil thread_switch_to() -> context_switch()
 *                     (assembly) sehingga thread dipaksa melepas CPU tanpa kerja sama
 *                     (tanpa yield). Konteks lengkap thread tersimpan di stack-nya sendiri
 *                     (pushad + frame iret dari ISR + register callee-saved dari context_switch).
 * ============================================================================= */

#include "sched.h"
#include "thread.h"
#include "sync.h"
#include "vga.h"
#include "irq.h"
#include "idt.h"

#define DEFAULT_QUANTUM 50

static volatile uint32_t ticks = 0;
static volatile int      preempt_enabled = 0;
static volatile int      trace_enabled = 0;
static volatile uint32_t quantum = DEFAULT_QUANTUM;
static volatile int32_t  slice_left = DEFAULT_QUANTUM;
static volatile sched_stats_t stats;

uint32_t sched_ticks(void) { return ticks; }
int sched_preempt_active(void) { return preempt_enabled; }

void sched_stats_reset(void) {
    stats.switches = 0;
    stats.preemptions = 0;
    stats.blocks = 0;
}

sched_stats_t sched_stats(void) {
    sched_stats_t s;
    uint32_t f = irq_save();
    s.switches = stats.switches;
    s.preemptions = stats.preemptions;
    s.blocks = stats.blocks;
    irq_restore(f);
    return s;
}

/* Dipanggil setiap kali CPU berpindah thread: thread baru mendapat quantum penuh. */
void sched_note_switch(void) {
    stats.switches++;
    slice_left = (int32_t)quantum;
}

/* Cari thread READY berikutnya secara melingkar (Round Robin), termasuk TID 0. */
static int pick_next_ready(int cur) {
    for (int offset = 1; offset < MAX_THREADS; offset++) {
        int cand = (cur + offset) % MAX_THREADS;
        if (thread_get(cand)->state == THREAD_READY) {
            return cand;
        }
    }
    return -1;
}

static void trace_switch(int prev, int next, const char *why) {
    vga_print_colored("\n[t=", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_print_int(ticks);
    vga_print_colored("ms] ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_print("#");
    vga_print_int((uint32_t)prev);
    vga_print(" ");
    vga_print(thread_get(prev)->name);
    vga_print_colored(" -> ", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("#");
    vga_print_int((uint32_t)next);
    vga_print(" ");
    vga_print(thread_get(next)->name);
    vga_print(" (");
    vga_print(why);
    vga_print("): ");
}

/* === Handler timer (berjalan di dalam ISR, IF=0, EOI sudah dikirim) === */
void sched_tick(void) {
    ticks++;
    int cur = thread_current();
    thread_get(cur)->cpu_ticks++;

    if (!preempt_enabled) {
        return;
    }
    if (--slice_left > 0) {
        return;
    }

    /* Quantum habis -> preempt */
    int next = pick_next_ready(cur);
    if (next < 0) {
        slice_left = (int32_t)quantum;   /* tidak ada kandidat, lanjutkan thread ini */
        return;
    }
    stats.preemptions++;
    if (trace_enabled) {
        trace_switch(cur, next, "quantum habis");
    }
    thread_switch_to(next, THREAD_READY);  /* RUNNING -> READY, next: READY -> RUNNING */
}

void sched_start(uint32_t quantum_ticks, int trace) {
    uint32_t f = irq_save();
    quantum = quantum_ticks;
    slice_left = (int32_t)quantum_ticks;
    trace_enabled = trace;
    preempt_enabled = 1;
    irq_restore(f);
}

void sched_stop(void) {
    uint32_t f = irq_save();
    preempt_enabled = 0;
    trace_enabled = 0;
    irq_restore(f);
}

/* Blokir thread yang sedang berjalan sampai dibangunkan sched_wake(). IF harus 0. */
void sched_block(void *obj, const char *reason) {
    int cur = thread_current();
    tcb_t *me = thread_get(cur);
    me->wait_obj = obj;
    stats.blocks++;

    int next = pick_next_ready(cur);
    while (next < 0) {
        /* Tidak ada thread READY: idle sampai ada yang bangun (jika tidak pernah, itulah deadlock) */
        __asm__ volatile ("sti; hlt; cli" : : : "memory");
        next = pick_next_ready(cur);
    }
    if (trace_enabled) {
        trace_switch(cur, next, reason);
    }
    thread_switch_to(next, THREAD_WAITING);   /* RUNNING -> WAITING */
    /* Kembali ke sini setelah dibangunkan lalu dijadwalkan ulang */
}

void sched_wake(int tid, const char *reason) {
    tcb_t *t = thread_get(tid);
    t->wait_obj = NULL;
    (void)reason;
    thread_set_state(tid, THREAD_READY, NULL, 0);  /* WAITING -> READY */
}

/* =============================================================================
 * Demo Tahap 3: 3 worker CPU-bound yang TIDAK pernah memanggil yield().
 * Setiap tick CPU yang diterima worker dicetak sebagai satu huruf (A/B/C). Pola
 * AAAAA BBBBB CCCCC membuktikan giliran CPU diatur oleh timer interrupt.
 * ============================================================================= */

#define DEMO_WORKERS 3
#define MARK_TICKS   10     /* 1 huruf dicetak tiap 10 tick (10 ms) CPU milik worker */
#define DEMO_MARKS   15     /* tiap worker mencetak 15 huruf = 150 ms CPU = 3 quantum */

static semaphore_t demo_done;

static void sched_worker(void) {
    int tid = thread_current();
    tcb_t *me = thread_get(tid);
    char mark = (char)('A' + (tid - 1));
    uint32_t last = me->cpu_ticks / MARK_TICKS;
    int marks = 0;

    while (marks < DEMO_MARKS) {            /* busy loop murni, tanpa yield/wait */
        uint32_t now = me->cpu_ticks / MARK_TICKS;
        if (now != last) {
            last = now;
            marks++;
            me->step_count++;
            uint32_t f = irq_save();
            vga_putchar(mark);
            irq_restore(f);
        }
    }
    uint32_t f = irq_save();
    vga_putchar('\n');
    irq_restore(f);
    sem_post(&demo_done);
}

void sched_run_demo(void) {
    vga_print_colored("=== [DEMO TAHAP 3] Preemptive Round Robin Scheduler ===\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("PIT 1000 Hz, quantum 50 ms, worker tanpa yield(); 1 huruf = 10 ms CPU worker.");
    thread_set_verbose(0);

    sem_init(&demo_done, 0, "demo_done");
    sched_stats_reset();

    static const char *names[DEMO_WORKERS] = { "worker-A", "worker-B", "worker-C" };
    for (int i = 0; i < DEMO_WORKERS; i++) {
        thread_create(names[i], sched_worker);
    }
    thread_ready_all();

    uint32_t t0 = ticks;
    sched_start(DEFAULT_QUANTUM, 1);
    for (int i = 0; i < DEMO_WORKERS; i++) {
        sem_wait(&demo_done);               /* shell WAITING sampai semua worker selesai */
    }
    sched_stop();
    thread_set_verbose(1);

    sched_stats_t st = sched_stats();
    vga_print_colored("\n[Hasil] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("durasi ");
    vga_print_int((ticks - t0) * (1000 / TIMER_HZ));
    vga_print(" ms | context switch: ");
    vga_print_int(st.switches);
    vga_print(" | dipicu timer (preemption): ");
    vga_print_int(st.preemptions);
    vga_putchar('\n');
    thread_print_table();
}
