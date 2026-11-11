/* =============================================================================
 * kernel/thread.c - Pengelolaan Proses & Kernel Thread (Tahap 2)
 * Mengimplementasikan Thread Control Block (TCB), Context Switching nyata via
 * pertukaran register ESP, dan transisi status:
 * NEW -> READY -> RUNNING -> WAITING -> READY -> RUNNING -> TERMINATED
 * ============================================================================= */

#include "thread.h"
#include "vga.h"
#include "shell.h"
#include "irq.h"
#include "sched.h"

/* Tabel TCB statis dan ruang stack terpisah untuk masing-masing thread */
static tcb_t   thread_table[MAX_THREADS];
static uint8_t thread_stacks[MAX_THREADS][THREAD_STACK_SIZE] __attribute__((aligned(16)));
static int     current_tid = 0;
static int     thread_verbose = 1;   /* 0 = senyapkan log pembuatan/transisi status (dipakai demo sinkronisasi) */

/* Menyalin string dengan batas aman */
static void kstrcpy_safe(char *dst, const char *src, size_t max_len) {
    size_t i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

const char *thread_state_str(thread_state_t state) {
    switch (state) {
        case THREAD_NEW:        return "NEW";
        case THREAD_READY:      return "READY";
        case THREAD_RUNNING:    return "RUNNING";
        case THREAD_WAITING:    return "WAITING";
        case THREAD_TERMINATED: return "TERMINATED";
        default:                return "UNUSED";
    }
}

static vga_color_t thread_state_color(thread_state_t state) {
    switch (state) {
        case THREAD_NEW:        return VGA_COLOR_LIGHT_CYAN;
        case THREAD_READY:      return VGA_COLOR_YELLOW;
        case THREAD_RUNNING:    return VGA_COLOR_LIGHT_GREEN;
        case THREAD_WAITING:    return VGA_COLOR_LIGHT_MAGENTA;
        case THREAD_TERMINATED: return VGA_COLOR_DARK_GREY;
        default:                return VGA_COLOR_LIGHT_GREY;
    }
}

/* Cetak satu baris log perubahan status thread agar mudah diamati saat demo */
static void log_state_change(int tid, thread_state_t old_st, thread_state_t new_st, const char *reason) {
    vga_print("  [TCB #");
    vga_print_int((uint32_t)tid);
    vga_print(" ");
    vga_print(thread_table[tid].name);
    vga_print("] ");
    vga_print_colored(thread_state_str(old_st), thread_state_color(old_st), VGA_COLOR_BLACK);
    vga_print(" -> ");
    vga_print_colored(thread_state_str(new_st), thread_state_color(new_st), VGA_COLOR_BLACK);
    if (reason != NULL) {
        vga_print(" (");
        vga_print(reason);
        vga_print(")");
    }
    vga_putchar('\n');
}

/* Wrapper eksekusi awal setiap kernel thread baru */
static void thread_entry_wrapper(void) {
    /* Thread baru pertama kali dijalankan dari dalam context switch (IF=0), aktifkan interrupt
     * agar timer dapat mem-preempt thread ini. */
    irq_enable();
    int tid = current_tid;
    if (thread_table[tid].entry != NULL) {
        thread_table[tid].entry();
    }
    thread_exit();
}

void thread_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_table[i].esp        = NULL;
        thread_table[i].tid        = (uint32_t)i;
        thread_table[i].name[0]    = '\0';
        thread_table[i].state      = THREAD_UNUSED;
        thread_table[i].entry      = NULL;
        thread_table[i].step_count = 0;
        thread_table[i].wait_obj   = NULL;
        thread_table[i].cpu_ticks  = 0;
    }

    /* Slot 0 mewakili Main Kernel / CLI Shell Thread yang sedang aktif */
    current_tid = 0;
    kstrcpy_safe(thread_table[0].name, "kernel-shell", sizeof(thread_table[0].name));
    thread_table[0].state = THREAD_RUNNING;
}

int thread_create(const char *name, void (*entry)(void)) {
    int slot = -1;
    /* Cari slot TCB yang kosong atau sudah TERMINATED */
    for (int i = 1; i < MAX_THREADS; i++) {
        if (thread_table[i].state == THREAD_UNUSED ||
            thread_table[i].state == THREAD_TERMINATED) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return -1;
    }

    /* Siapkan stack frame awal untuk context_switch:
     * Urutan pop di context_switch: EDI, ESI, EBX, EBP, lalu RET (ke EIP). */
    uint32_t *stack_top = (uint32_t *)(thread_stacks[slot] + THREAD_STACK_SIZE);

    *(--stack_top) = (uint32_t)thread_entry_wrapper; /* EIP (Return Address) */
    *(--stack_top) = 0;                              /* EBP */
    *(--stack_top) = 0;                              /* EBX */
    *(--stack_top) = 0;                              /* ESI */
    *(--stack_top) = 0;                              /* EDI */

    thread_table[slot].esp        = stack_top;
    thread_table[slot].tid        = (uint32_t)slot;
    kstrcpy_safe(thread_table[slot].name, name, sizeof(thread_table[slot].name));
    thread_table[slot].state      = THREAD_NEW;
    thread_table[slot].entry      = entry;
    thread_table[slot].step_count = 0;
    thread_table[slot].wait_obj   = NULL;
    thread_table[slot].cpu_ticks  = 0;

    if (!thread_verbose) {
        return slot;
    }

    vga_print("  [CREATE] Thread #");
    vga_print_int((uint32_t)slot);
    vga_print(" '");
    vga_print(thread_table[slot].name);
    vga_print("' dibuat dengan status awal: ");
    vga_print_colored("NEW\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

    return slot;
}

/* Transisi semua thread berstatus NEW menjadi READY (masuk antrean eksekusi) */
void thread_ready_all(void) {
    for (int i = 1; i < MAX_THREADS; i++) {
        if (thread_table[i].state == THREAD_NEW) {
            if (thread_verbose) log_state_change(i, THREAD_NEW, THREAD_READY, "admitted to ready queue");
            thread_table[i].state = THREAD_READY;
        }
    }
}

/* Membangunkan thread yang sedang WAITING menjadi READY */
void thread_wake_all(void) {
    for (int i = 1; i < MAX_THREADS; i++) {
        /* Hanya bangunkan thread yang menunggu event I/O simulasi; thread yang diblokir
         * mutex/semaphore (wait_obj != NULL) hanya boleh dibangunkan oleh unlock/post. */
        if (thread_table[i].state == THREAD_WAITING && thread_table[i].wait_obj == NULL) {
            if (thread_verbose) log_state_change(i, THREAD_WAITING, THREAD_READY, "I/O or event completed");
            thread_table[i].state = THREAD_READY;
        }
    }
}

/* Memilih thread READY berikutnya secara giliran (Round-Robin) dan melakukan Context Switch */
static void schedule_next(thread_state_t prev_next_state, const char *reason) {
    uint32_t irq_flags = irq_save();   /* pemilihan thread + perubahan status harus atomik */
    int prev_tid = current_tid;
    int next_tid = -1;
    /* Saat preemptive scheduler aktif, kernel-shell (TID 0) ikut dalam antrean Round Robin. */
    int shell_eligible = sched_preempt_active();

    /* Cari thread yang berstatus READY */
    for (int offset = 1; offset < MAX_THREADS; offset++) {
        int candidate = (prev_tid + offset) % MAX_THREADS;
        if ((candidate != 0 || shell_eligible) && thread_table[candidate].state == THREAD_READY) {
            next_tid = candidate;
            break;
        }
    }

    /* Jika tidak ada worker yang READY tetapi ada yang WAITING dan kita di shell (TID 0),
     * bangunkan thread yang WAITING agar simulasi berlanjut. */
    if (next_tid == -1 && prev_tid == 0) {
        thread_wake_all();
        for (int i = 1; i < MAX_THREADS; i++) {
            if (thread_table[i].state == THREAD_READY) {
                next_tid = i;
                break;
            }
        }
    }

    /* Jika tetap tidak ada worker READY, kembali ke thread kernel-shell (TID 0) */
    if (next_tid == -1) {
        next_tid = 0;
    }

    if (next_tid == prev_tid) {
        irq_restore(irq_flags);
        return;
    }

    /* Ubah status thread lama */
    thread_state_t old_state = thread_table[prev_tid].state;
    thread_table[prev_tid].state = prev_next_state;
    if (prev_tid != 0 && thread_verbose) {
        log_state_change(prev_tid, old_state, prev_next_state, reason);
    }

    /* Ubah status thread baru menjadi RUNNING */
    thread_state_t next_old_state = thread_table[next_tid].state;
    thread_table[next_tid].state = THREAD_RUNNING;
    if (next_tid != 0 && thread_verbose) {
        log_state_change(next_tid, next_old_state, THREAD_RUNNING, "context switch CPU");
    }

    current_tid = next_tid;
    sched_note_switch();
    context_switch(&thread_table[prev_tid].esp, thread_table[next_tid].esp);
    irq_restore(irq_flags);            /* kembali ke sini saat thread ini dijadwalkan lagi */
}

/* --- API tambahan untuk scheduler preemptive & sinkronisasi (Tahap 3 & 4) --- */

tcb_t *thread_get(int tid) {
    return &thread_table[tid];
}

int thread_current(void) {
    return current_tid;
}

void thread_set_verbose(int on) {
    thread_verbose = on;
}

void thread_set_state(int tid, thread_state_t st, const char *reason, int log) {
    if (log) {
        log_state_change(tid, thread_table[tid].state, st, reason);
    }
    thread_table[tid].state = st;
}

void thread_switch_to(int next_tid, thread_state_t prev_state) {
    int prev_tid = current_tid;
    if (next_tid == prev_tid) {
        return;
    }
    thread_table[prev_tid].state = prev_state;
    thread_table[next_tid].state = THREAD_RUNNING;
    current_tid = next_tid;
    sched_note_switch();
    context_switch(&thread_table[prev_tid].esp, thread_table[next_tid].esp);
}

void thread_yield(void) {
    schedule_next(THREAD_READY, "yield CPU");
}

void thread_wait(void) {
    schedule_next(THREAD_WAITING, "waiting for resource/event");
}

void thread_exit(void) {
    schedule_next(THREAD_TERMINATED, "task finished");
    /* Tidak akan pernah kembali ke sini karena status sudah TERMINATED */
    while (1) {
        __asm__ volatile ("hlt");
    }
}

void thread_print_table(void) {
    vga_print_colored("----------------------------------------------------------------------\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_print_colored(" TID | Nama Thread        | Status       | Step | Ticks | Stack Ptr\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print_colored("----------------------------------------------------------------------\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state == THREAD_UNUSED) {
            continue;
        }

        vga_print("  ");
        vga_print_int(thread_table[i].tid);
        vga_print("  | ");

        /* Cetak nama dengan padding 18 karakter */
        vga_print(thread_table[i].name);
        size_t len = kstrlen(thread_table[i].name);
        while (len < 18) {
            vga_putchar(' ');
            len++;
        }
        vga_print(" | ");

        /* Cetak status berwarna dengan padding 12 karakter */
        const char *st_str = thread_state_str(thread_table[i].state);
        vga_print_colored(st_str, thread_state_color(thread_table[i].state), VGA_COLOR_BLACK);
        size_t slen = kstrlen(st_str);
        while (slen < 12) {
            vga_putchar(' ');
            slen++;
        }
        vga_print(" |  ");
        vga_print_int(thread_table[i].step_count);
        vga_print("   | ");
        vga_print_int(thread_table[i].cpu_ticks);
        {
            uint32_t v = thread_table[i].cpu_ticks;
            int digits = 1;
            while (v >= 10) { v /= 10; digits++; }
            while (digits < 5) { vga_putchar(' '); digits++; }
        }
        vga_print(" | 0x");

        /* Cetak alamat ESP dalam heksadesimal singkat */
        uint32_t esp_val = (uint32_t)thread_table[i].esp;
        const char *hex = "0123456789ABCDEF";
        for (int shift = 28; shift >= 0; shift -= 4) {
            vga_putchar(hex[(esp_val >> shift) & 0xF]);
        }
        vga_putchar('\n');
    }
    vga_print_colored("----------------------------------------------------------------------\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
}

/* =============================================================================
 * Dua Kernel Thread Berbeda Tugas untuk Pengujian Tahap 2:
 * 1. worker_prime_task : Mencari dan mencetak bilangan prima (CPU-bound task)
 * 2. worker_sensor_task: Simulasi pembacaan sensor I/O yang melakukan WAITING
 * ============================================================================= */

static void worker_prime_task(void) {
    uint32_t primes[2] = { 2, 3 };
    for (int step = 0; step < 2; step++) {
        thread_table[current_tid].step_count++;
        vga_print_colored("    -> [Worker-1 Prime] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_print("Menghitung bilangan prima ke-");
        vga_print_int((uint32_t)(step + 1));
        vga_print(" = ");
        vga_print_int(primes[step]);
        vga_putchar('\n');

        /* Serahkan CPU ke thread lain (RUNNING -> READY) */
        thread_yield();
    }
}

static void worker_sensor_task(void) {
    thread_table[current_tid].step_count++;
    vga_print_colored("    -> [Worker-2 Sensor] ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("Mengirim request baca sensor I/O (masuk status WAITING)...\n");

    /* Simulasi blokir menunggu I/O selesai (RUNNING -> WAITING) */
    thread_wait();

    thread_table[current_tid].step_count++;
    vga_print_colored("    -> [Worker-2 Sensor] ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("Data sensor diterima: Suhu CPU = 42 C. Tugas selesai!\n");
}

void thread_run_demo(void) {
    vga_print_colored("=== [DEMO TAHAP 2] Pengelolaan Proses & Kernel Thread ===\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    /* 1. Fase Pembuatan Thread (Status: NEW) */
    vga_print_colored("\n[Fase 1] Membuat 2 Kernel Thread dengan tugas berbeda (Status: NEW)\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    thread_create("worker-prime", worker_prime_task);
    thread_create("worker-sensor", worker_sensor_task);
    thread_print_table();

    /* 2. Fase Adopsi ke Antrean Ready (Status: NEW -> READY) */
    vga_print_colored("\n[Fase 2] Memasukkan Thread ke Ready Queue (NEW -> READY)\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    thread_ready_all();
    thread_print_table();

    /* 3. Fase Eksekusi & Perpindahan Status (READY <-> RUNNING <-> WAITING -> TERMINATED) */
    vga_print_colored("\n[Fase 3] Eksekusi Context Switching & Transisi Status Thread:\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    while (1) {
        int active_workers = 0;
        for (int i = 1; i < MAX_THREADS; i++) {
            if (thread_table[i].state == THREAD_READY ||
                thread_table[i].state == THREAD_WAITING) {
                active_workers++;
            }
        }
        if (active_workers == 0) {
            break;
        }
        thread_yield();
    }

    /* 4. Tampilkan Tabel TCB Akhir (Status: TERMINATED) */
    vga_print_colored("\n[Fase 4] Seluruh Worker Thread Selesai (Status Akhir: TERMINATED)\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    thread_print_table();
}
