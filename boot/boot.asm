; =============================================================================
; boot/boot.asm - Multiboot v1 Header, Kernel Entry Point & Context Switch
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
global context_switch
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

; =============================================================================
; void context_switch(uint32_t **old_esp, uint32_t *new_esp)
; Menyimpan register callee-saved (EBP, EBX, ESI, EDI) ke stack thread lama,
; menyimpan ESP lama ke *old_esp, mengganti ESP ke new_esp, lalu memulihkan
; register dari stack thread baru dan melompat (ret) ke EIP thread baru.
; =============================================================================
context_switch:
    mov eax, [esp + 4]          ; eax = pointer ke old_esp (uint32_t **)
    mov edx, [esp + 8]          ; edx = nilai new_esp (uint32_t *)

    ; Simpan callee-saved registers milik thread yang sedang berjalan
    push ebp
    push ebx
    push esi
    push edi

    ; Simpan stack pointer (ESP) saat ini ke dalam TCB thread lama (*old_esp = esp)
    mov [eax], esp

    ; Pindahkan stack pointer (ESP) ke stack milik thread baru (esp = new_esp)
    mov esp, edx

    ; Pulihkan callee-saved registers milik thread baru
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; Lompat ke alamat instruksi (EIP) yang tersimpan di puncak stack thread baru
    ret
