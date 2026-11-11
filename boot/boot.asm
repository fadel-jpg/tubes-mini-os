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
global irq0_stub
global isr_stub_table
extern kernel_main
extern timer_irq_handler
extern exception_handler

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

; =============================================================================
; Interrupt stubs (Tahap 3)
; irq0_stub  : IRQ0 timer PIT -> timer_irq_handler() -> sched_tick() (preemption)
; isr0..isr31: CPU exception -> exception_handler(regs_t *)
; Semua register disimpan dengan pushad di stack thread yang sedang berjalan,
; sehingga context switch di dalam ISR otomatis menyimpan konteks lengkap thread.
; =============================================================================
irq0_stub:
    pushad
    call timer_irq_handler
    popad
    iretd

%macro ISR_NOERR 1
isr%1:
    push dword 0            ; error code palsu agar layout stack seragam
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    push dword %1           ; error code sudah di-push oleh CPU
    jmp isr_common
%endmacro

isr_common:
    pushad
    push esp                ; argumen: regs_t *
    call exception_handler
    add esp, 4
    popad
    add esp, 8              ; buang int_no + err_code
    iretd

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

section .rodata
isr_stub_table:
%assign i 0
%rep 32
    dd isr %+ i
%assign i i+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
