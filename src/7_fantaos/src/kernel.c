#include <libc/stdint.h>
#include <gdt.h>
#include <terminal.h>

// Kernel entry point.
// Called from _start in multiboot2.asm with:
// magic - should equal 0x36D76289 (Multiboot2 magic)
// mbi - physical address of the Multiboot2 information structure
void main(uint32_t magic, void *mbi) {
    (void)magic;
    (void)mbi;

    // Install the Global Descriptor Table.
    // This replaces the bootloader's temporary GDT and sets up the three
    // descriptors (null, kernel code, kernel data) that all subsequent
    // memory accesses rely on.
    gdt_init();

    // Initialise the VGA text-mode terminal and clear the screen.
    terminal_init();

    printf("Hello World\n");
}
