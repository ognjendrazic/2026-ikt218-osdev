; idt.asm - ISR stubs for CPU exceptions 0-31, IRQ stubs 0-15, and IDT loader

extern isr_handler
extern irq_handler

section .text
bits 32

; Load the IDT from the idt_ptr struct whose address is passed in [esp+4].
global idt_load
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret

; ISR_NO_ERR: for exceptions that do NOT push an error code.
; Push dummy 0 so the stack layout is uniform across all handlers.
%macro ISR_NO_ERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp common_isr_stub
%endmacro

; ISR_ERR: for exceptions that DO push an error code.
; CPU already pushed the error code; just push the vector number.
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp common_isr_stub
%endmacro

ISR_NO_ERR 0 ; #DE - Divide Error
ISR_NO_ERR 1 ; #DB - Debug
ISR_NO_ERR 2 ; NMI - Non-Maskable Interrupt
ISR_NO_ERR 3 ; #BP - Breakpoint
ISR_NO_ERR 4 ; #OF - Overflow
ISR_NO_ERR 5 ; #BR - BOUND Range Exceeded
ISR_NO_ERR 6 ; #UD - Invalid Opcode
ISR_NO_ERR 7 ; #NM - Device Not Available
ISR_ERR    8 ; #DF - Double Fault (error code always 0)
ISR_NO_ERR 9 ; Coprocessor Segment Overrun (obsolete)
ISR_ERR    10 ; #TS - Invalid TSS
ISR_ERR    11 ; #NP - Segment Not Present
ISR_ERR    12 ; #SS - Stack-Segment Fault
ISR_ERR    13 ; #GP - General Protection Fault
ISR_ERR    14 ; #PF - Page Fault
ISR_NO_ERR 15 ; Reserved
ISR_NO_ERR 16 ; #MF - x87 FPU Floating-Point Error
ISR_ERR    17 ; #AC - Alignment Check
ISR_NO_ERR 18 ; #MC - Machine Check
ISR_NO_ERR 19 ; #XM - SIMD Floating-Point Exception
ISR_NO_ERR 20 ; #VE - Virtualization Exception
ISR_NO_ERR 21 ; Reserved
ISR_NO_ERR 22 ; Reserved
ISR_NO_ERR 23 ; Reserved
ISR_NO_ERR 24 ; Reserved
ISR_NO_ERR 25 ; Reserved
ISR_NO_ERR 26 ; Reserved
ISR_NO_ERR 27 ; Reserved
ISR_NO_ERR 28 ; Reserved
ISR_NO_ERR 29 ; Reserved
ISR_NO_ERR 30 ; Reserved
ISR_NO_ERR 31 ; Reserved

; Common entry for all ISR stubs.
; Stack layout on entry (top = lowest address):
; [esp+ 0] int_no (pushed by stub)
; [esp+ 4] err_code (pushed by stub or CPU)
; [esp+ 8] eip (pushed by CPU)
; [esp+12] cs (pushed by CPU)
; [esp+16] eflags (pushed by CPU)
common_isr_stub:
    pusha ; Saves EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI

    ; Save DS and reload kernel data segment
    mov ax, ds
    push eax
    mov ax, 0x10 ; kernel data selector (GDT entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass pointer to the full register frame to the C handler.
    ; ESP currently points to the saved DS, that is the start of registers_t.
    push esp
    call isr_handler
    add esp, 4

    ; Restore segment registers
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8 ; Discard int_no and err_code
    iret

; IRQ macro: IRQs never push an error code, so always push dummy 0.
; First arg is the IRQ number (0-15), second is the IDT vector (0x20-0x2F).
%macro IRQ 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp common_irq_stub
%endmacro

IRQ 0,  0x20
IRQ 1,  0x21
IRQ 2,  0x22
IRQ 3,  0x23
IRQ 4,  0x24
IRQ 5,  0x25
IRQ 6,  0x26
IRQ 7,  0x27
IRQ 8,  0x28
IRQ 9,  0x29
IRQ 10, 0x2A
IRQ 11, 0x2B
IRQ 12, 0x2C
IRQ 13, 0x2D
IRQ 14, 0x2E
IRQ 15, 0x2F

; Common entry for all IRQ stubs. Identical save/restore sequence to
; common_isr_stub but calls irq_handler, which sends EOI before returning.
common_irq_stub:
    pusha

    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret
