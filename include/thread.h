#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include <stddef.h>

#define MAX_THREADS       8
#define THREAD_STACK_SIZE 8192

/* Siklus Status Thread sesuai spesifikasi Tugas Besar (Tahap 2) */
typedef enum {
    THREAD_UNUSED = 0,
    THREAD_NEW,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_TERMINATED
} thread_state_t;

/* Struktur Data Thread Control Block (TCB) */
typedef struct {
    uint32_t       *esp;        /* Saved Stack Pointer untuk Context Switch */
    uint32_t        tid;        /* Thread ID unik */
    char            name[32];   /* Nama tugas/thread */
    thread_state_t  state;      /* Status thread saat ini */
    void          (*entry)(void); /* Fungsi tugas yang dijalankan thread */
    uint32_t        step_count; /* Counter progres kerja thread */
    void           *wait_obj;   /* Objek sinkronisasi (mutex/semaphore) yang ditunggu; NULL = tidak ada */
    volatile uint32_t cpu_ticks; /* Jumlah tick timer saat thread ini berstatus RUNNING (Tahap 3) */
} tcb_t;

/* Fungsi Assembly untuk menukar Stack Pointer (ESP) antar-thread */
extern void context_switch(uint32_t **old_esp, uint32_t *new_esp);

/* API Pengelolaan Thread (Tahap 2) */
void thread_init(void);
int  thread_create(const char *name, void (*entry)(void));
void thread_ready_all(void);
void thread_yield(void);
void thread_wait(void);
void thread_wake_all(void);
void thread_exit(void);
void thread_print_table(void);
void thread_run_demo(void);

/* API tambahan untuk Preemptive Scheduler & Sinkronisasi (Tahap 3 & 4) */
tcb_t *thread_get(int tid);
int    thread_current(void);
void   thread_set_verbose(int on);
void   thread_set_state(int tid, thread_state_t st, const char *reason, int log);
/* Pindah CPU ke next_tid. Pemanggil WAJIB sudah mematikan interrupt (IF=0). */
void   thread_switch_to(int next_tid, thread_state_t prev_state);

/* Helper untuk mendapatkan string nama status thread */
const char *thread_state_str(thread_state_t state);

#endif /* THREAD_H */
