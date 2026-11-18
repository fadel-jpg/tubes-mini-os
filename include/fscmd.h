#ifndef FSCMD_H
#define FSCMD_H

/* =============================================================================
 * include/fscmd.h - Perintah CLI untuk VFS (Tahap 7) dan Proteksi (Tahap 8)
 *
 * Dipisah dari shell.c supaya perubahan di shell.c minimal.
 * ============================================================================= */

/* Coba jalankan baris perintah sebagai perintah berkas/pengguna.
 * Return 1 bila perintah dikenali (sudah dijalankan), 0 bila bukan perintah ini. */
int  fscmd_try(const char *line);

/* Cetak prompt "user@dalfon# " (root) atau "user@dalfon$ " (pengguna biasa). */
void fscmd_print_prompt(void);

/* Dipakai perintah sistem yang hanya boleh dijalankan root (reboot, halt, ...).
 * Return 1 bila boleh; bila tidak, mencetak "Permission denied" lalu return 0. */
int  fscmd_require_root(const char *cmd);

#endif /* FSCMD_H */
