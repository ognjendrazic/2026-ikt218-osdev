#include <gdt.h>

// The GDT holds three descriptors: null, kernel code, and kernel data.
// Three entries × 8 bytes each = 24 bytes total.
static struct gdt_entry gdt[3];

// This structure is passed directly to the lgdt instruction (via gdt_flush).
// but we pass its address as a function argument instead.
static struct gdt_ptr gdt_descriptor;

// gdt_flush is implemented in gdt.asm.
// It calls lgdt and then reloads all segment registers.
extern void gdt_flush(uint32_t gdt_ptr_addr);

// Encodes one GDT entry into the hardware-mandated 8-byte format.
// base - 32-bit linear base address of the segment
// limit - 20-bit segment limit (unit is bytes if G=0, 4 KB pages if G=1)
// access - access byte: present, DPL, descriptor type, and type flags
// flags - 4-bit flags nibble: granularity (G), default size (D/B), long (L)
static void set_entry(int idx, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t flags) {
    gdt[idx].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt[idx].base_low = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_mid = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].access = access;
    // Upper nibble = flags, lower nibble = limit bits 19:16
    gdt[idx].limit_flags = (uint8_t)(((limit >> 16) & 0x0F) | (flags << 4));
    gdt[idx].base_high = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_init(void) {
    gdt_descriptor.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_descriptor.base  = (uint32_t)gdt;

    // Entry 0 - Null descriptor.
    // The CPU requires the first GDT entry to be all zeros.
    // Any segment load with selector 0 causes a #GP fault.
    set_entry(0, 0, 0, 0x00, 0x0);

    // Entry 1 - Kernel code segment.
    // Base=0, Limit=4 GB (0xFFFFF pages × 4 KB granularity).
    // Access 0x9A = 1001 1010b:
    // Bit 7 (P) = 1 present
    // Bits 6:5 (DPL) = 00  ring 0
    // Bit 4 (S) = 1 code/data descriptor
    // Bits 3:0 (Type) = 1010  code, readable, non-conforming
    // Flags 0xC = 1100b:
    // Bit 3 (G) = 1 4 KB page granularity
    // Bit 2 (D/B) = 1 32-bit protected mode
    set_entry(1, 0, 0xFFFFF, 0x9A, 0xC);

    // Entry 2 - Kernel data segment.
    // Same base/limit as code segment.
    // Access 0x92 = 1001 0010b:
    // Type = 0010  data, writable, grow-up
    set_entry(2, 0, 0xFFFFF, 0x92, 0xC);

    // Hand the GDT pointer to the assembly stub, which runs lgdt and then
    // performs a far jump to reload CS plus moves 0x10 into DS/ES/FS/GS/SS.
    gdt_flush((uint32_t)&gdt_descriptor);
}
