#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

/* =============================================================================
 * include/vfs.h - Virtual File System berbasis RAMDisk (Tahap 7)
 *
 * Tabel berkas statis (namespace datar, tanpa direktori). Isi tiap berkas
 * disimpan di heap kernel lewat kmalloc(), jadi hilang saat reboot (RAMDisk).
 * Setiap operasi memeriksa hak akses (lihat Tahap 8: security.h).
 * ============================================================================= */

#define VFS_MAX_FILES 32     /* jumlah berkas maksimum                     */
#define VFS_NAME_MAX  24     /* panjang nama termasuk '\0' (maks 23 huruf) */
#define VFS_MAX_SIZE  4096   /* ukuran isi maksimum per berkas (byte)      */

/* Bit izin (gaya UNIX): tiap kelas (pemilik / lainnya) punya r dan w */
#define VFS_PERM_R 4
#define VFS_PERM_W 2

/* Kode hasil. Nilai >= 0 berarti sukses (untuk read/write = jumlah byte). */
#define VFS_OK       0
#define VFS_ENOENT  (-1)   /* berkas tidak ditemukan                */
#define VFS_EEXIST  (-2)   /* nama sudah dipakai                    */
#define VFS_EACCES  (-3)   /* hak akses ditolak (Permission denied) */
#define VFS_ENOSPC  (-4)   /* tabel berkas penuh                    */
#define VFS_ENOMEM  (-5)   /* heap habis (Out of Memory)            */
#define VFS_EINVAL  (-6)   /* nama / argumen tidak valid            */
#define VFS_EFBIG   (-7)   /* melebihi VFS_MAX_SIZE                 */

typedef struct {
    uint8_t   used;                 /* 1 = slot terpakai                          */
    char      name[VFS_NAME_MAX];
    uint8_t  *data;                 /* hasil kmalloc, NULL bila berkas kosong     */
    uint32_t  size;                 /* jumlah byte isi                            */
    uint32_t  uid;                  /* pemilik berkas                             */
    uint16_t  mode;                 /* izin oktal, mis. 0644 (digit tengah/group
                                       disimpan tetapi belum dipakai)             */
} vfs_file_t;

typedef struct {
    uint32_t size;
    uint32_t uid;
    uint16_t mode;
} vfs_stat_t;

void vfs_init(void);

/* CRUD. Parameter uid = identitas pemanggil (untuk pemeriksaan izin). */
int  vfs_create(const char *name, uint32_t uid, uint16_t mode);                  /* Create */
int  vfs_read  (const char *name, uint32_t uid, void *buf, uint32_t max);         /* Read   */
int  vfs_write (const char *name, uint32_t uid, const void *buf, uint32_t len,
                int append);                                                      /* Update */
int  vfs_delete(const char *name, uint32_t uid);                                  /* Delete */

int  vfs_stat (const char *name, vfs_stat_t *st);
int  vfs_chmod(const char *name, uint32_t uid, uint16_t mode);
int  vfs_chown(const char *name, uint32_t uid, uint32_t new_uid);

/* Untuk perintah ls / df */
const vfs_file_t *vfs_entry(int index);   /* NULL bila slot kosong / di luar rentang */
int      vfs_count(void);
uint32_t vfs_total_bytes(void);

const char *vfs_strerror(int err);
void        vfs_mode_str(uint16_t mode, char out[10]);   /* "rw-r--r--" */

#endif /* VFS_H */
