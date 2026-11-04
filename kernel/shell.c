/* =============================================================================
 * kernel/shell.c - Basic Interactive CLI Shell for Mini-OS
 * Supports: help, clear, about, echo, color, reboot, halt
 * ============================================================================= */

#include "shell.h"
#include "vga.h"
#include "keyboard.h"

/* Freestanding string helper functions */
size_t kstrlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int kstrcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int kstrncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/* Trim leading whitespace */
static const char *skip_spaces(const char *str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    return str;
}

void shell_print_banner(void) {
    vga_print_colored("================================================================================\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_print_colored("  __  __ _       _         ___  ____  \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print_colored(" |  \\/  (_)_ __ (_)       / _ \\/ ___| \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print_colored(" | |\\/| | | '_ \\| |_____ | | | \\___ \\ \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print_colored(" | |  | | | | | | |_____|| |_| |___) |\n", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_print_colored(" |_|  |_|_|_| |_|_|       \\___/|____/   v0.1.0 (x86 32-bit Bare-Metal)\n", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_print_colored("================================================================================\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_print_colored("Welcome to Mini-OS! Type ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_print_colored("'help'", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print_colored(" to see available commands.\n\n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void cmd_help(void) {
    vga_print_colored("Available Commands:\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("  help         - Show this help message\n");
    vga_print("  clear        - Clear the terminal screen\n");
    vga_print("  about        - Display information about Mini-OS\n");
    vga_print("  banner       - Reprint the Mini-OS ASCII logo\n");
    vga_print("  echo <text>  - Print text back to the terminal\n");
    vga_print("  reboot       - Reboot the system via PS/2 controller\n");
    vga_print("  halt         - Halt the CPU\n");
}

static void cmd_about(void) {
    vga_print_colored("Mini-OS Kernel v0.1.0\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("  Architecture : x86 32-bit (i686) Protected Mode\n");
    vga_print("  Boot Standard: GNU Multiboot v1 (0x1BADB002)\n");
    vga_print("  Display      : VGA Text Mode 80x25 @ 0xB8000\n");
    vga_print("  Input Driver : PS/2 Keyboard Polling (Port 0x60/0x64)\n");
    vga_print("  Purpose      : Operating Systems Course Project (Tubes SO)\n");
}

static void cmd_reboot(void) {
    vga_print_colored("Rebooting system...\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    /* Pulse CPU reset line via 8042 PS/2 keyboard controller */
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
    __asm__ volatile ("hlt");
}

static void cmd_halt(void) {
    vga_print_colored("System halted. You can close QEMU now.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    __asm__ volatile ("cli; hlt");
}

void shell_execute(const char *input) {
    const char *cmd = skip_spaces(input);

    /* Ignore empty input */
    if (*cmd == '\0') {
        return;
    }

    if (kstrcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (kstrcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (kstrcmp(cmd, "about") == 0) {
        cmd_about();
    } else if (kstrcmp(cmd, "banner") == 0) {
        shell_print_banner();
    } else if (kstrcmp(cmd, "reboot") == 0) {
        cmd_reboot();
    } else if (kstrcmp(cmd, "halt") == 0) {
        cmd_halt();
    } else if (kstrncmp(cmd, "echo ", 5) == 0) {
        vga_print(cmd + 5);
        vga_putchar('\n');
    } else if (kstrcmp(cmd, "echo") == 0) {
        vga_putchar('\n');
    } else {
        vga_print_colored("Unknown command: '", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print_colored(cmd, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_print_colored("'. Type 'help' for a list of commands.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
}

void shell_run(void) {
    char input_buffer[SHELL_BUFFER_SIZE];

    while (1) {
        vga_print_colored("mini-os", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_print_colored("> ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

        keyboard_readline(input_buffer, SHELL_BUFFER_SIZE);
        shell_execute(input_buffer);
    }
}

