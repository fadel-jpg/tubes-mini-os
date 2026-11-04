/* =============================================================================
 * drivers/keyboard.c - PS/2 Keyboard Driver (Polling Mode, Scancode Set 1)
 * ============================================================================= */

#include "keyboard.h"
#include "vga.h"

static int shift_pressed = 0;
static int caps_lock = 0;

/* US QWERTY Scancode Set 1 - Normal (Unshifted) */
static const char scancode_normal[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* Left Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, /* Left Shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, /* Right Shift */
    '*',
    0, /* Alt */
    ' ', /* Space */
    0, /* Caps Lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1 - F10 */
    0, /* Num Lock */
    0, /* Scroll Lock */
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* US QWERTY Scancode Set 1 - Shifted */
static const char scancode_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, /* Left Ctrl */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, /* Left Shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0, /* Right Shift */
    '*',
    0, /* Alt */
    ' ', /* Space */
    0, /* Caps Lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1 - F10 */
    0, /* Num Lock */
    0, /* Scroll Lock */
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void keyboard_init(void) {
    shift_pressed = 0;
    caps_lock = 0;

    /* Flush any pending bytes in the PS/2 controller output buffer */
    while (inb(KB_STATUS_PORT) & 0x01) {
        (void)inb(KB_DATA_PORT);
    }
}

char keyboard_getchar(void) {
    while (1) {
        /* Wait until PS/2 output buffer is full (bit 0 == 1) */
        if ((inb(KB_STATUS_PORT) & 0x01) == 0) {
            continue;
        }

        uint8_t scancode = inb(KB_DATA_PORT);

        /* Key release event (top bit set) */
        if (scancode & 0x80) {
            uint8_t released = scancode & 0x7F;
            if (released == 0x2A || released == 0x36) { /* Left or Right Shift */
                shift_pressed = 0;
            }
            continue;
        }

        /* Key press event */
        if (scancode == 0x2A || scancode == 0x36) { /* Left or Right Shift */
            shift_pressed = 1;
            continue;
        }

        if (scancode == 0x3A) { /* Caps Lock toggle */
            caps_lock = !caps_lock;
            continue;
        }

        if (scancode < 128) {
            char c = shift_pressed ? scancode_shift[scancode] : scancode_normal[scancode];

            /* Apply Caps Lock to alphabetic characters */
            if (caps_lock) {
                if (c >= 'a' && c <= 'z') {
                    c = c - 'a' + 'A';
                } else if (c >= 'A' && c <= 'Z') {
                    c = c - 'A' + 'a';
                }
            }

            if (c != 0) {
                return c;
            }
        }
    }
}

void keyboard_readline(char *buffer, size_t max_len) {
    size_t idx = 0;

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            vga_putchar('\n');
            buffer[idx] = '\0';
            return;
        } else if (c == '\b') {
            if (idx > 0) {
                idx--;
                vga_putchar('\b');
            }
        } else if (c >= ' ' && c <= '~') {
            if (idx < max_len - 1) {
                buffer[idx++] = c;
                vga_putchar(c);
            }
        }
    }
}

