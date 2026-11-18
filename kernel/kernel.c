/* =============================================================================
 * kernel/kernel.c - Main Kernel Entry Point
 * ============================================================================= */

#include <stdint.h>
#include "vga.h"
#include "keyboard.h"
#include "thread.h"
#include "shell.h"
#include "vfs.h"
#include "security.h"
#include "idt.h"
#include "irq.h"
#include "memory.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t multiboot_info) {
    (void)multiboot_info; /* Unused in initial stage */

    /* Initialize VGA Text Mode terminal */
    vga_init();

    /* Verify that we were booted by a Multiboot-compliant bootloader */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_print_colored("[ERROR] Invalid Multiboot Magic Number!\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        return;
    }

    vga_print_colored("[OK] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("Multiboot v1 verified.\n");

    vga_print_colored("[OK] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("VGA 80x25 text mode initialized at 0xB8000.\n");

    /* Initialize PS/2 Keyboard polling driver */
    keyboard_init();
    vga_print_colored("[OK] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("PS/2 Keyboard driver initialized.\n");

    /* Initialize Kernel Thread Subsystem (TCB Table) */
    thread_init();
    vga_print_colored("[OK] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("Kernel Thread Manager (TCB) initialized.\n");

    /* Inisialisasi Alokator Memori Dinamis (heap kernel) */
    mem_init();
    vga_print_colored("[OK] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("Kernel heap allocator initialized (256 KB, kmalloc/kfree).\n");

    /* Tahap 8: akun pengguna (boot sebagai root) */
    security_init();
    vga_print_colored("[OK] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("Security module initialized (users: root, budi, ani).\n");

    /* Tahap 7: VFS RAMDisk (butuh heap) + dua berkas contoh */
    vfs_init();
    vfs_create("readme.txt", SEC_UID_ROOT, 0644);
    vfs_write("readme.txt", SEC_UID_ROOT, "Selamat datang di Dalfon OS!\n", 29, 0);
    vfs_create("rahasia.txt", SEC_UID_ROOT, 0600);
    vfs_write("rahasia.txt", SEC_UID_ROOT, "hanya root yang boleh membaca ini\n", 34, 0);
    vga_print_colored("[OK] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("VFS RAMDisk initialized (readme.txt, rahasia.txt).\n");

    /* GDT, IDT, PIC, PIT (timer 1000 Hz) lalu aktifkan interrupt */
    interrupts_init(TIMER_HZ);
    irq_enable();
    vga_print_colored("[OK] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("GDT/IDT/PIC loaded, PIT timer 1000 Hz running (IRQ0).\n\n");

    /* Display ASCII art banner and launch interactive CLI shell */
    shell_print_banner();
    shell_run();
}
