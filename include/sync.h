#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include "thread.h"

/* Antrean tunggu FIFO (ring buffer) berisi TID thread yang diblokir */
typedef struct {
    int buf[MAX_THREADS];
    int head;
    int count;
} waitq_t;

/* Mutex blocking dengan ownership dan antrean tunggu FIFO */
typedef struct {
    volatile int locked;
    int          owner;       /* TID pemilik, -1 jika bebas */
    const char  *name;
    waitq_t      waiters;
} mutex_t;

/* Counting semaphore dengan antrean tunggu FIFO */
typedef struct {
    volatile int count;
    const char  *name;
    waitq_t      waiters;
} semaphore_t;

void mutex_init(mutex_t *m, const char *name);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

void sem_init(semaphore_t *s, int initial, const char *name);
void sem_wait(semaphore_t *s);
void sem_post(semaphore_t *s);

/* Demo Tahap 4: race condition vs mutex vs semaphore pada shared counter */
void sync_run_demo(void);

#endif /* SYNC_H */
