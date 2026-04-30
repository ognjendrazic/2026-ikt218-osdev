#pragma once

#include <libc/stdint.h>

// One 8-byte IDT gate descriptor.
// packed prevents padding that would corrupt the hardware-expected layout.
struct idt_entry {
    uint16_t offset_low; // Handler address bits 15:0
    uint16_t selector; // Code segment selector
    uint8_t zero; // Always 0
    uint8_t type_attr; // Gate type and attributes (P, DPL, type)
    uint16_t offset_high; // Handler address bits 31:16
} __attribute__((packed));

// 6-byte structure loaded by the lidt instruction.
struct idt_ptr {
    uint16_t limit; // Size of IDT in bytes minus 1
    uint32_t base; // Linear address of the IDT
} __attribute__((packed));

// Register state saved by the ISR stub before calling isr_handler.
// Layout must exactly match the push sequence in idt.asm.
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // saved by pusha
    uint32_t int_no, err_code; // pushed by stub
    uint32_t eip, cs, eflags; // pushed by CPU
} registers_t;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);

// Register a C handler for the given IRQ line (0-15).
// The handler is called from irq_handler before EOI is sent.
void irq_install_handler(uint8_t irq, void (*handler)(void));
