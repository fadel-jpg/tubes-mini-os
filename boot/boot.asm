; =============================================================================
; boot/boot.asm - Multiboot v1 Header & Kernel Entry Point (x86 32-bit)
; =============================================================================

MBALIGN  equ  1 << 0            ; Align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; Provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; Checksum of above, to prove we are multiboot

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                  ; 16 KB stack space
stack_top:

section .text
global _start
extern kernel_main

_start:
    ; Set up stack pointer
    mov esp, stack_top

    ; Reset EFLAGS
    push 0
    popf

    ; Push Multiboot info pointer (ebx) and magic number (eax)
    push ebx
    push eax

    ; Call C kernel entry point
    call kernel_main

    ; If kernel_main returns, disable interrupts and halt CPU forever
    cli
.hang:
    hlt
    jmp .hang

