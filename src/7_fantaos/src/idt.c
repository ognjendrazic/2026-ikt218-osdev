#include <idt.h>
#include <pic.h>
#include <terminal.h>

#define IDT_ENTRIES 256

// 0x8E = 1000 1110b: present, ring 0, 32-bit interrupt gate
#define IDT_GATE_FLAGS 0x8E

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_descriptor;

extern void idt_load(uint32_t);

extern void isr0(void); extern void isr1(void);  extern void isr2(void);
extern void isr3(void); extern void isr4(void);  extern void isr5(void);
extern void isr6(void); extern void isr7(void);  extern void isr8(void);
extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

extern void irq0(void); extern void irq1(void);  extern void irq2(void);
extern void irq3(void); extern void irq4(void);  extern void irq5(void);
extern void irq6(void); extern void irq7(void);  extern void irq8(void);
extern void irq9(void); extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static void (*isr_stubs[32])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static void (*irq_stubs[16])(void) = {
    irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
    irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15,
};

static void (*irq_handlers[16])(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = (uint16_t)(base & 0xFFFF);
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (uint16_t)((base >> 16) & 0xFFFF);
}

static const char *exception_names[32] = {
    "Division Error", // 0  #DE
    "Debug", // 1  #DB
    "Non-Maskable Interrupt", // 2
    "Breakpoint", // 3  #BP
    "Overflow", // 4  #OF
    "BOUND Range Exceeded", // 5  #BR
    "Invalid Opcode", // 6  #UD
    "Device Not Available", // 7  #NM
    "Double Fault", // 8  #DF
    "Coprocessor Segment Overrun", // 9
    "Invalid TSS", // 10 #TS
    "Segment Not Present", // 11 #NP
    "Stack-Segment Fault", // 12 #SS
    "General Protection Fault", // 13 #GP
    "Page Fault", // 14 #PF
    "Reserved", // 15
    "x87 FPU Error", // 16 #MF
    "Alignment Check", // 17 #AC
    "Machine Check", // 18 #MC
    "SIMD FP Exception", // 19 #XM
    "Virtualization Exception", // 20 #VE
    "Reserved", // 21
    "Reserved", // 22
    "Reserved", // 23
    "Reserved", // 24
    "Reserved", // 25
    "Reserved", // 26
    "Reserved", // 27
    "Reserved", // 28
    "Reserved", // 29
    "Reserved", // 30
    "Reserved", // 31
};

void irq_install_handler(uint8_t irq, void (*handler)(void)) {
    irq_handlers[irq] = handler;
}

void irq_handler(registers_t *r) {
    uint8_t irq = (uint8_t)(r->int_no - 0x20);
    if (irq_handlers[irq]) {
        irq_handlers[irq]();
    }
    pic_send_eoi(irq);
}

void isr_handler(registers_t *r) {
    printf("Exception %u: %s", r->int_no, exception_names[r->int_no]);
    if (r->err_code) {
        printf(" (error: 0x%x)", r->err_code);
    }
    printf("\n");

    // Unrecoverable exceptions: halt rather than return to broken state
    if (r->int_no == 8 || r->int_no == 13 || r->int_no == 14) {
        printf("KERNEL HALTED\n");
        for (;;) { asm volatile("hlt"); }
    }
}

void idt_init(void) {
    idt_descriptor.limit = (uint16_t)(sizeof(idt) - 1);
    idt_descriptor.base  = (uint32_t)idt;

    for (int i = 0; i < 32; i++) {
        idt_set_gate((uint8_t)i, (uint32_t)isr_stubs[i], 0x08, IDT_GATE_FLAGS);
    }

    for (int i = 0; i < 16; i++) {
        idt_set_gate((uint8_t)(0x20 + i), (uint32_t)irq_stubs[i], 0x08, IDT_GATE_FLAGS);
    }

    idt_load((uint32_t)&idt_descriptor);
}
