/* =============================================================================
 * drivers/vga.c - VGA Text Mode (80x25, 0xB8000) Driver with Scrollback Buffer
 * ============================================================================= */

#include "vga.h"
#include "irq.h"

#define VGA_SCROLLBACK_LINES 200

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static volatile uint16_t *terminal_buffer;

/* Buffer layar aktif (live) dan ring buffer riwayat baris yang ter-scroll ke atas */
static uint16_t live_buffer[VGA_HEIGHT][VGA_WIDTH];
static uint16_t history_buffer[VGA_SCROLLBACK_LINES][VGA_WIDTH];
static size_t   history_write = 0;
static size_t   history_count = 0;
static int      scroll_offset = 0; /* 0 = tampilan live, >0 = mundur N baris ke atas */

static void vga_render_view(void) {
    if (scroll_offset == 0) {
        for (size_t y = 0; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[y * VGA_WIDTH + x] = live_buffer[y][x];
            }
        }
        vga_update_cursor(terminal_column, terminal_row);
        return;
    }

    /* Total baris tersedia = history_count + VGA_HEIGHT.
     * Baris tampilan [0..VGA_HEIGHT-1] dimulai dari (history_count - scroll_offset). */
    int start_line = (int)history_count - scroll_offset;
    for (int y = 0; y < VGA_HEIGHT; y++) {
        int line_idx = start_line + y;
        if (line_idx < (int)history_count) {
            /* Ambil dari ring buffer history */
            size_t oldest = (history_count < VGA_SCROLLBACK_LINES) ? 0 : history_write;
            size_t ring_idx = (oldest + (size_t)line_idx) % VGA_SCROLLBACK_LINES;
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[y * VGA_WIDTH + x] = history_buffer[ring_idx][x];
            }
        } else {
            /* Ambil dari live_buffer */
            size_t live_y = (size_t)(line_idx - (int)history_count);
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[y * VGA_WIDTH + x] = live_buffer[live_y][x];
            }
        }
    }

    /* Sembunyikan kursor hardware sementara saat sedang melihat riwayat ke atas */
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)((VGA_WIDTH * VGA_HEIGHT) & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)(((VGA_WIDTH * VGA_HEIGHT) >> 8) & 0xFF));
}

void vga_scroll_reset(void) {
    uint32_t f = irq_save();
    if (scroll_offset != 0) {
        scroll_offset = 0;
        vga_render_view();
    }
    irq_restore(f);
}

void vga_scroll_view(int delta) {
    uint32_t f = irq_save();
    int new_offset = scroll_offset + delta;
    if (new_offset < 0) {
        new_offset = 0;
    }
    if (new_offset > (int)history_count) {
        new_offset = (int)history_count;
    }
    if (new_offset != scroll_offset) {
        scroll_offset = new_offset;
        vga_render_view();
    }
    irq_restore(f);
}

/* Scroll the screen up by one line when reaching the bottom */
static void vga_scroll(void) {
    if (terminal_row < VGA_HEIGHT) {
        return;
    }

    /* Simpan baris paling atas ke history_buffer sebelum tergeser keluar layar */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        history_buffer[history_write][x] = live_buffer[0][x];
    }
    history_write = (history_write + 1) % VGA_SCROLLBACK_LINES;
    if (history_count < VGA_SCROLLBACK_LINES) {
        history_count++;
    }

    /* Geser seluruh baris live_buffer naik 1 baris */
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            live_buffer[y][x] = live_buffer[y + 1][x];
        }
    }

    /* Kosongkan baris terakhir */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        live_buffer[VGA_HEIGHT - 1][x] = vga_entry(' ', terminal_color);
    }

    terminal_row = VGA_HEIGHT - 1;

    /* Salin live_buffer ke VRAM 0xB8000 */
    if (scroll_offset == 0) {
        for (size_t y = 0; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_buffer[y * VGA_WIDTH + x] = live_buffer[y][x];
            }
        }
    }
}

void vga_update_cursor(size_t x, size_t y) {
    uint16_t pos = (uint16_t)(y * VGA_WIDTH + x);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (volatile uint16_t *)VGA_MEMORY;
    history_write = 0;
    history_count = 0;
    scroll_offset = 0;

    /* Enable hardware cursor (scanlines 14 to 15) */
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);

    vga_clear();
}

void vga_clear(void) {
    scroll_offset = 0;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            uint16_t blank = vga_entry(' ', terminal_color);
            live_buffer[y][x] = blank;
            terminal_buffer[y * VGA_WIDTH + x] = blank;
        }
    }
    terminal_row = 0;
    terminal_column = 0;
    vga_update_cursor(terminal_column, terminal_row);
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    terminal_color = vga_entry_color(fg, bg);
}

void vga_putchar(char c) {
    if (scroll_offset != 0) {
        scroll_offset = 0;
        vga_render_view();
    }

    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else if (c == '\r') {
        terminal_column = 0;
    } else if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            uint16_t blank = vga_entry(' ', terminal_color);
            live_buffer[terminal_row][terminal_column] = blank;
            terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = blank;
        } else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
            uint16_t blank = vga_entry(' ', terminal_color);
            live_buffer[terminal_row][terminal_column] = blank;
            terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = blank;
        }
    } else if (c == '\t') {
        size_t spaces = 4 - (terminal_column % 4);
        for (size_t i = 0; i < spaces; i++) {
            vga_putchar(' ');
        }
        return;
    } else {
        uint16_t entry = vga_entry((unsigned char)c, terminal_color);
        live_buffer[terminal_row][terminal_column] = entry;
        terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = entry;
        terminal_column++;
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
    }

    vga_scroll();
    vga_update_cursor(terminal_column, terminal_row);
}

void vga_print(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

void vga_print_colored(const char *str, vga_color_t fg, vga_color_t bg) {
    uint8_t old_color = terminal_color;
    vga_set_color(fg, bg);
    vga_print(str);
    terminal_color = old_color;
}

void vga_print_int(uint32_t num) {
    if (num == 0) {
        vga_putchar('0');
        return;
    }

    char buf[12];
    int i = 0;
    while (num > 0) {
        buf[i++] = (char)('0' + (num % 10));
        num /= 10;
    }

    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}
