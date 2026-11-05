/* =============================================================================
 * kernel/shell.c - Basic Interactive CLI Shell for Dalfon OS
 * Supports: help, clear, about
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

/* Trim leading whitespace */
static const char *skip_spaces(const char *str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    return str;
}

void shell_print_banner(void) {
    vga_print_colored("================================================================================\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_print_colored("  ____    _    _     _____ ___  _   _    ___  ____  \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print_colored(" |  _ \\  / \\  | |   |  ___/ _ \\| \\ | |  / _ \\/ ___| \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print_colored(" | | | |/ _ \\ | |   | |_ | | | |  \\| | | | | \\___ \\ \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print_colored(" | |_| / ___ \\| |___|  _|| |_| | |\\  | | |_| |___) |\n", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_print_colored(" |____/_/   \\_\\_____|_|   \\___/|_| \\_|  \\___/|____/   v0.1.0 (x86 32-bit)\n", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_print_colored("================================================================================\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_print_colored("Welcome to Dalfon OS! Type ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_print_colored("'help'", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print_colored(" to see available commands.\n\n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void cmd_help(void) {
    vga_print_colored("Available Commands (Dalfon OS):\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("  help   - Show this help message\n");
    vga_print("  clear  - Clear the terminal screen\n");
    vga_print("  about  - Display information about Dalfon OS\n");
}

static void cmd_about(void) {
    vga_print_colored("Dalfon OS Kernel v0.1.0\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("  OS Name      : Dalfon OS (Mini-OS Project)\n");
    vga_print("  Architecture : x86 32-bit (i686) Protected Mode\n");
    vga_print("  Boot Standard: GNU Multiboot v1 (0x1BADB002)\n");
    vga_print("  Display      : VGA Text Mode 80x25 @ 0xB8000\n");
    vga_print("  Input Driver : PS/2 Keyboard Polling (Port 0x60/0x64)\n");
    vga_print("  Purpose      : Operating Systems Course Project (Tubes SO)\n");
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
    } else {
        vga_print_colored("Unknown command: '", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print_colored(cmd, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_print_colored("'. Type 'help' for a list of commands.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
}

void shell_run(void) {
    char input_buffer[SHELL_BUFFER_SIZE];

    while (1) {
        vga_print_colored("dalfon", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_print_colored("> ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

        keyboard_readline(input_buffer, SHELL_BUFFER_SIZE);
        shell_execute(input_buffer);
    }
}
