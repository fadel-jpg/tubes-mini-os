/* =============================================================================
 * tests/test_vfs.c - Uji VFS (Tahap 7) dan Proteksi (Tahap 8) di luar QEMU.
 *
 * Yang dijalankan adalah kode ASLI: kernel/vfs.c, kernel/security.c,
 * kernel/fscmd.c dan alokator kernel/memory.c (milik Tahap 6).
 * Yang ditiru hanya layar (vga_*), keyboard (diisi skrip) dan irq_save/restore.
 * Program 32-bit tanpa libc; keluaran lewat syscall write.
 * Jalankan: make test-vfs
 * ============================================================================= */
#include <stdint.h>
#include <stddef.h>
#include "vga.h"
#include "keyboard.h"
#include "shell.h"
#include "memory.h"
#include "vfs.h"
#include "security.h"
#include "fscmd.h"

static inline int sys3(int n, int a, int b, int c) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}
static void out(const char *s) { int n = 0; while (s[n]) n++; sys3(4, 1, (int)(uintptr_t)s, n); }
static void die(int code) { sys3(1, code, 0, 0); for (;;) {} }

/* ---- fungsi shell.c yang dipakai fscmd.c ---- */
size_t kstrlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

/* ---- layar palsu: keluaran ditampung supaya bisa diperiksa ---- */
static char cap[65536];
static size_t cap_len;
static int echo_screen;
void vga_putchar(char c) {
    if (cap_len < sizeof(cap) - 1) { cap[cap_len++] = c; cap[cap_len] = 0; }
    if (echo_screen) { sys3(4, 1, (int)(uintptr_t)&c, 1); }
}
void vga_print(const char *s) { while (*s) vga_putchar(*s++); }
void vga_print_colored(const char *s, vga_color_t f, vga_color_t b) { (void)f; (void)b; vga_print(s); }
void vga_print_int(uint32_t n) {
    char t[12]; int i = 0;
    if (n == 0) { vga_putchar('0'); return; }
    while (n) { t[i++] = (char)('0' + n % 10); n /= 10; }
    while (i) vga_putchar(t[--i]);
}

/* ---- keyboard palsu: membaca dari skrip ---- */
static const char *kb;
char keyboard_getchar(void) {
    if (kb == NULL || *kb == 0) { out("\n[TEST] skrip keyboard habis\n"); die(99); }
    return *kb++;
}

/* ---- kerangka uji ---- */
static int total, failed;
static void ok(int cond, const char *what) {
    total++;
    if (!cond) { failed++; out("  GAGAL: "); out(what); out("\n"); }
}
static int has(const char *needle) {                    /* needle ada di keluaran? */
    for (size_t i = 0; cap[i]; i++) {
        size_t j = 0;
        while (needle[j] && cap[i + j] == needle[j]) j++;
        if (!needle[j]) return 1;
    }
    return 0;
}
static void clear_cap(void) { cap_len = 0; cap[0] = 0; }
static int run(const char *line) { clear_cap(); return fscmd_try(line); }
static void boot_state(void) {                          /* sama seperti kernel.c */
    mem_init(); security_init(); vfs_init();
    vfs_create("readme.txt", 0, 0644);
    vfs_write("readme.txt", 0, "Selamat datang di Dalfon OS!\n", 29, 0);
    vfs_create("rahasia.txt", 0, 0600);
    vfs_write("rahasia.txt", 0, "hanya root yang boleh membaca ini\n", 34, 0);
}

/* ====================== uji 1: API VFS langsung ====================== */
static void t_vfs_api(void) {
    boot_state();
    char b[64]; vfs_stat_t st;
    ok(vfs_create("a.txt", 0, 0644) == VFS_OK, "create");
    ok(vfs_create("a.txt", 0, 0644) == VFS_EEXIST, "create duplikat");
    ok(vfs_create("bad/name", 0, 0644) == VFS_EINVAL, "nama dengan /");
    ok(vfs_create("", 0, 0644) == VFS_EINVAL, "nama kosong");
    ok(vfs_create("abcdefghijklmnopqrstuvwx", 0, 0644) == VFS_EINVAL, "nama 24 huruf (terlalu panjang)");
    ok(vfs_create("abcdefghijklmnopqrstuvw", 0, 0644) == VFS_OK, "nama 23 huruf (batas)");
    ok(vfs_write("a.txt", 0, "hello", 5, 0) == 5, "write");
    ok(vfs_write("a.txt", 0, " world", 6, 1) == 6, "append");
    ok(vfs_read("a.txt", 0, b, 63) == 11 && !kstrcmp((b[11] = 0, b), "hello world"), "read hasil append");
    ok(vfs_read("a.txt", 0, b, 5) == 5, "read dibatasi max");
    ok(vfs_write("a.txt", 0, "x", 1, 0) == 1, "timpa");
    ok(vfs_read("a.txt", 0, b, 63) == 1 && b[0] == 'x', "read hasil timpa");
    ok(vfs_write("a.txt", 0, "", 0, 0) == 0, "timpa dengan kosong");
    ok(vfs_stat("a.txt", &st) == VFS_OK && st.size == 0, "stat ukuran 0");
    ok(vfs_read("a.txt", 0, b, 63) == 0, "read berkas kosong");
    ok(vfs_delete("a.txt", 0) == VFS_OK, "delete");
    ok(vfs_read("a.txt", 0, b, 8) == VFS_ENOENT, "read setelah delete");
    ok(vfs_delete("a.txt", 0) == VFS_ENOENT, "delete dua kali");
    ok(vfs_write("tidakada", 0, "x", 1, 0) == VFS_ENOENT, "write berkas tidak ada");
}

/* ============== uji 2: batas ukuran, tabel penuh, bocor heap ============== */
static void t_limits(void) {
    boot_state();
    mem_stats_t m0 = mem_get_stats();
    static char big[VFS_MAX_SIZE + 8];
    for (int i = 0; i < (int)sizeof(big); i++) big[i] = 'A';
    vfs_create("big", 0, 0644);
    ok(vfs_write("big", 0, big, VFS_MAX_SIZE + 1, 0) == VFS_EFBIG, "tulis 4097 byte ditolak");
    ok(vfs_write("big", 0, big, VFS_MAX_SIZE, 0) == VFS_MAX_SIZE, "tulis tepat 4096 byte");
    ok(vfs_write("big", 0, "z", 1, 1) == VFS_EFBIG, "append melewati batas ditolak");
    vfs_stat_t st; vfs_stat("big", &st);
    ok(st.size == VFS_MAX_SIZE, "isi lama utuh setelah append gagal");
    vfs_delete("big", 0);

    char nm[8]; int made = 0;
    for (int i = 0; i < 100; i++) {
        nm[0] = 'f'; nm[1] = (char)('a' + i / 26); nm[2] = (char)('a' + i % 26); nm[3] = 0;
        if (vfs_create(nm, 0, 0644) == VFS_OK) made++;
    }
    ok(vfs_count() == VFS_MAX_FILES, "tabel terisi penuh (32)");
    ok(vfs_create("lagi", 0, 0644) == VFS_ENOSPC, "berkas ke-33 ditolak (ENOSPC)");
    (void)made;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        const vfs_file_t *f = vfs_entry(i);
        if (f && f->name[0] == 'f') { char tmp[VFS_NAME_MAX]; int k = 0; while ((tmp[k] = f->name[k])) k++; vfs_delete(tmp, 0); }
    }
    vfs_delete("readme.txt", 0); vfs_delete("rahasia.txt", 0);
    ok(vfs_count() == 0, "semua berkas terhapus");
    mem_stats_t m1 = mem_get_stats();
    ok(m1.used_bytes == m0.used_bytes - 0 || m1.used_bytes <= m0.used_bytes, "tidak ada kebocoran heap setelah semua dihapus");
    ok(m1.used_blocks == 0 && m1.free_blocks == 1, "heap utuh kembali (coalescing berfungsi)");
}

/* ============ uji 3: OOM saat menulis, isi lama harus aman ============ */
static void t_oom(void) {
    boot_state();
    char b[64];
    vfs_create("keep", 0, 0644);
    vfs_write("keep", 0, "isi-lama", 8, 0);
    void *hog[400]; int n = 0;
    while (n < 400) { void *p = kmalloc(2048); if (!p) break; hog[n++] = p; }
    /* sisakan celah sangat kecil, lalu coba tulis data yang tak muat */
    void *last = NULL;
    for (;;) { void *p = kmalloc(16); if (!p) break; last = p; }
    (void)last;
    int r = vfs_write("keep", 0, "baru-dan-lebih-panjang", 22, 0);
    ok(r == VFS_ENOMEM, "tulis saat heap habis -> ENOMEM (bukan crash)");
    ok(vfs_read("keep", 0, b, 63) == 8 && b[0] == 'i' && b[7] == 'a', "isi lama tetap utuh setelah OOM");
    clear_cap();
    fscmd_try("write keep teks-lain");
    ok(has("memori tidak cukup"), "perintah write melaporkan Out of Memory dengan jelas");
    for (int i = 0; i < n; i++) kfree(hog[i]);
    /* sisa kmalloc(16) tidak dilacak -> bebaskan lewat reset heap */
    mem_init(); vfs_init();
}

/* ============ uji 4: izin & autentikasi lewat API ============ */
static void t_perm(void) {
    boot_state();
    char b[64];
    ok(security_verify("root", "root123") && security_verify("budi", "budi123") && security_verify("ani", "ani123"), "ketiga akun bisa login");
    ok(!security_verify("budi", "salah") && !security_verify("budi", "") && !security_verify("hantu", "x"), "password salah / user tak ada ditolak");
    ok(!security_verify("budi", "budi1234") && !security_verify("budi", "Budi123"), "mirip tapi beda ditolak");
    ok(security_find_uid("budi") == 1000 && security_find_uid("x") == -1, "find_uid");
    ok(security_is_root(), "boot sebagai root");
    ok(security_login("budi", "salah") == 0 && security_uid() == 0, "login gagal tidak mengganti user");
    ok(security_login("budi", "budi123") == 1 && security_uid() == 1000, "login berhasil mengganti user");
    ok(security_switch_noauth("ani") == 0 && security_uid() == 1000, "non-root tidak bisa pindah tanpa password");
    security_login("root", "root123");
    ok(security_switch_noauth("ani") == 1 && security_uid() == 1001, "root pindah tanpa password");
    security_login("root", "root123");

    ok(vfs_read("rahasia.txt", 1000, b, 63) == VFS_EACCES, "budi tak boleh baca rahasia.txt (600)");
    ok(vfs_read("rahasia.txt", 0, b, 63) == 34, "root boleh baca rahasia.txt");
    ok(vfs_read("readme.txt", 1001, b, 63) == 29, "ani boleh baca readme.txt (644)");
    ok(vfs_write("readme.txt", 1001, "x", 1, 0) == VFS_EACCES, "ani tak boleh tulis readme.txt (644)");
    ok(vfs_delete("readme.txt", 1000) == VFS_EACCES, "budi tak boleh hapus berkas root");
    ok(vfs_chmod("readme.txt", 1000, 0666) == VFS_EACCES, "budi tak boleh chmod berkas root");
    ok(vfs_chown("readme.txt", 1000, 1000) == VFS_EACCES, "budi tak boleh chown");

    vfs_create("tulis.txt", 1000, 0666);
    ok(vfs_write("tulis.txt", 1001, "oleh ani", 8, 0) == 8, "mode 666: orang lain boleh menulis");
    vfs_chmod("tulis.txt", 1000, 0640);                       /* group digit tak dipakai: others = 0 */
    ok(vfs_read("tulis.txt", 1001, b, 63) == VFS_EACCES, "mode 640: others tak punya izin baca");
    vfs_chmod("tulis.txt", 1000, 0604);
    ok(vfs_read("tulis.txt", 1001, b, 63) == 8, "mode 604: others boleh baca");
    ok(vfs_write("tulis.txt", 1001, "x", 1, 0) == VFS_EACCES, "mode 604: others tak boleh tulis");
    vfs_chmod("tulis.txt", 1000, 0200);
    ok(vfs_read("tulis.txt", 1000, b, 63) == VFS_EACCES, "mode 200: pemilik sendiri tak boleh baca");
    ok(vfs_read("tulis.txt", 0, b, 63) == 8, "root mengabaikan izin");
    char m[10]; vfs_mode_str(0644, m);
    ok(!kstrcmp(m, "rw-r--r--"), "format izin rw-r--r--");
    vfs_mode_str(0600, m);
    ok(!kstrcmp(m, "rw-------"), "format izin rw-------");
}

/* ============ uji 5: alur perintah shell (tokenizer, su, root-only) ============ */
static void t_shell(void) {
    boot_state();
    ok(fscmd_try("help") == 0 && fscmd_try("bukanperintah") == 0 && fscmd_try("") == 0 && fscmd_try("   ") == 0, "bukan perintah fscmd -> return 0");
    ok(fscmd_try("lsx") == 0 && fscmd_try("catt a") == 0, "nama perintah harus persis");

    ok(run("ls") == 1 && has("readme.txt") && has("rahasia.txt") && has("rw-------") && has("(2 berkas"), "ls menampilkan 2 berkas awal");
    ok(run("touch catatan.txt") == 1, "touch");
    ok(run("write catatan.txt halo\\ndunia") == 1 && has("10 byte ditulis"), "write dengan \\n (10 byte)");
    ok(run("append catatan.txt    sayang  kamu") == 1 && has("12 byte ditulis"), "append: spasi di dalam teks dipertahankan");
    ok(run("cat catatan.txt") == 1 && has("halo\ndunia") && has("sayang  kamu"), "cat menampilkan isi (baris baru + spasi ganda)");
    ok(run("stat catatan.txt") == 1 && has("22 byte") && has("root (uid 0)") && has("rw-r--r--") && has("(644)"), "stat");
    ok(run("chmod 600 catatan.txt") == 1 && (run("stat catatan.txt"), has("rw-------") && has("(600)")), "chmod 600");
    ok(run("chmod 999 catatan.txt") == 1 && has("3 digit oktal"), "chmod dengan angka tidak valid ditolak");
    ok(run("chmod 64 catatan.txt") == 1 && has("3 digit oktal"), "chmod 2 digit ditolak");
    ok(run("chmod 644") == 1 && has("pemakaian"), "chmod tanpa nama -> bantuan");
    ok(run("write") == 1 && has("pemakaian"), "write tanpa argumen -> bantuan");
    ok(run("write a.txt") == 1 && has("pemakaian"), "write tanpa teks -> bantuan");
    ok(run("cat") == 1 && has("pemakaian"), "cat tanpa argumen -> bantuan");
    ok(run("cat tidakada") == 1 && has("tidak ditemukan"), "cat berkas tak ada");
    ok(run("touch catatan.txt") == 1 && has("sudah ada"), "touch duplikat");
    ok(run("touch bad/nama") == 1 && has("tidak valid"), "touch nama tak valid");
    ok(run("df") == 1 && has("RAMDisk : 3/32") && has("Heap"), "df");
    ok(run("users") == 1 && has("root") && has("budi") && has("ani") && has("<- aktif"), "users");
    ok(run("whoami") == 1 && has("root (uid 0)"), "whoami root");
    ok(fscmd_require_root("reboot") == 1, "root boleh reboot");

    /* root pindah user TANPA password; user biasa wajib password */
    ok(run("su budi") == 1 && security_uid() == 1000 && !has("Password"), "root su budi tanpa password");
    kb = "salah\n";
    ok(run("su ani") == 1 && has("Password untuk ani:") && has("*****") && !has("salah") && has("autentikasi gagal"), "su password salah (hanya * tampil)");
    ok(security_uid() == 1000, "tetap budi setelah su gagal");
    kb = "ani123\n";
    ok(run("su ani") == 1 && security_uid() == 1001, "budi su ani dengan password benar");
    kb = "budi123\n";
    ok(run("su budi") == 1 && security_uid() == 1000, "kembali ke budi");
    ok(run("whoami") == 1 && has("budi (uid 1000)"), "whoami budi");
    clear_cap(); fscmd_print_prompt();
    ok(has("budi@dalfon$"), "prompt user biasa memakai $");

    /* sebagai budi */
    ok(run("cat rahasia.txt") == 1 && has("Permission denied") && !has("hanya root"), "budi ditolak membaca rahasia.txt");
    ok(run("cat catatan.txt") == 1 && has("Permission denied"), "budi ditolak membaca catatan.txt (600)");
    ok(run("cat readme.txt") == 1 && has("Selamat datang"), "budi boleh membaca readme.txt");
    ok(run("write readme.txt retas") == 1 && has("Permission denied"), "budi ditolak menulis readme.txt");
    ok(run("rm readme.txt") == 1 && has("Permission denied"), "budi ditolak menghapus berkas root");
    ok(run("chmod 666 readme.txt") == 1 && has("Permission denied"), "budi ditolak chmod berkas root");
    ok(run("chown budi readme.txt") == 1 && has("Permission denied") && has("hanya root"), "budi ditolak chown");
    ok(fscmd_require_root("reboot") == 0 && has("Permission denied"), "budi ditolak reboot");
    ok((clear_cap(), fscmd_require_root("halt")) == 0 && has("halt: Permission denied"), "budi ditolak halt");
    ok(run("touch punya_budi.txt") == 1 && run("write punya_budi.txt data budi") == 1, "budi membuat berkas sendiri");
    ok(run("ls") == 1 && has("budi") && has("punya_budi.txt"), "ls menampilkan pemilik budi");
    ok(run("cat punya_budi.txt") == 1 && has("data budi"), "budi membaca berkas sendiri");
    ok(run("rm punya_budi.txt") == 1 && !has("Permission"), "budi menghapus berkas sendiri");
    ok(run("sectest") == 1 && has("HASIL: SEMUA LULUS") && !has("[FAIL]"), "sectest lulus (dijalankan dari budi)");
    ok(security_uid() == 1000, "sectest tidak mengganti user aktif");

    /* kembali ke root: butuh password; root tidak butuh password untuk su berikutnya */
    kb = "root123\n";
    ok(run("su root") == 1 && security_uid() == 0, "su root dengan password benar");
    ok(run("su ani") == 1 && security_uid() == 1001, "root su ani tanpa password");
    kb = "root123\n";
    ok(run("su") == 1 && has("Password untuk root") && security_uid() == 0, "ani: 'su' tanpa argumen meminta password root");
}

/* ============ uji 6: self-test bawaan OS (yang akan didemokan) ============ */
static void t_selftests(void) {
    boot_state();
    ok(run("fstest") == 1 && has("HASIL: SEMUA LULUS") && !has("[FAIL]"), "perintah fstest lulus");
    boot_state();
    ok(run("sectest") == 1 && has("HASIL: SEMUA LULUS") && !has("[FAIL]"), "perintah sectest lulus");
    boot_state();
    ok(vfs_count() == 2, "self-test tidak meninggalkan berkas sisa");
}

void _start(void) {
    out("== Uji VFS (Tahap 7) & Proteksi (Tahap 8) ==\n");
    t_vfs_api();   out("[1] API VFS ........ selesai\n");
    t_limits();    out("[2] batas & heap ... selesai\n");
    t_oom();       out("[3] OOM ............ selesai\n");
    t_perm();      out("[4] izin & login ... selesai\n");
    t_shell();     out("[5] perintah shell . selesai\n");
    t_selftests(); out("[6] fstest/sectest . selesai\n");
    out("\n");
    char r[48]; int i = 0;
    int p = total - failed;
    char tmp[12]; int t = 0;
    if (p == 0) tmp[t++] = '0';
    while (p) { tmp[t++] = (char)('0' + p % 10); p /= 10; }
    while (t) r[i++] = tmp[--t];
    r[i++] = '/';
    p = total; t = 0;
    while (p) { tmp[t++] = (char)('0' + p % 10); p /= 10; }
    while (t) r[i++] = tmp[--t];
    r[i++] = ' '; r[i] = 0;
    out(r); out("pemeriksaan lulus\n");
    die(failed ? 1 : 0);
}
