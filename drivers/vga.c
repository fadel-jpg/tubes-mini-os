/* =============================================================================
 * drivers/vga.c - VGA Text Mode (80x25, 0xB8000) Driver
 * ============================================================================= */

#include "vga.h"

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static volatile uint16_t *terminal_buffer;

/* Scroll the screen up by one line when reaching the bottom */
static void vga_scroll(void) {
    if (terminal_row < VGA_HEIGHT) {
        return;
    }

    /* Move every line up by one row */
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    /* Clear the last row */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }

    terminal_row = VGA_HEIGHT - 1;
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

    /* Enable hardware cursor (scanlines 14 to 15) */
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);

    vga_clear();
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
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
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else if (c == '\r') {
        terminal_column = 0;
    } else if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] =
                vga_entry(' ', terminal_color);
        } else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
            terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] =
                vga_entry(' ', terminal_color);
        }
    } else if (c == '\t') {
        size_t spaces = 4 - (terminal_column % 4);
        for (size_t i = 0; i < spaces; i++) {
            vga_putchar(' ');
        }
        return;
    } else {
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = vga_entry((unsigned char)c, terminal_color);
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

