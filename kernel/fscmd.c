/* =============================================================================
 * kernel/fscmd.c - Perintah CLI: berkas (Tahap 7) & pengguna/proteksi (Tahap 8)
 *
 * Berkas  : ls touch write append cat rm stat chmod chown df fstest
 * Pengguna: whoami su users sectest
 *
 * Shell lama mencocokkan seluruh baris, jadi tidak bisa membaca argumen. Di sini
 * baris dipecah menjadi kata-kata (tokenizer) lalu argv[0] dicocokkan ke perintah.
 * ============================================================================= */

#include "fscmd.h"
#include "vfs.h"
#include "security.h"
#include "memory.h"
#include "vga.h"
#include "keyboard.h"
#include "shell.h"

#define MAX_ARGS 8

/* ---------------------------------------------------------------------------
 * Helper tampilan
 * --------------------------------------------------------------------------- */
static void print_pad(const char *s, int width) {
    vga_print(s);
    for (int i = (int)kstrlen(s); i < width; i++) {
        vga_putchar(' ');
    }
}

static void print_uint_pad(uint32_t v, int width) {
    int digits = 1;
    for (uint32_t t = v; t >= 10; t /= 10) {
        digits++;
    }
    for (int i = digits; i < width; i++) {
        vga_putchar(' ');
    }
    vga_print_int(v);
}

static void print_oct3(uint16_t mode) {
    vga_putchar((char)('0' + ((mode >> 6) & 7)));
    vga_putchar((char)('0' + ((mode >> 3) & 7)));
    vga_putchar((char)('0' + (mode & 7)));
}

static void print_error(const char *cmd, int code) {
    vga_print_colored(cmd, VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print_colored(": ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print_colored(vfs_strerror(code), VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_putchar('\n');
}

static void print_usage(const char *text) {
    vga_print_colored("pemakaian: ", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print(text);
    vga_putchar('\n');
}

/* ---------------------------------------------------------------------------
 * Tokenizer
 * --------------------------------------------------------------------------- */
static int is_space(char c) {
    return c == ' ' || c == '\t';
}

/* Pecah line (diubah di tempat) menjadi argv[]; kembalikan argc */
static int tokenize(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;
    while (*p != '\0') {
        while (is_space(*p)) {
            *p++ = '\0';
        }
        if (*p == '\0') {
            break;
        }
        if (argc < max) {
            argv[argc++] = p;
        }
        while (*p != '\0' && !is_space(*p)) {
            p++;
        }
    }
    return argc;
}

/* Lewati n kata pertama beserta spasi sesudahnya (untuk mengambil sisa teks apa adanya) */
static const char *skip_tokens(const char *s, int n) {
    while (n-- > 0) {
        while (is_space(*s)) {
            s++;
        }
        while (*s != '\0' && !is_space(*s)) {
            s++;
        }
    }
    while (is_space(*s)) {
        s++;
    }
    return s;
}

/* Ubah "\n" menjadi baris baru dan "\\" menjadi satu backslash. Return panjang hasil. */
static uint32_t unescape(const char *src, char *dst, uint32_t cap) {
    uint32_t n = 0;
    while (*src != '\0' && n + 1 < cap) {
        if (src[0] == '\\' && src[1] == 'n') {
            dst[n++] = '\n';
            src += 2;
        } else if (src[0] == '\\' && src[1] == '\\') {
            dst[n++] = '\\';
            src += 2;
        } else {
            dst[n++] = *src++;
        }
    }
    dst[n] = '\0';
    return n;
}

/* ---------------------------------------------------------------------------
 * Hak akses perintah
 * --------------------------------------------------------------------------- */
int fscmd_require_root(const char *cmd) {
    if (security_is_root()) {
        return 1;
    }
    vga_print_colored(cmd, VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print_colored(": Permission denied - hanya root yang boleh (user aktif: ",
                      VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print_colored(security_username(security_uid()), VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print_colored(")\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    return 0;
}

void fscmd_print_prompt(void) {
    vga_print_colored(security_username(security_uid()), VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print_colored("@dalfon", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print_colored(security_is_root() ? "# " : "$ ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
}

/* ---------------------------------------------------------------------------
 * Perintah berkas (Tahap 7)
 * --------------------------------------------------------------------------- */
static void cmd_ls(void) {
    vga_print_colored("  IZIN        PEMILIK  UKURAN  NAMA\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        const vfs_file_t *f = vfs_entry(i);
        if (f == NULL) {
            continue;
        }
        char m[10];
        vfs_mode_str(f->mode, m);
        vga_print("  -");
        vga_print(m);
        vga_print("  ");
        print_pad(security_username(f->uid), 7);
        vga_print("  ");
        print_uint_pad(f->size, 6);
        vga_print("  ");
        vga_print(f->name);
        vga_putchar('\n');
    }
    vga_print("(");
    vga_print_int((uint32_t)vfs_count());
    vga_print(" berkas, ");
    vga_print_int(vfs_total_bytes());
    vga_print(" byte data)\n");
}

static void cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        print_usage("touch <nama>");
        return;
    }
    int r = vfs_create(argv[1], security_uid(), 0644);
    if (r < 0) {
        print_error("touch", r);
    }
}

static void cmd_write(const char *line, int argc, char **argv, int append) {
    const char *cmd = append ? "append" : "write";
    if (argc < 3) {
        print_usage(append ? "append <nama> <teks>   (\\n = baris baru)"
                           : "write <nama> <teks>    (\\n = baris baru)");
        return;
    }
    char text[SHELL_BUFFER_SIZE];
    uint32_t n = unescape(skip_tokens(line, 2), text, sizeof(text));
    int r = vfs_write(argv[1], security_uid(), text, n, append);
    if (r < 0) {
        print_error(cmd, r);
        return;
    }
    vga_print_int((uint32_t)r);
    vga_print(" byte ditulis ke ");
    vga_print(argv[1]);
    vga_putchar('\n');
}

static void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        print_usage("cat <nama>");
        return;
    }
    vfs_stat_t st;
    int r = vfs_stat(argv[1], &st);
    if (r < 0) {
        print_error("cat", r);
        return;
    }
    char *buf = (char *)kmalloc(st.size + 1);
    if (buf == NULL) {
        print_error("cat", VFS_ENOMEM);
        return;
    }
    r = vfs_read(argv[1], security_uid(), buf, st.size);
    if (r < 0) {
        kfree(buf);
        print_error("cat", r);
        return;
    }
    buf[r] = '\0';
    vga_print(buf);
    if (r == 0 || buf[r - 1] != '\n') {
        vga_putchar('\n');
    }
    kfree(buf);
}

static void cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        print_usage("rm <nama>");
        return;
    }
    int r = vfs_delete(argv[1], security_uid());
    if (r < 0) {
        print_error("rm", r);
    }
}

static void cmd_stat(int argc, char **argv) {
    if (argc < 2) {
        print_usage("stat <nama>");
        return;
    }
    vfs_stat_t st;
    int r = vfs_stat(argv[1], &st);
    if (r < 0) {
        print_error("stat", r);
        return;
    }
    char m[10];
    vfs_mode_str(st.mode, m);
    vga_print("  nama    : "); vga_print(argv[1]); vga_putchar('\n');
    vga_print("  ukuran  : "); vga_print_int(st.size); vga_print(" byte\n");
    vga_print("  pemilik : "); vga_print(security_username(st.uid));
    vga_print(" (uid "); vga_print_int(st.uid); vga_print(")\n");
    vga_print("  izin    : "); vga_print(m); vga_print(" ("); print_oct3(st.mode); vga_print(")\n");
}

static void cmd_chmod(int argc, char **argv) {
    if (argc < 3) {
        print_usage("chmod <3 digit oktal> <nama>   contoh: chmod 600 rahasia.txt");
        return;
    }
    const char *o = argv[1];
    if (kstrlen(o) != 3 || o[0] < '0' || o[0] > '7' || o[1] < '0' || o[1] > '7' || o[2] < '0' || o[2] > '7') {
        vga_print_colored("chmod: mode harus 3 digit oktal (mis. 644)\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        return;
    }
    uint16_t mode = (uint16_t)(((o[0] - '0') << 6) | ((o[1] - '0') << 3) | (o[2] - '0'));
    int r = vfs_chmod(argv[2], security_uid(), mode);
    if (r < 0) {
        print_error("chmod", r);
    }
}

static void cmd_chown(int argc, char **argv) {
    if (!fscmd_require_root("chown")) {
        return;
    }
    if (argc < 3) {
        print_usage("chown <user> <nama>");
        return;
    }
    int uid = security_find_uid(argv[1]);
    if (uid < 0) {
        vga_print_colored("chown: user tidak ada\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        return;
    }
    int r = vfs_chown(argv[2], security_uid(), (uint32_t)uid);
    if (r < 0) {
        print_error("chown", r);
    }
}

static void cmd_df(void) {
    mem_stats_t ms = mem_get_stats();
    vga_print("RAMDisk : ");
    vga_print_int((uint32_t)vfs_count());
    vga_print("/");
    vga_print_int(VFS_MAX_FILES);
    vga_print(" berkas, ");
    vga_print_int(vfs_total_bytes());
    vga_print(" byte data (maks ");
    vga_print_int(VFS_MAX_SIZE);
    vga_print(" byte/berkas)\n");
    vga_print("Heap    : ");
    vga_print_int(ms.used_bytes);
    vga_print(" terpakai, ");
    vga_print_int(ms.free_bytes);
    vga_print(" tersedia dari ");
    vga_print_int(ms.total_bytes);
    vga_print(" byte\n");
}

/* ---------------------------------------------------------------------------
 * Perintah pengguna (Tahap 8)
 * --------------------------------------------------------------------------- */
static void cmd_whoami(void) {
    vga_print(security_username(security_uid()));
    vga_print(" (uid ");
    vga_print_int(security_uid());
    vga_print(")\n");
}

/* Baca password tanpa menampilkannya; setiap huruf tampil sebagai '*' */
static void read_password(char *buf, size_t max) {
    size_t idx = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            vga_putchar('\n');
            buf[idx] = '\0';
            return;
        } else if (c == '\b') {
            if (idx > 0) {
                idx--;
                vga_putchar('\b');
            }
        } else if (c >= ' ' && c <= '~') {
            if (idx < max - 1) {
                buf[idx++] = c;
                vga_putchar('*');
            }
        }
    }
}

static void cmd_su(int argc, char **argv) {
    const char *target = (argc >= 2) ? argv[1] : "root";
    if (security_find_uid(target) < 0) {
        vga_print_colored("su: user tidak ada\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        return;
    }
    if (security_is_root()) {
        security_switch_noauth(target);          /* root tidak perlu password */
        return;
    }
    char pw[32];
    vga_print("Password untuk ");
    vga_print(target);
    vga_print(": ");
    read_password(pw, sizeof(pw));
    if (!security_login(target, pw)) {
        vga_print_colored("su: autentikasi gagal\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
}

static void cmd_users(void) {
    vga_print_colored("  UID   NAMA\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    for (int i = 0; i < security_user_count(); i++) {
        vga_print("  ");
        print_uint_pad(security_user_uid_at(i), 4);
        vga_print("  ");
        vga_print(security_user_name_at(i));
        if (security_user_uid_at(i) == security_uid()) {
            vga_print("   <- aktif");
        }
        vga_putchar('\n');
    }
}

/* ---------------------------------------------------------------------------
 * Uji otomatis (bukti pengujian untuk laporan & demo)
 * --------------------------------------------------------------------------- */
static int check(const char *what, int ok) {
    if (ok) {
        vga_print_colored("  [PASS] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        vga_print_colored("  [FAIL] ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
    vga_print(what);
    vga_putchar('\n');
    return ok ? 0 : 1;
}

static void print_verdict(int fails) {
    if (fails == 0) {
        vga_print_colored("HASIL: SEMUA LULUS\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        vga_print_colored("HASIL: ADA YANG GAGAL\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
}

static uint32_t cstr_len(const char *s) {
    return (uint32_t)kstrlen(s);
}

/* Baca seluruh isi berkas (<= 63 byte) sebagai root ke buf lalu akhiri dengan '\0' */
static int read_text(const char *name, uint32_t uid, char *buf) {
    int r = vfs_read(name, uid, buf, 63);
    if (r >= 0) {
        buf[r] = '\0';
    }
    return r;
}

static void cmd_fstest(void) {
    const char *f = "fstest.tmp";
    char buf[64];
    int fails = 0;

    vga_print_colored("Uji CRUD berkas teks di RAMDisk (sebagai root):\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vfs_delete(f, 0);
    mem_stats_t before = mem_get_stats();

    fails += check("CREATE: vfs_create berhasil", vfs_create(f, 0, 0644) == VFS_OK);
    fails += check("CREATE nama yang sama -> EEXIST", vfs_create(f, 0, 0644) == VFS_EEXIST);

    const char *t1 = "Halo Dalfon OS";
    fails += check("UPDATE: tulis awal", vfs_write(f, 0, t1, cstr_len(t1), 0) == (int)cstr_len(t1));
    fails += check("READ : isi sama dengan yang ditulis",
                   read_text(f, 0, buf) == (int)cstr_len(t1) && kstrcmp(buf, t1) == 0);

    const char *t2 = " - append";
    fails += check("UPDATE: append (tambah di akhir)", vfs_write(f, 0, t2, cstr_len(t2), 1) == (int)cstr_len(t2));
    fails += check("READ : isi = \"Halo Dalfon OS - append\"",
                   read_text(f, 0, buf) >= 0 && kstrcmp(buf, "Halo Dalfon OS - append") == 0);

    fails += check("UPDATE: timpa (overwrite) dengan \"baru\"", vfs_write(f, 0, "baru", 4, 0) == 4);
    fails += check("READ : isi = \"baru\"", read_text(f, 0, buf) == 4 && kstrcmp(buf, "baru") == 0);

    vfs_stat_t st;
    fails += check("STAT : ukuran 4 byte, pemilik root",
                   vfs_stat(f, &st) == VFS_OK && st.size == 4 && st.uid == 0);

    char *big = (char *)kmalloc(VFS_MAX_SIZE + 1);
    int big_ok = (big != NULL) && vfs_write(f, 0, big, VFS_MAX_SIZE + 1, 0) == VFS_EFBIG;
    kfree(big);
    fails += check("batas ukuran: tulis > 4096 byte ditolak (EFBIG)", big_ok);
    fails += check("isi lama tetap utuh setelah tulis gagal", read_text(f, 0, buf) == 4 && kstrcmp(buf, "baru") == 0);

    fails += check("DELETE: vfs_delete berhasil", vfs_delete(f, 0) == VFS_OK);
    fails += check("READ setelah delete -> ENOENT", vfs_read(f, 0, buf, 8) == VFS_ENOENT);
    fails += check("nama tidak valid ditolak (EINVAL)", vfs_create("a/b", 0, 0644) == VFS_EINVAL);

    mem_stats_t after = mem_get_stats();
    fails += check("tidak ada kebocoran heap (used sama seperti sebelum uji)", after.used_bytes == before.used_bytes);

    print_verdict(fails);
}

static void cmd_sectest(void) {
    const uint32_t ROOT = 0, BUDI = 1000, ANI = 1001;
    char buf[64];
    int fails = 0;

    vga_print_colored("Uji kontrol hak akses (root=0, budi=1000, ani=1001):\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vfs_delete("sec_root.tmp", ROOT);
    vfs_delete("sec_budi.tmp", ROOT);

    vfs_create("sec_root.tmp", ROOT, 0600);
    vfs_write("sec_root.tmp", ROOT, "rahasia", 7, 0);
    fails += check("root membaca berkas miliknya (600) -> DIIZINKAN", vfs_read("sec_root.tmp", ROOT, buf, 63) == 7);
    fails += check("budi membaca berkas root 600 -> DITOLAK", vfs_read("sec_root.tmp", BUDI, buf, 63) == VFS_EACCES);
    fails += check("budi menulis berkas root 600 -> DITOLAK", vfs_write("sec_root.tmp", BUDI, "x", 1, 0) == VFS_EACCES);
    fails += check("budi menghapus berkas root -> DITOLAK", vfs_delete("sec_root.tmp", BUDI) == VFS_EACCES);

    vfs_create("sec_budi.tmp", BUDI, 0644);
    fails += check("budi menulis berkas miliknya -> DIIZINKAN", vfs_write("sec_budi.tmp", BUDI, "data budi", 9, 0) == 9);
    fails += check("ani membaca berkas budi (644, others r) -> DIIZINKAN", vfs_read("sec_budi.tmp", ANI, buf, 63) == 9);
    fails += check("ani menulis berkas budi (others tanpa w) -> DITOLAK", vfs_write("sec_budi.tmp", ANI, "x", 1, 0) == VFS_EACCES);
    fails += check("ani chmod berkas budi -> DITOLAK", vfs_chmod("sec_budi.tmp", ANI, 0666) == VFS_EACCES);
    fails += check("budi chmod 600 berkas miliknya -> DIIZINKAN", vfs_chmod("sec_budi.tmp", BUDI, 0600) == VFS_OK);
    fails += check("ani membaca setelah chmod 600 -> DITOLAK", vfs_read("sec_budi.tmp", ANI, buf, 63) == VFS_EACCES);
    fails += check("root tetap boleh membaca (superuser) -> DIIZINKAN", vfs_read("sec_budi.tmp", ROOT, buf, 63) == 9);
    fails += check("budi chown (hanya root) -> DITOLAK", vfs_chown("sec_budi.tmp", BUDI, ANI) == VFS_EACCES);
    fails += check("root chown ke ani -> DIIZINKAN", vfs_chown("sec_budi.tmp", ROOT, ANI) == VFS_OK);
    fails += check("ani (pemilik baru) membaca berkas 600 -> DIIZINKAN", vfs_read("sec_budi.tmp", ANI, buf, 63) == 9);

    fails += check("login benar: budi/budi123 -> cocok", security_verify("budi", "budi123"));
    fails += check("login salah password -> ditolak", !security_verify("budi", "salah"));
    fails += check("login user tidak ada -> ditolak", !security_verify("hantu", "budi123"));

    fails += check("pemilik menghapus berkasnya -> DIIZINKAN", vfs_delete("sec_budi.tmp", ANI) == VFS_OK);
    vfs_delete("sec_root.tmp", ROOT);

    print_verdict(fails);
}

/* ---------------------------------------------------------------------------
 * Dispatcher
 * --------------------------------------------------------------------------- */
int fscmd_try(const char *line) {
    char work[SHELL_BUFFER_SIZE];
    char *argv[MAX_ARGS];

    size_t i = 0;
    while (line[i] != '\0' && i < sizeof(work) - 1) {
        work[i] = line[i];
        i++;
    }
    work[i] = '\0';

    int argc = tokenize(work, argv, MAX_ARGS);
    if (argc == 0) {
        return 0;
    }
    const char *c = argv[0];

    if      (kstrcmp(c, "ls")      == 0) { cmd_ls(); }
    else if (kstrcmp(c, "touch")   == 0) { cmd_touch(argc, argv); }
    else if (kstrcmp(c, "write")   == 0) { cmd_write(line, argc, argv, 0); }
    else if (kstrcmp(c, "append")  == 0) { cmd_write(line, argc, argv, 1); }
    else if (kstrcmp(c, "cat")     == 0) { cmd_cat(argc, argv); }
    else if (kstrcmp(c, "rm")      == 0) { cmd_rm(argc, argv); }
    else if (kstrcmp(c, "stat")    == 0) { cmd_stat(argc, argv); }
    else if (kstrcmp(c, "chmod")   == 0) { cmd_chmod(argc, argv); }
    else if (kstrcmp(c, "chown")   == 0) { cmd_chown(argc, argv); }
    else if (kstrcmp(c, "df")      == 0) { cmd_df(); }
    else if (kstrcmp(c, "fstest")  == 0) { cmd_fstest(); }
    else if (kstrcmp(c, "whoami")  == 0) { cmd_whoami(); }
    else if (kstrcmp(c, "su")      == 0) { cmd_su(argc, argv); }
    else if (kstrcmp(c, "users")   == 0) { cmd_users(); }
    else if (kstrcmp(c, "sectest") == 0) { cmd_sectest(); }
    else { return 0; }

    return 1;
}
