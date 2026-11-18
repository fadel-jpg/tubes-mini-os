/* =============================================================================
 * kernel/deadlock.c - Demonstrasi Deadlock, Deteksi, dan Pencegahan (Tahap 5)
 *
 * Skenario 1 (DEADLOCK) : dua thread memakai dua mutex dengan urutan BERLAWANAN.
 *     T1: lock(A) -> tahan sebentar -> lock(B)
 *     T2: lock(B) -> tahan sebentar -> lock(A)
 *   Keduanya sukses mengambil mutex pertama, lalu saling menunggu mutex kedua
 *   selamanya (status WAITING). Siklus ini dideteksi dengan wait-for graph, lalu
 *   kedua thread dihentikan paksa dan mutex di-reset (pemulihan).
 *
 * Skenario 2 (PENCEGAHAN): kedua thread mengambil mutex dengan URUTAN YANG SAMA
 *   (A lebih dulu, baru B). Circular wait menjadi mustahil sehingga deadlock tidak
 *   pernah terjadi (salah satu dari 4 syarat Coffman sengaja ditiadakan).
 *
 * Mutex yang dipakai adalah mutex blocking dari Tahap 4 (sync.c). Pemilik mutex
 * diketahui dari mutex_t.owner, mutex yang ditunggu dari tcb_t.wait_obj.
 * ============================================================================= */

#include "deadlock.h"
#include "sync.h"
#include "sched.h"
#include "thread.h"
#include "vga.h"
#include "irq.h"

#define DL_WORKERS      2
#define HOLD_MS         100u    /* lama menahan mutex pertama sebelum meminta mutex kedua */
#define WORK_MS         20u     /* lama bekerja di dalam critical section                 */
#define QUANTUM_TICKS   10u     /* time quantum scheduler untuk demo ini (10 ms)          */
#define WAIT_LIMIT_MS   3000u   /* batas tunggu sebelum demo dianggap gagal               */

static mutex_t lock_a;
static mutex_t lock_b;

/* Mutex yang dilacak oleh detektor deadlock */
static mutex_t *const tracked[] = { &lock_a, &lock_b };
#define TRACKED_N ((int)(sizeof(tracked) / sizeof(tracked[0])))

static volatile int finished;
static int          worker_tid[DL_WORKERS];

/* ---------------------------------------------------------------------------
 * Thread pekerja
 * --------------------------------------------------------------------------- */
static void busy_wait_ms(uint32_t ms) {
    uint32_t t0 = sched_ticks();
    while (sched_ticks() - t0 < ms) {
        /* busy loop: thread dapat di-preempt timer, sehingga thread lain tetap berjalan */
    }
}

static void worker_done(void) {
    uint32_t f = irq_save();
    finished++;
    irq_restore(f);
}

/* step_count dipakai sebagai penanda progres yang terlihat di perintah 'ps':
 *   0 = belum punya mutex, 1 = sudah punya mutex pertama, 2 = punya kedua mutex, 3 = selesai */
static void take_two(mutex_t *first, mutex_t *second) {
    tcb_t *me = thread_get(thread_current());
    mutex_lock(first);
    me->step_count = 1;
    busy_wait_ms(HOLD_MS);          /* beri kesempatan thread lain mengambil mutex pertamanya */
    mutex_lock(second);             /* pada skenario 1, thread berhenti selamanya di sini */
    me->step_count = 2;
    busy_wait_ms(WORK_MS);          /* kerja di dalam critical section */
    mutex_unlock(second);
    mutex_unlock(first);
    me->step_count = 3;
    worker_done();
}

static void worker_a_then_b(void) { take_two(&lock_a, &lock_b); }
static void worker_b_then_a(void) { take_two(&lock_b, &lock_a); }

/* ---------------------------------------------------------------------------
 * Deteksi deadlock: wait-for graph
 *   Thread T -> Thread U  jika T berstatus WAITING pada mutex yang dimiliki U.
 *   Karena tiap mutex hanya punya satu pemilik, siklus pada graf = deadlock.
 * --------------------------------------------------------------------------- */
static mutex_t *mutex_waited_by(int tid) {
    tcb_t *t = thread_get(tid);
    if (t->state != THREAD_WAITING || t->wait_obj == NULL) {
        return NULL;
    }
    for (int i = 0; i < TRACKED_N; i++) {
        if ((void *)tracked[i] == t->wait_obj) {
            return tracked[i];
        }
    }
    return NULL;                    /* menunggu semaphore/objek lain: bukan bagian graf ini */
}

int deadlock_detect(int *cycle, int max) {
    int found = 0;
    uint32_t f = irq_save();        /* graf dibaca dalam keadaan konsisten */

    for (int start = 0; start < MAX_THREADS && !found; start++) {
        int path[MAX_THREADS];
        int len = 0;
        int cur = start;

        while (1) {
            for (int k = 0; k < len; k++) {
                if (path[k] == cur) {               /* kembali ke thread yang sudah dilewati */
                    int n = len - k;
                    for (int j = 0; j < n && j < max; j++) {
                        cycle[j] = path[k + j];
                    }
                    found = (n < max) ? n : max;
                    goto done_path;
                }
            }
            path[len++] = cur;
            mutex_t *m = mutex_waited_by(cur);
            if (m == NULL || m->owner < 0) {
                break;                              /* rantai putus: bukan deadlock */
            }
            cur = m->owner;
        }
done_path:
        ;
    }

    irq_restore(f);
    return found;
}

/* ---------------------------------------------------------------------------
 * Alat bantu tampilan dan kontrol thread demo
 * --------------------------------------------------------------------------- */
static void print_thread(int tid) {
    vga_print("#");
    vga_print_int((uint32_t)tid);
    vga_print(" ");
    vga_print(thread_get(tid)->name);
}

static int workers_terminated(void) {
    for (int i = 0; i < DL_WORKERS; i++) {
        if (worker_tid[i] >= 0 && thread_get(worker_tid[i])->state != THREAD_TERMINATED) {
            return 0;
        }
    }
    return 1;
}

/* Pemulihan: hentikan paksa thread yang macet dan reset kedua mutex */
static void recover_workers(void) {
    uint32_t f = irq_save();
    for (int i = 0; i < DL_WORKERS; i++) {
        if (worker_tid[i] >= 0 && thread_get(worker_tid[i])->state != THREAD_TERMINATED) {
            thread_get(worker_tid[i])->wait_obj = NULL;
            thread_set_state(worker_tid[i], THREAD_TERMINATED, NULL, 0);
        }
    }
    mutex_init(&lock_a, "lock_A");
    mutex_init(&lock_b, "lock_B");
    irq_restore(f);
}

typedef enum { RES_DONE, RES_DEADLOCK, RES_TIMEOUT, RES_ERROR } result_t;

/* Jalankan dua thread; shell memantau dengan polling agar CLI tidak pernah ikut macet. */
static result_t run_scenario(const char *name1, void (*entry1)(void),
                             const char *name2, void (*entry2)(void),
                             int *cycle, int *cycle_len, uint32_t *elapsed_ms) {
    mutex_init(&lock_a, "lock_A");
    mutex_init(&lock_b, "lock_B");
    finished = 0;
    *cycle_len = 0;

    thread_set_verbose(0);
    worker_tid[0] = thread_create(name1, entry1);
    worker_tid[1] = thread_create(name2, entry2);
    thread_set_verbose(1);
    if (worker_tid[0] < 0 || worker_tid[1] < 0) {
        recover_workers();
        return RES_ERROR;
    }
    thread_ready_all();

    uint32_t t0 = sched_ticks();
    result_t r = RES_TIMEOUT;
    sched_start(QUANTUM_TICKS, 0);

    while (sched_ticks() - t0 < WAIT_LIMIT_MS) {
        if (finished >= DL_WORKERS) {
            r = RES_DONE;
            break;
        }
        int n = deadlock_detect(cycle, MAX_THREADS);
        if (n > 0) {
            *cycle_len = n;
            r = RES_DEADLOCK;
            break;
        }
        __asm__ volatile ("sti; hlt");   /* tunggu tick berikutnya */
    }
    *elapsed_ms = sched_ticks() - t0;   /* 1 tick = 1 ms (PIT 1000 Hz) */

    if (r == RES_DONE) {
        /* beri kesempatan kedua thread menyelesaikan thread_exit() (status TERMINATED) */
        uint32_t t1 = sched_ticks();
        while (!workers_terminated() && sched_ticks() - t1 < 500) {
            __asm__ volatile ("sti; hlt");
        }
    }
    sched_stop();
    return r;
}

static void print_cycle(const int *cycle, int n) {
    for (int i = 0; i < n; i++) {
        int tid  = cycle[i];
        int next = cycle[(i + 1) % n];
        mutex_t *m = mutex_waited_by(tid);
        vga_print("    ");
        print_thread(tid);
        vga_print_colored("  menunggu ", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_print(m ? m->name : "?");
        vga_print("  milik  ");
        print_thread(next);
        vga_putchar('\n');
    }
}

/* ---------------------------------------------------------------------------
 * Demo Tahap 5 (perintah 'deadlock')
 * --------------------------------------------------------------------------- */
void deadlock_run_demo(void) {
    int cycle[MAX_THREADS];
    int cycle_len = 0;
    uint32_t ms = 0;
    int ok1 = 0, ok2 = 0;

    vga_print_colored("=== [DEMO TAHAP 5] Deadlock: Deteksi & Pencegahan (Lock Ordering) ===\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("2 thread, 2 mutex (lock_A, lock_B), quantum 10 ms.\n\n");

    /* ----- Skenario 1: urutan berlawanan -> deadlock ----- */
    vga_print_colored("[Skenario 1] Urutan BERLAWANAN\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("  T1-AB : lock(A) -> lock(B)     T2-BA : lock(B) -> lock(A)\n");

    result_t r1 = run_scenario("T1-AB", worker_a_then_b, "T2-BA", worker_b_then_a,
                               cycle, &cycle_len, &ms);
    if (r1 == RES_DEADLOCK) {
        ok1 = 1;
        vga_print_colored("  [DEADLOCK TERDETEKSI]", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print(" setelah "); vga_print_int(ms); vga_print(" ms. Siklus menunggu:\n");
        print_cycle(cycle, cycle_len);
        vga_print("  Status thread saat ini (lihat kolom Status = WAITING, Step = 1):\n");
        thread_print_table();
        vga_print("  4 syarat Coffman terpenuhi: mutual exclusion, hold and wait,\n");
        vga_print("  no preemption, dan circular wait (T1 -> lock_B -> T2 -> lock_A -> T1).\n");
        recover_workers();
        vga_print_colored("  [PEMULIHAN]", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_print(" kedua thread dihentikan paksa (TERMINATED), mutex di-reset.\n");
    } else if (r1 == RES_DONE) {
        vga_print_colored("  CATATAN: deadlock tidak terjadi pada run ini (jalankan 'deadlock' lagi).\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else {
        recover_workers();
        vga_print_colored("  PERINGATAN: batas waktu habis tanpa deadlock/selesai; thread dihentikan.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }

    /* ----- Skenario 2: urutan sama -> aman ----- */
    vga_print_colored("\n[Skenario 2] SOLUSI: Lock Ordering (selalu A dulu, baru B)\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("  T1-AB : lock(A) -> lock(B)     T2-AB : lock(A) -> lock(B)\n");

    result_t r2 = run_scenario("T1-AB", worker_a_then_b, "T2-AB", worker_a_then_b,
                               cycle, &cycle_len, &ms);
    if (r2 == RES_DONE) {
        ok2 = 1;
        vga_print_colored("  [SELESAI]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_print(" kedua thread selesai dalam "); vga_print_int(ms);
        vga_print(" ms tanpa deadlock.\n");
        vga_print("  Circular wait mustahil karena semua thread mengambil mutex dengan urutan A < B.\n");
    } else {
        if (r2 == RES_DEADLOCK) {
            vga_print_colored("  PERINGATAN: deadlock masih terjadi!\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        } else {
            vga_print_colored("  PERINGATAN: skenario tidak selesai dalam batas waktu.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        }
        recover_workers();
    }

    vga_putchar('\n');
    if (ok1 && ok2) {
        vga_print_colored("KESIMPULAN: urutan mutex berlawanan menyebabkan deadlock; lock ordering mencegahnya.\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        vga_print_colored("HASIL TIDAK SESUAI HARAPAN: periksa output di atas.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
}
