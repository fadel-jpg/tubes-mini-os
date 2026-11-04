/* =============================================================================
 * linker.ld - Linker Script for x86 32-bit Multiboot Kernel
 * Places the kernel at 1MB (0x00100000) physical address
 * ============================================================================= */

ENTRY(_start)

SECTIONS
{
    /* Load kernel at 1 MB physical memory */
    . = 1M;

    /* Multiboot header must come first (within first 8KB of binary) */
    .text BLOCK(4K) : ALIGN(4K)
    {
        KEEP(*(.multiboot))
        *(.text*)
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    /* Read-write initialized data */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data*)
    }

    /* Read-write uninitialized data and stack */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }
}

