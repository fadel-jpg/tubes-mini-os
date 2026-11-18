/* =============================================================================
 * kernel/security.c - Akun pengguna & autentikasi (Tahap 8)
 *
 * Password TIDAK disimpan sebagai teks: tabel hanya berisi hash djb2 (32-bit).
 * Saat login, password yang diketik di-hash lalu dibandingkan.
 * PERHATIAN: djb2 hanya untuk demo (cepat & mudah dijelaskan), BUKAN hash
 * kriptografis. Sistem sungguhan memakai hash lambat + salt (bcrypt/argon2).
 *
 * Akun demo (password ada di README / dokumen demo):
 *   root (uid 0)    budi (uid 1000)    ani (uid 1001)
 * ============================================================================= */

#include "security.h"

typedef struct {
    const char *name;
    uint32_t    uid;
    uint32_t    pass_hash;   /* djb2(password) */
} user_t;

static const user_t users[] = {
    { "root", 0,    0x56345c1fu },
    { "budi", 1000, 0x93c26e1fu },
    { "ani",  1001, 0xf2245c13u },
};
#define NUSERS ((int)(sizeof(users) / sizeof(users[0])))

static uint32_t current_uid = SEC_UID_ROOT;

/* djb2: h = h * 33 + c, mulai dari 5381 */
static uint32_t hash_djb2(const char *s) {
    uint32_t h = 5381;
    while (*s != '\0') {
        h = h * 33u + (uint8_t)*s;
        s++;
    }
    return h;
}

static int str_equal(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

void security_init(void) {
    current_uid = SEC_UID_ROOT;
}

uint32_t security_uid(void) {
    return current_uid;
}

int security_is_root(void) {
    return current_uid == SEC_UID_ROOT;
}

const char *security_username(uint32_t uid) {
    for (int i = 0; i < NUSERS; i++) {
        if (users[i].uid == uid) {
            return users[i].name;
        }
    }
    return "?";
}

int security_find_uid(const char *name) {
    for (int i = 0; i < NUSERS; i++) {
        if (str_equal(users[i].name, name)) {
            return (int)users[i].uid;
        }
    }
    return -1;
}

int security_verify(const char *name, const char *password) {
    for (int i = 0; i < NUSERS; i++) {
        if (str_equal(users[i].name, name)) {
            return users[i].pass_hash == hash_djb2(password);
        }
    }
    return 0;
}

int security_login(const char *name, const char *password) {
    if (!security_verify(name, password)) {
        return 0;
    }
    current_uid = (uint32_t)security_find_uid(name);
    return 1;
}

/* root boleh berpindah ke user mana pun tanpa password (seperti 'su' oleh root) */
int security_switch_noauth(const char *name) {
    if (!security_is_root()) {
        return 0;
    }
    int uid = security_find_uid(name);
    if (uid < 0) {
        return 0;
    }
    current_uid = (uint32_t)uid;
    return 1;
}

int security_user_count(void) {
    return NUSERS;
}

const char *security_user_name_at(int index) {
    return (index >= 0 && index < NUSERS) ? users[index].name : "?";
}

uint32_t security_user_uid_at(int index) {
    return (index >= 0 && index < NUSERS) ? users[index].uid : 0xFFFFFFFFu;
}
