; gdt.asm - loads the GDT and reloads all segment registers
;
; Called from C (cdecl convention) as:
;   extern void gdt_flush(uint32_t gdt_ptr_addr);
;
; In the cdecl calling convention the first argument is pushed onto the stack
; by the caller before the call instruction.  After 'call gdt_flush' the
; stack frame looks like:
;
; [esp+0] return address (pushed by call)
; [esp+4] gdt_ptr_addr (first argument - address of our gdt_ptr struct)

global gdt_flush

section .text
bits 32

gdt_flush:
    ; Retrieve the address of the gdt_ptr struct from the stack.
    ; EAX will hold a pointer to the 6-byte { limit, base } structure that
    ; lgdt expects to read from memory.
    mov eax, [esp+4]

    ; Load the GDT register (GDTR) from the gdt_ptr structure.
    ; After this instruction the CPU knows where the GDT lives and how large
    ; it is, but the segment registers (CS, DS, …) still cache old values.
    lgdt [eax]

    ; Reload CS (code segment register) via a far jump.
    ; CS cannot be changed with a simple mov, it must be updated through a
    ; control-transfer instruction that encodes the new selector.
    ; The selector 0x08 is entry 1 in the GDT (1 × 8 bytes = 0x08),
    ; with RPL=0 (ring 0) and TI=0 (use GDT).
    ; The far jump flushes the instruction pipeline and forces the CPU to
    ; fetch CS's descriptor from our newly installed GDT.
    jmp 0x08:.reload_cs

.reload_cs:
    ; Reload all data segment registers with the kernel data selector.
    ; 0x10 is entry 2 in the GDT (2 × 8 bytes = 0x10), RPL=0, TI=0.
    ; DS, ES, FS, GS, and SS must all reference a valid, present descriptor
    ; or any memory access will raise a #GP fault.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Return to the C caller. The stack is unchanged (cdecl: caller cleans up).
    ret
