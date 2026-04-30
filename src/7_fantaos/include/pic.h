#pragma once

#include <libc/stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20 // End-of-interrupt command

// Remap PIC1 to vectors 0x20-0x27 and PIC2 to 0x28-0x2F,
// then mask all IRQs. Call before sti.
void pic_init(void);

// Signal end-of-interrupt for the given IRQ (0-15).
// Must be called at the end of every IRQ handler.
void pic_send_eoi(uint8_t irq);

// Clear the mask bit for one IRQ line, enabling it to fire.
void pic_unmask_irq(uint8_t irq);
