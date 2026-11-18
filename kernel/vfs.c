/* =============================================================================
 * kernel/vfs.c - Virtual File System RAMDisk (Tahap 7)
 *
 * Struktur   : tabel berkas statis (vfs_file_t files[VFS_MAX_FILES]).
 * Penyimpanan: isi berkas dialokasikan dengan kmalloc() dari heap kernel (Tahap 6)
 *              dan dibebaskan dengan kfree() saat berkas ditimpa / dihapus.
 * Izin       : root (uid 0) boleh semuanya; pemilik memakai bit "owner";
 *              pengguna lain memakai bit "others" (lihat perm_ok()).
 * Catatan    : fungsi di sini hanya dipanggil dari thread shell, jadi tidak
 *              memakai kunci sendiri (kmalloc/kfree sudah aman terhadap interrupt).
 * ============================================================================= */

#include "vfs.h"
#include "memory.h"

static vfs_file_t files[VFS_MAX_FILES];

/* ---------------------------------------------------------------------------
 * Helper string/memori sendiri (kernel dibangun -nostdlib, tanpa libc).
 * Dipisah per-fungsi dan diberi atribut agar GCC tidak mengubah loop ini
 * menjadi panggilan memcpy/memset (yang tidak kita punya) saat -O2.
 * --------------------------------------------------------------------------- */
#define NO_LIBCALL __attribute__((noinline, optimize("no-tree-loop-distribute-patterns")))

static NO_LIBCALL size_t v_strlen(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static NO_LIBCALL int v_streq(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static NO_LIBCALL void v_copy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static NO_LIBCALL void v_zero(void *dst, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) {
        d[i] = 0;
    }
}

/* ---------------------------------------------------------------------------
 * Validasi & pencarian
 * --------------------------------------------------------------------------- */

/* Nama berkas: 1..23 karakter dari huruf, angka, '.', '_', '-' */
static int name_valid(const char *n) {
    size_t len = v_strlen(n);
    if (len == 0 || len >= VFS_NAME_MAX) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        char c = n[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) {
            return 0;
        }
    }
    return 1;
}

static vfs_file_t *find_file(const char *name) {
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (files[i].used && v_streq(files[i].name, name)) {
            return &files[i];
        }
    }
    return NULL;
}

/* Pemeriksaan hak akses. want = VFS_PERM_R atau VFS_PERM_W.
 * mode oktal: bit 8..6 = pemilik, bit 2..0 = pengguna lain. */
static int perm_ok(const vfs_file_t *f, uint32_t uid, int want) {
    if (uid == 0) {
        return 1;                                   /* root: superuser */
    }
    int bits = (uid == f->uid) ? (f->mode >> 6) & 7 : f->mode & 7;
    return (bits & want) == want;
}

/* ---------------------------------------------------------------------------
 * Inisialisasi
 * --------------------------------------------------------------------------- */
void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (files[i].used && files[i].data != NULL) {
            kfree(files[i].data);
        }
        v_zero(&files[i], sizeof(files[i]));
    }
}

/* ---------------------------------------------------------------------------
 * C - Create
 * --------------------------------------------------------------------------- */
int vfs_create(const char *name, uint32_t uid, uint16_t mode) {
    if (name == NULL || !name_valid(name)) {
        return VFS_EINVAL;
    }
    if (find_file(name) != NULL) {
        return VFS_EEXIST;
    }
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!files[i].used) {
            v_zero(&files[i], sizeof(files[i]));
            files[i].used = 1;
            v_copy(files[i].name, name, v_strlen(name) + 1);
            files[i].data = NULL;
            files[i].size = 0;
            files[i].uid  = uid;
            files[i].mode = mode & 0666;
            return VFS_OK;
        }
    }
    return VFS_ENOSPC;
}

/* ---------------------------------------------------------------------------
 * R - Read: salin sampai max byte ke buf, kembalikan jumlah byte yang dibaca
 * --------------------------------------------------------------------------- */
int vfs_read(const char *name, uint32_t uid, void *buf, uint32_t max) {
    vfs_file_t *f = find_file(name);
    if (f == NULL) {
        return VFS_ENOENT;
    }
    if (!perm_ok(f, uid, VFS_PERM_R)) {
        return VFS_EACCES;
    }
    uint32_t n = (f->size < max) ? f->size : max;
    if (n > 0) {
        v_copy(buf, f->data, n);
    }
    return (int)n;
}

/* ---------------------------------------------------------------------------
 * U - Update: timpa (append = 0) atau tambah di akhir (append = 1)
 * Buffer baru dialokasikan DULU; baru setelah berhasil buffer lama dibebaskan,
 * sehingga bila heap habis isi lama tetap utuh.
 * --------------------------------------------------------------------------- */
int vfs_write(const char *name, uint32_t uid, const void *buf, uint32_t len, int append) {
    vfs_file_t *f = find_file(name);
    if (f == NULL) {
        return VFS_ENOENT;
    }
    if (!perm_ok(f, uid, VFS_PERM_W)) {
        return VFS_EACCES;
    }

    uint32_t base = append ? f->size : 0;
    if (len > VFS_MAX_SIZE || base + len > VFS_MAX_SIZE) {
        return VFS_EFBIG;
    }

    uint32_t newlen = base + len;
    if (newlen == 0) {                       /* menimpa dengan isi kosong */
        if (f->data != NULL) {
            kfree(f->data);
        }
        f->data = NULL;
        f->size = 0;
        return 0;
    }

    uint8_t *p = (uint8_t *)kmalloc(newlen);
    if (p == NULL) {
        return VFS_ENOMEM;
    }
    if (base > 0) {
        v_copy(p, f->data, base);            /* isi lama (hanya untuk append) */
    }
    if (len > 0) {
        v_copy(p + base, buf, len);
    }
    if (f->data != NULL) {
        kfree(f->data);
    }
    f->data = p;
    f->size = newlen;
    return (int)len;
}

/* ---------------------------------------------------------------------------
 * D - Delete: hanya pemilik atau root
 * --------------------------------------------------------------------------- */
int vfs_delete(const char *name, uint32_t uid) {
    vfs_file_t *f = find_file(name);
    if (f == NULL) {
        return VFS_ENOENT;
    }
    if (uid != 0 && uid != f->uid) {
        return VFS_EACCES;
    }
    if (f->data != NULL) {
        kfree(f->data);
    }
    v_zero(f, sizeof(*f));
    return VFS_OK;
}

/* ---------------------------------------------------------------------------
 * Metadata
 * --------------------------------------------------------------------------- */
int vfs_stat(const char *name, vfs_stat_t *st) {
    vfs_file_t *f = find_file(name);
    if (f == NULL) {
        return VFS_ENOENT;
    }
    st->size = f->size;
    st->uid  = f->uid;
    st->mode = f->mode;
    return VFS_OK;
}

int vfs_chmod(const char *name, uint32_t uid, uint16_t mode) {
    vfs_file_t *f = find_file(name);
    if (f == NULL) {
        return VFS_ENOENT;
    }
    if (uid != 0 && uid != f->uid) {
        return VFS_EACCES;                   /* hanya pemilik atau root */
    }
    f->mode = mode & 0666;
    return VFS_OK;
}

int vfs_chown(const char *name, uint32_t uid, uint32_t new_uid) {
    vfs_file_t *f = find_file(name);
    if (f == NULL) {
        return VFS_ENOENT;
    }
    if (uid != 0) {
        return VFS_EACCES;                   /* hanya root */
    }
    f->uid = new_uid;
    return VFS_OK;
}

/* ---------------------------------------------------------------------------
 * Info untuk perintah ls / df
 * --------------------------------------------------------------------------- */
const vfs_file_t *vfs_entry(int index) {
    if (index < 0 || index >= VFS_MAX_FILES || !files[index].used) {
        return NULL;
    }
    return &files[index];
}

int vfs_count(void) {
    int n = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (files[i].used) {
            n++;
        }
    }
    return n;
}

uint32_t vfs_total_bytes(void) {
    uint32_t n = 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (files[i].used) {
            n += files[i].size;
        }
    }
    return n;
}

const char *vfs_strerror(int err) {
    switch (err) {
        case VFS_OK:      return "OK";
        case VFS_ENOENT:  return "berkas tidak ditemukan";
        case VFS_EEXIST:  return "berkas sudah ada";
        case VFS_EACCES:  return "Permission denied (akses ditolak)";
        case VFS_ENOSPC:  return "tabel berkas penuh";
        case VFS_ENOMEM:  return "memori tidak cukup (Out of Memory)";
        case VFS_EINVAL:  return "nama berkas tidak valid";
        case VFS_EFBIG:   return "ukuran berkas melebihi batas";
        default:          return "error tidak dikenal";
    }
}

/* mode 0644 -> "rw-r--r--" (kolom x selalu '-', belum ada berkas eksekusi) */
void vfs_mode_str(uint16_t mode, char out[10]) {
    for (int g = 0; g < 3; g++) {
        int bits = (mode >> (6 - 3 * g)) & 7;
        out[g * 3 + 0] = (bits & 4) ? 'r' : '-';
        out[g * 3 + 1] = (bits & 2) ? 'w' : '-';
        out[g * 3 + 2] = '-';
    }
    out[9] = '\0';
}
