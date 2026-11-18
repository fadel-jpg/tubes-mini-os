#ifndef SECURITY_H
#define SECURITY_H

#include <stdint.h>

/* =============================================================================
 * include/security.h - Proteksi & Hak Akses (Tahap 8)
 *
 * Model: kontrol hak akses sederhana (pilihan "kontrol hak akses" pada soal).
 *   - Ada beberapa akun pengguna (uid). root (uid 0) = superuser.
 *   - Pengguna aktif ganti lewat 'su' dengan autentikasi password.
 *   - Berkas punya pemilik + mode izin; VFS memeriksa izin pada tiap operasi.
 *   - Perintah sistem berbahaya (reboot, halt, chown) hanya untuk root.
 * ============================================================================= */

#define SEC_UID_ROOT 0u

void        security_init(void);                 /* mulai sebagai root */
uint32_t    security_uid(void);                  /* uid pengguna aktif */
int         security_is_root(void);
const char *security_username(uint32_t uid);     /* "?" bila tidak dikenal */
int         security_find_uid(const char *name); /* -1 bila tidak ada */

int         security_verify(const char *name, const char *password); /* 1 = cocok */
int         security_login(const char *name, const char *password);  /* verifikasi + ganti user */
int         security_switch_noauth(const char *name);                /* khusus root */

int         security_user_count(void);
const char *security_user_name_at(int index);
uint32_t    security_user_uid_at(int index);

#endif /* SECURITY_H */
