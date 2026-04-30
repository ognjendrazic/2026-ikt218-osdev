#pragma once

#include <libc/stdint.h>

// Segment selector byte offsets into the GDT.
// Ring 0, GDT: RPL=0, TI=0, so selector == index * 8.
#define GDT_KERNEL_CODE 0x08 // Entry 1
#define GDT_KERNEL_DATA 0x10 // Entry 2

// One 8-byte GDT entry as described in Intel SDM Vol. 3A §3.4.5.
// The packed attribute prevents the compiler from inserting padding bytes
// between fields, which would corrupt the hardware-defined layout.
struct gdt_entry {
    uint16_t limit_low; // Segment limit bits 15:0
    uint16_t base_low; // Base address bits 15:0
    uint8_t base_mid; // Base address bits 23:16
    uint8_t access; // Access byte (present, DPL, type flags)
    uint8_t limit_flags; // Limit bits 19:16 (low nibble) | flags (high nibble)
    uint8_t base_high; // Base address bits 31:24
} __attribute__((packed));

// 6-byte structure loaded by the lgdt instruction.
// 'limit' is the byte size of the GDT minus 1; 'base' is its linear address.
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Initialises the GDT and reloads all segment registers.
void gdt_init(void);
