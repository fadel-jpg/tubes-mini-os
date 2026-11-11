/* =============================================================================
 * kernel/sync.c - Sinkronisasi: Mutex & Semaphore (Tahap 4)
 *
 * Ditujukan untuk kernel single-CPU yang preemptive. Atomisitas operasi lock/unlock dijamin
 * dengan mematikan interrupt (irq_save/irq_restore) selama manipulasi struktur data, sehingga
 * timer tidak dapat mem-preempt di tengah operasi. Thread yang gagal mengambil lock tidak
 * spin: ia masuk antrean FIFO, berstatus WAITING, dan melepas CPU (sched_block). Saat unlock/
 * post, thread terdepan di antrean dibangunkan (READY) lalu MENCOBA ULANG (semantik Mesa).
 * Dengan begitu lock tidak diserahkan paksa sehingga terhindar dari lock convoy, yaitu dua
 * thread yang saling bergantian CPU pada setiap iterasi.
 * ============================================================================= */

#include "sync.h"
#include "sched.h"
#include "vga.h"
#include "irq.h"
#include "idt.h"

/* ---------- antrean tunggu ---------- */
static void wq_init(waitq_t *q) { q->head = 0; q->count = 0; }

static void wq_push(waitq_t *q, int tid) {
    q->buf[(q->head + q->count) % MAX_THREADS] = tid;
    q->count++;
}

static int wq_pop(waitq_t *q) {
    int tid = q->buf[q->head];
    q->head = (q->head + 1) % MAX_THREADS;
    q->count--;
    return tid;
}

/* ---------- mutex ---------- */
void mutex_init(mutex_t *m, const char *name) {
    m->locked = 0;
    m->owner = -1;
    m->name = name;
    wq_init(&m->waiters);
}

void mutex_lock(mutex_t *m) {
    uint32_t f = irq_save();
    int me = thread_current();
    while (m->locked) {                      /* dicek ulang setelah dibangunkan */
        wq_push(&m->waiters, me);
        sched_block(m, "mutex terkunci");
    }
    m->locked = 1;
    m->owner = me;
    irq_restore(f);
}

void mutex_unlock(mutex_t *m) {
    uint32_t f = irq_save();
    if (!m->locked || m->owner != thread_current()) {
        irq_restore(f);
        return;                              /* hanya pemilik yang boleh unlock */
    }
    m->locked = 0;
    m->owner = -1;
    if (m->waiters.count > 0) {
        sched_wake(wq_pop(&m->waiters), "mutex dibuka");   /* waiter akan mencoba lock lagi */
    }
    irq_restore(f);
}

/* ---------- semaphore ---------- */
void sem_init(semaphore_t *s, int initial, const char *name) {
    s->count = initial;
    s->name = name;
    wq_init(&s->waiters);
}

void sem_wait(semaphore_t *s) {
    uint32_t f = irq_save();
    while (s->count <= 0) {                  /* dicek ulang setelah dibangunkan */
        wq_push(&s->waiters, thread_current());
        sched_block(s, "semaphore kosong");
    }
    s->count--;
    irq_restore(f);
}

void sem_post(semaphore_t *s) {
    uint32_t f = irq_save();
    s->count++;
    if (s->waiters.count > 0) {
        sched_wake(wq_pop(&s->waiters), "semaphore di-post");
    }
    irq_restore(f);
}

/* =============================================================================
 * Demo Tahap 4: dua thread menaikkan satu shared counter sebanyak RACE_ITER kali.
 * Penambahan sengaja ditulis sebagai load -> add -> store (3 langkah terpisah) agar
 * timer dapat mem-preempt di antaranya, sama seperti yang terjadi pada counter++ non-atomik
 * di kernel nyata. Quantum 1 tick (10 ms) dipakai supaya banyak preemption terjadi.
 * ============================================================================= */

#define RACE_ITER    2000000u
#define RACE_THREADS 2

static volatile uint32_t shared_counter;

/* Pekerjaan kecil di dalam critical section (mis. validasi/kalkulasi sebelum menyimpan hasil).
 * Berisi percabangan dan pemanggilan fungsi sehingga window antara LOAD dan STORE melintasi
 * batas translation block QEMU TCG; emulator ini hanya memeriksa interrupt di batas blok,
 * jadi tanpa ini race tidak teramati di QEMU walau di CPU asli tetap terjadi. */
static void __attribute__((noinline)) critical_work(void) {
    for (volatile int d = 0; d < 4; d++) { }
}
static mutex_t           counter_mutex;
static semaphore_t       counter_sem;      /* binary semaphore (nilai awal 1) */
static semaphore_t       done_sem;         /* sinyal selesai dari worker ke shell */

static void worker_unsafe(void) {
    tcb_t *me = thread_get(thread_current());
    for (uint32_t i = 0; i < RACE_ITER; i++) {
        uint32_t tmp = shared_counter;     /* 1. load  */
        critical_work();
        tmp = tmp + 1;                     /* 2. add   */
        shared_counter = tmp;              /* 3. store */
    }
    me->step_count = RACE_ITER;
    sem_post(&done_sem);
}

static void worker_mutex(void) {
    tcb_t *me = thread_get(thread_current());
    for (uint32_t i = 0; i < RACE_ITER; i++) {
        mutex_lock(&counter_mutex);
        uint32_t tmp = shared_counter;
        critical_work();
        tmp = tmp + 1;
        shared_counter = tmp;
        mutex_unlock(&counter_mutex);
    }
    me->step_count = RACE_ITER;
    sem_post(&done_sem);
}

static void worker_sem(void) {
    tcb_t *me = thread_get(thread_current());
    for (uint32_t i = 0; i < RACE_ITER; i++) {
        sem_wait(&counter_sem);
        uint32_t tmp = shared_counter;
        critical_work();
        tmp = tmp + 1;
        shared_counter = tmp;
        sem_post(&counter_sem);
    }
    me->step_count = RACE_ITER;
    sem_post(&done_sem);
}

static uint32_t run_test(const char *title, void (*worker)(void)) {
    vga_print_colored(title, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_putchar('\n');

    shared_counter = 0;
    mutex_init(&counter_mutex, "counter_mutex");
    sem_init(&counter_sem, 1, "counter_sem");
    sem_init(&done_sem, 0, "done_sem");
    sched_stats_reset();

    for (int i = 0; i < RACE_THREADS; i++) {
        thread_create(i == 0 ? "counter-A" : "counter-B", worker);
    }
    thread_ready_all();

    sched_start(1, 0);
    for (int i = 0; i < RACE_THREADS; i++) {
        sem_wait(&done_sem);
    }
    sched_stop();

    uint32_t expected = RACE_ITER * RACE_THREADS;
    uint32_t actual = shared_counter;
    sched_stats_t st = sched_stats();

    vga_print("  Diharapkan : "); vga_print_int(expected); vga_putchar('\n');
    vga_print("  Hasil      : "); vga_print_int(actual);
    if (actual == expected) {
        vga_print_colored("  -> KONSISTEN\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        vga_print_colored("  -> RACE CONDITION", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print(" (hilang ");
        vga_print_int(expected - actual);
        vga_print(" update)\n");
    }
    vga_print("  Context switch: "); vga_print_int(st.switches);
    vga_print(" | preemption: ");     vga_print_int(st.preemptions);
    vga_print(" | blokir lock: ");     vga_print_int(st.blocks);
    vga_print("\n\n");
    return actual;
}

void sync_run_demo(void) {
    vga_print_colored("=== [DEMO TAHAP 4] Sinkronisasi: Race Condition vs Mutex/Semaphore ===\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("2 thread x 2.000.000 increment pada 1 shared counter, quantum 1 ms.\n\n");

    thread_set_verbose(0);
    uint32_t r1 = run_test("[Tes 1] TANPA sinkronisasi", worker_unsafe);
    uint32_t r2 = run_test("[Tes 2] DENGAN mutex (blocking, antrean FIFO)", worker_mutex);
    uint32_t r3 = run_test("[Tes 3] DENGAN binary semaphore", worker_sem);
    thread_set_verbose(1);

    uint32_t expected = RACE_ITER * RACE_THREADS;
    if (r1 != expected && r2 == expected && r3 == expected) {
        vga_print_colored("KESIMPULAN: race terbukti tanpa lock; mutex & semaphore menjaga konsistensi.\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else if (r1 == expected) {
        vga_print_colored("CATATAN: race tidak muncul pada run ini (jalankan 'sync' lagi).\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else {
        vga_print_colored("PERINGATAN: sinkronisasi gagal menjaga konsistensi!\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
}
