#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Ukuran heap kernel (disediakan statis di section .bss) */
#define HEAP_SIZE (256 * 1024)

/* Ringkasan kondisi heap. Invarian: used_bytes + free_bytes == total_bytes */
typedef struct {
    uint32_t total_bytes;    /* ukuran seluruh heap                                  */
    uint32_t used_bytes;     /* byte terpakai (header + payload blok terpakai)       */
    uint32_t free_bytes;     /* byte tersedia (total - terpakai)                     */
    uint32_t used_blocks;    /* jumlah blok yang sedang dialokasikan                 */
    uint32_t free_blocks;    /* jumlah blok bebas (>1 berarti heap terfragmentasi)   */
    uint32_t largest_free;   /* payload terbesar yang masih bisa dialokasikan        */
    uint32_t total_allocs;   /* kumulatif kmalloc yang berhasil                      */
    uint32_t total_frees;    /* kumulatif kfree yang valid                           */
    uint32_t failed_allocs;  /* kumulatif kmalloc yang gagal (Out of Memory)         */
    uint32_t invalid_frees;  /* kumulatif kfree yang ditolak (double free, dll)      */
} mem_stats_t;

/* API Alokator Memori Dinamis (Tahap 6) */
void        mem_init(void);
void       *kmalloc(size_t size);   /* NULL jika size == 0 atau memori habis (OOM) */
void        kfree(void *ptr);       /* aman: pointer tidak valid ditolak, tidak crash */
mem_stats_t mem_get_stats(void);
void        mem_print_info(void);   /* perintah CLI: meminfo */
void        mem_run_demo(void);     /* perintah CLI: memtest */

#endif /* MEMORY_H */
