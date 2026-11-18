#ifndef DEADLOCK_H
#define DEADLOCK_H

/* Cari siklus pada wait-for graph mutex yang dilacak.
 * Mengembalikan panjang siklus (> 0) dan mengisi cycle[] dengan TID thread yang saling
 * menunggu secara melingkar, atau 0 jika tidak ada deadlock. */
int  deadlock_detect(int *cycle, int max);

/* Demo Tahap 5: deadlock (urutan mutex berlawanan) lalu solusi lock ordering */
void deadlock_run_demo(void);

#endif /* DEADLOCK_H */
