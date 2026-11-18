/* =============================================================================
 * kernel/memory.c - Alokator Memori Dinamis Kernel (Tahap 6)
 *
 * Heap  : area statis HEAP_SIZE (256 KB) di section .bss.
 * Metode: free-list "first-fit" berbasis header blok.
 *   - Seluruh heap selalu terbagi menjadi blok-blok berurutan secara fisik
 *     (doubly linked list: prev/next), masing-masing diawali header 16 byte.
 *   - kmalloc: telusuri blok dari awal, ambil blok BEBAS pertama yang cukup besar.
 *     Jika sisanya cukup untuk blok baru, blok DIPECAH (split).
 *   - kfree  : tandai blok bebas, lalu GABUNGKAN (coalesce) dengan tetangga fisik
 *     yang juga bebas agar fragmentasi tidak menumpuk.
 *   - Ukuran payload dibulatkan ke kelipatan 16 byte, sehingga alamat yang
 *     dikembalikan selalu rata 16 byte.
 * Keamanan:
 *   - Out of Memory   : kmalloc mengembalikan NULL (tidak crash, tidak panic).
 *   - kfree tidak valid (di luar heap, tidak rata, double free) ditolak dan dicatat.
 *   - Magic number di header mendeteksi header rusak/korup.
 *   - Seluruh operasi dilindungi irq_save/irq_restore karena thread dapat
 *     di-preempt oleh timer kapan saja (kernel single-CPU).
 * ============================================================================= */

#include "memory.h"
#include "vga.h"
#include "irq.h"

#define HEAP_ALIGN   16u
#define BLOCK_USED   0xA110CA7Eu
#define BLOCK_FREE   0xF4EEB10Cu

typedef struct block {
    uint32_t      magic;   /* BLOCK_USED atau BLOCK_FREE                    */
    uint32_t      size;    /* ukuran payload (kelipatan 16), tanpa header   */
    struct block *prev;    /* blok sebelumnya secara fisik                  */
    struct block *next;    /* blok sesudahnya secara fisik                  */
} block_t;

#define HDR ((uint32_t)sizeof(block_t))

_Static_assert(sizeof(block_t) == 16, "header harus 16 byte agar payload selalu rata 16 byte");

static uint8_t  heap_area[HEAP_SIZE] __attribute__((aligned(16)));
static block_t *heap_head = NULL;

static uint32_t total_allocs  = 0;
static uint32_t total_frees   = 0;
static uint32_t failed_allocs = 0;
static uint32_t invalid_frees = 0;

/* ---------------------------------------------------------------------------
 * Inti alokator
 * --------------------------------------------------------------------------- */
static void heap_reset_locked(void) {
    heap_head = (block_t *)heap_area;
    heap_head->magic = BLOCK_FREE;
    heap_head->size  = HEAP_SIZE - HDR;
    heap_head->prev  = NULL;
    heap_head->next  = NULL;
    total_allocs = total_frees = failed_allocs = invalid_frees = 0;
}

void mem_init(void) {
    uint32_t f = irq_save();
    heap_reset_locked();
    irq_restore(f);
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    uint32_t f = irq_save();
    if (heap_head == NULL) {
        heap_reset_locked();            /* inisialisasi malas jika mem_init belum dipanggil */
    }

    /* Tolak lebih awal agar pembulatan ke atas tidak pernah overflow */
    if (size > HEAP_SIZE) {
        failed_allocs++;
        irq_restore(f);
        return NULL;
    }

    uint32_t need = ((uint32_t)size + (HEAP_ALIGN - 1)) & ~(HEAP_ALIGN - 1);

    for (block_t *b = heap_head; b != NULL; b = b->next) {
        if (b->magic != BLOCK_USED && b->magic != BLOCK_FREE) {
            break;                      /* header korup: hentikan, anggap memori habis */
        }
        if (b->magic == BLOCK_FREE && b->size >= need) {
            /* Pecah blok bila sisanya cukup untuk header + minimal 16 byte payload */
            if (b->size >= need + HDR + HEAP_ALIGN) {
                block_t *n = (block_t *)((uint8_t *)b + HDR + need);
                n->magic = BLOCK_FREE;
                n->size  = b->size - need - HDR;
                n->prev  = b;
                n->next  = b->next;
                if (b->next != NULL) {
                    b->next->prev = n;
                }
                b->next = n;
                b->size = need;
            }
            b->magic = BLOCK_USED;
            total_allocs++;
            irq_restore(f);
            return (uint8_t *)b + HDR;
        }
    }

    failed_allocs++;                    /* Out of Memory: kembalikan NULL, kernel tetap hidup */
    irq_restore(f);
    return NULL;
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;                         /* kfree(NULL) tidak melakukan apa pun (seperti free()) */
    }

    uint32_t f = irq_save();
    uint8_t *p = (uint8_t *)ptr;

    /* Validasi: harus di dalam heap dan rata 16 byte */
    if (heap_head == NULL ||
        p < heap_area + HDR || p >= heap_area + HEAP_SIZE ||
        (((uint32_t)(p - heap_area)) % HEAP_ALIGN) != 0) {
        invalid_frees++;
        irq_restore(f);
        return;
    }

    block_t *b = (block_t *)(p - HDR);
    if (b->magic != BLOCK_USED) {       /* sudah bebas (double free) atau bukan blok valid */
        invalid_frees++;
        irq_restore(f);
        return;
    }

    b->magic = BLOCK_FREE;
    total_frees++;

    /* Gabungkan dengan blok berikutnya jika bebas */
    if (b->next != NULL && b->next->magic == BLOCK_FREE) {
        block_t *n = b->next;
        b->size += HDR + n->size;
        b->next = n->next;
        if (n->next != NULL) {
            n->next->prev = b;
        }
        n->magic = 0;
    }
    /* Gabungkan dengan blok sebelumnya jika bebas */
    if (b->prev != NULL && b->prev->magic == BLOCK_FREE) {
        block_t *pv = b->prev;
        pv->size += HDR + b->size;
        pv->next = b->next;
        if (b->next != NULL) {
            b->next->prev = pv;
        }
        b->magic = 0;
    }
    irq_restore(f);
}

mem_stats_t mem_get_stats(void) {
    mem_stats_t s;
    s.total_bytes = HEAP_SIZE;
    s.used_bytes = s.used_blocks = s.free_blocks = s.largest_free = 0;

    uint32_t f = irq_save();
    for (block_t *b = heap_head; b != NULL; b = b->next) {
        if (b->magic == BLOCK_USED) {
            s.used_bytes += HDR + b->size;
            s.used_blocks++;
        } else if (b->magic == BLOCK_FREE) {
            s.free_blocks++;
            if (b->size > s.largest_free) {
                s.largest_free = b->size;
            }
        } else {
            break;
        }
    }
    s.total_allocs  = total_allocs;
    s.total_frees   = total_frees;
    s.failed_allocs = failed_allocs;
    s.invalid_frees = invalid_frees;
    irq_restore(f);

    s.free_bytes = s.total_bytes - s.used_bytes;
    return s;
}

/* ---------------------------------------------------------------------------
 * Tampilan: perintah 'meminfo'
 * --------------------------------------------------------------------------- */
static void print_hex(uint32_t v) {
    static const char hex[] = "0123456789ABCDEF";
    vga_print("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        vga_putchar(hex[(v >> shift) & 0xF]);
    }
}

/* Cetak "12345 B (12 KB)" */
static void print_bytes(uint32_t bytes) {
    vga_print_int(bytes);
    vga_print(" B");
    if (bytes >= 1024) {
        vga_print(" (");
        vga_print_int(bytes / 1024);
        vga_print(" KB)");
    }
}

void mem_print_info(void) {
    mem_stats_t s = mem_get_stats();

    vga_print_colored("=== [MEMINFO] Heap Kernel (first-fit, split & coalesce) ===\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("  Rentang heap : "); print_hex((uint32_t)heap_area);
    vga_print(" - ");              print_hex((uint32_t)heap_area + HEAP_SIZE);
    vga_print("  | "); print_bytes(s.total_bytes); vga_putchar('\n');

    vga_print("  Terpakai     : ");
    print_bytes(s.used_bytes);
    vga_print("  | "); vga_print_int(s.used_blocks); vga_print(" blok\n");

    vga_print("  Tersedia     : ");
    print_bytes(s.free_bytes);
    vga_print("  | "); vga_print_int(s.free_blocks); vga_print(" blok bebas\n");

    vga_print("  Blok bebas terbesar : "); print_bytes(s.largest_free); vga_putchar('\n');

    vga_print("  Statistik    : alokasi="); vga_print_int(s.total_allocs);
    vga_print(" | free=");                  vga_print_int(s.total_frees);
    vga_print(" | gagal(OOM)=");             vga_print_int(s.failed_allocs);
    vga_print(" | free-ditolak=");           vga_print_int(s.invalid_frees);
    vga_putchar('\n');
}

/* =============================================================================
 * Demo Tahap 6 ('memtest'): membuktikan alokasi, pelepasan, perubahan kapasitas
 * terpakai/tersedia, penggabungan blok, dan penanganan Out of Memory tanpa crash.
 * ============================================================================= */

#define CHUNK_SIZE   8192
#define MAX_CHUNKS   64

static void *chunks[MAX_CHUNKS];
static int   checks_failed;

static void check(const char *what, int ok) {
    vga_print("  ");
    if (ok) {
        vga_print_colored("[PASS] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        vga_print_colored("[FAIL] ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        checks_failed++;
    }
    vga_print(what);
    vga_putchar('\n');
}

static void fill(void *p, uint32_t n, uint8_t v) {
    uint8_t *q = (uint8_t *)p;
    for (uint32_t i = 0; i < n; i++) {
        q[i] = v;
    }
}

static int verify(const void *p, uint32_t n, uint8_t v) {
    const uint8_t *q = (const uint8_t *)p;
    for (uint32_t i = 0; i < n; i++) {
        if (q[i] != v) {
            return 0;
        }
    }
    return 1;
}

static void print_usage(void) {
    mem_stats_t s = mem_get_stats();
    vga_print("  >> Terpakai: ");  vga_print_int(s.used_bytes);
    vga_print(" B | Tersedia: ");  vga_print_int(s.free_bytes);
    vga_print(" B | blok bebas: "); vga_print_int(s.free_blocks);
    vga_putchar('\n');
}

void mem_run_demo(void) {
    checks_failed = 0;
    vga_print_colored("=== [DEMO TAHAP 6] Alokator Memori Dinamis (kmalloc/kfree) ===\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    mem_stats_t base = mem_get_stats();

    /* --- Tes 1: kondisi awal --- */
    vga_print_colored("[Tes 1] Kondisi awal heap\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    mem_print_info();

    /* --- Tes 2: alokasi dan integritas data --- */
    vga_print_colored("\n[Tes 2] kmalloc 3 blok: A=1000 B, B=4000 B, C=16000 B\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    void *a = kmalloc(1000);
    void *b = kmalloc(4000);
    void *c = kmalloc(16000);
    vga_print("  A="); print_hex((uint32_t)a);
    vga_print("  B="); print_hex((uint32_t)b);
    vga_print("  C="); print_hex((uint32_t)c);
    vga_putchar('\n');
    check("ketiga alokasi berhasil dan alamat rata 16 byte",
          a && b && c && (((uint32_t)a | (uint32_t)b | (uint32_t)c) % 16) == 0);
    if (a && b && c) {
        fill(a, 1000, 0xA1); fill(b, 4000, 0xB2); fill(c, 16000, 0xC3);
        check("data tiap blok utuh, tidak saling menimpa",
              verify(a, 1000, 0xA1) && verify(b, 4000, 0xB2) && verify(c, 16000, 0xC3));
    }
    print_usage();
    mem_stats_t s2 = mem_get_stats();
    check("memori terpakai bertambah, memori tersedia berkurang",
          s2.used_bytes > base.used_bytes && s2.free_bytes < base.free_bytes);

    /* --- Tes 3: pelepasan, fragmentasi, pemakaian ulang, penggabungan --- */
    vga_print_colored("\n[Tes 3] kfree(B) -> lubang di tengah, lalu kmalloc(2000) memakainya\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kfree(b);
    print_usage();
    mem_stats_t s3 = mem_get_stats();
    check("setelah kfree(B): terpakai turun, ada 2 blok bebas (fragmentasi)",
          s3.used_bytes < s2.used_bytes && s3.free_blocks == 2);
    void *d = kmalloc(2000);
    check("kmalloc(2000) menempati lubang bekas B (first-fit)",
          d != NULL && (uint8_t *)d >= (uint8_t *)b && (uint8_t *)d < (uint8_t *)b + 4000);
    kfree(d);
    kfree(a);
    kfree(c);
    print_usage();
    mem_stats_t s4 = mem_get_stats();
    check("semua dibebaskan: blok bebas bergabung jadi 1 (coalescing)", s4.free_blocks == 1);
    check("memori terpakai kembali seperti kondisi awal", s4.used_bytes == base.used_bytes);

    /* --- Tes 4: Out of Memory --- */
    vga_print_colored("\n[Tes 4] Out of Memory: kmalloc(8 KB) berulang sampai heap habis\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    int n = 0;
    while (n < MAX_CHUNKS && (chunks[n] = kmalloc(CHUNK_SIZE)) != NULL) {
        fill(chunks[n], CHUNK_SIZE, (uint8_t)(n + 1));
        n++;
    }
    vga_print("  Berhasil "); vga_print_int((uint32_t)n);
    vga_print(" blok (");     vga_print_int((uint32_t)n * CHUNK_SIZE / 1024);
    vga_print(" KB); alokasi berikutnya mengembalikan NULL.\n");
    print_usage();
    check("heap habis ditangani: kmalloc mengembalikan NULL, kernel tidak crash",
          n > 0 && n < MAX_CHUNKS && chunks[n] == NULL);
    int intact = 1;
    for (int i = 0; i < n; i++) {
        if (!verify(chunks[i], CHUNK_SIZE, (uint8_t)(i + 1))) {
            intact = 0;
        }
    }
    check("data semua blok tetap utuh saat memori penuh", intact);
    uint32_t failed_before = mem_get_stats().failed_allocs;
    void *huge = kmalloc(HEAP_SIZE * 2);
    check("permintaan melebihi ukuran heap ditolak (NULL)",
          huge == NULL && mem_get_stats().failed_allocs == failed_before + 1);

    for (int i = 0; i < n; i++) {
        kfree(chunks[i]);
        chunks[i] = NULL;
    }
    mem_stats_t s5 = mem_get_stats();
    check("semua blok dilepas: memori pulih seperti awal (1 blok bebas)",
          s5.used_bytes == base.used_bytes && s5.free_blocks == 1);
    void *again = kmalloc(100000);
    check("setelah pulih, kmalloc(100000) berhasil lagi", again != NULL);
    kfree(again);

    /* --- Tes 5: kfree tidak valid ditolak --- */
    vga_print_colored("\n[Tes 5] kfree tidak valid: double free, alamat luar heap, alamat tidak rata\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    uint32_t inv_before = mem_get_stats().invalid_frees;
    void *x = kmalloc(64);
    kfree(x);
    kfree(x);                       /* double free */
    kfree((void *)0x12345);         /* di luar heap */
    kfree((uint8_t *)x + 4);        /* alamat tidak rata */
    check("3 kfree tidak valid ditolak dan dicatat, kernel tidak crash",
          mem_get_stats().invalid_frees == inv_before + 3);
    check("heap tetap konsisten setelah kfree tidak valid",
          mem_get_stats().used_bytes == base.used_bytes && mem_get_stats().free_blocks == 1);

    vga_putchar('\n');
    if (checks_failed == 0) {
        vga_print_colored("KESIMPULAN: semua pengujian alokator memori LULUS.\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        vga_print_colored("PERINGATAN: ada pengujian yang GAGAL: ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print_int((uint32_t)checks_failed);
        vga_putchar('\n');
    }
}
