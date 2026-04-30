; multiboot2.asm - kernel entry point and Multiboot2 header
;
; The Limine bootloader scans the kernel ELF for a Multiboot2 header.
; It must appear within the first 32 KiB of the image and be 8-byte aligned.
; After verifying the header, the bootloader jumps to _start in 32-bit
; protected mode with:
; EAX = 0x36D76289  (Multiboot2 magic value)
; EBX = physical address of the Multiboot2 information structure

extern main ; Defined in kernel.c

global _start ; Exported so the linker script can place it first


; Multiboot2 header
; Must reside in its own section so the linker script can guarantee it lands
; at the very start of the binary (see linker.ld, .boot section).
section .multiboot_header

header_start:
    ; Magic number identifying this as a Multiboot2 header.
    dd 0xE85250D6

    ; Architecture field: 0 = 32-bit protected mode (i386).
    dd 0

    ; Header length in bytes (from header_start to header_end inclusive).
    dd header_end - header_start

    ; Checksum: the four header fields must sum to zero (mod 2^32).
    ; Computed as two's complement of (magic + arch + length).
    dd 0x100000000 - (0xE85250D6 + 0 + (header_end - header_start))

    ; Required end tag — signals the end of the tag list.
    ; Fields: type=0, flags=0, size=8.
align 8
    dw 0 ; type
    dw 0 ; flags
    dd 8 ; size
header_end:

; Kernel entry point
section .text
bits 32

_start:
    ; Disable hardware interrupts immediately.
    ; The IDT is not yet set up — any interrupt delivered here would cause a
    ; triple fault and reset the CPU.
    cli

    ; Set up the boot stack.
    ; The linker script reserves 64 KiB in .bss (stack_bottom … stack_top).
    ; The x86 stack grows downward, so ESP starts at the top of the region.
    mov esp, stack_top

    ; Pass Multiboot2 arguments to main(uint32_t magic, void *mbi).
    ; The cdecl calling convention pushes arguments right-to-left, so the
    ; last argument (mbi, in EBX) is pushed first.
    push ebx   ; arg2: pointer to Multiboot2 information structure
    push eax   ; arg1: Multiboot2 magic value (0x36D76289)

    ; Transfer control to the C kernel entry point.
    call main

    ; main should never return.  If it does, disable interrupts and halt.
    ; The jmp ensures the CPU does not execute stray bytes after hlt.
.hang:
    cli
    hlt
    jmp .hang

; Boot stack
section .bss
align 16

stack_bottom:
    resb 4096 * 16   ; 64 KiB boot stack
stack_top:
