#include <keyboard.h>
#include <idt.h>
#include <pic.h>
#include <io.h>
#include <terminal.h>

#define KEYBOARD_DATA_PORT 0x60

// US QWERTY scancode set 1, make codes only (0x00-0x7F).
// 0 means the key has no printable ASCII representation.
// '\b' and '\t' are omitted because terminal_putchar does not handle them.
static const char scancode_table[128] = {
    /* 0x00 */  0,    0,  '1', '2', '3', '4', '5', '6',
    /* 0x08 */ '7',  '8', '9', '0', '-', '=',   0,   0,
    /* 0x10 */ 'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',
    /* 0x18 */ 'o',  'p', '[', ']','\n',  0,  'a', 's',
    /* 0x20 */ 'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',
    /* 0x28 */'\'',  '`',  0, '\\','z', 'x', 'c', 'v',
    /* 0x30 */ 'b',  'n', 'm', ',', '.', '/',  0,  '*',
    /* 0x38 */  0,   ' ',  0,   0,   0,   0,   0,   0,
    /* 0x40 */  0,    0,   0,   0,   0,   0,   0,  '7',
    /* 0x48 */ '8',  '9', '-', '4', '5', '6', '+', '1',
    /* 0x50 */ '2',  '3', '0', '.',  0,   0,   0,   0,
    /* 0x58 */  0,    0,   0,   0,   0,   0,   0,   0,
    /* 0x60 */  0,    0,   0,   0,   0,   0,   0,   0,
    /* 0x68 */  0,    0,   0,   0,   0,   0,   0,   0,
    /* 0x70 */  0,    0,   0,   0,   0,   0,   0,   0,
    /* 0x78 */  0,    0,   0,   0,   0,   0,   0,   0,
};

// Ring buffer for raw scancodes written by the IRQ handler.
// Size is 256 so uint8_t head/tail indices wrap naturally without modulo.
#define BUF_SIZE 256
static uint8_t scancode_buf[BUF_SIZE];
static volatile uint8_t buf_head = 0;  // written by IRQ handler
static volatile uint8_t buf_tail = 0;  // read by keyboard_poll

// IRQ1 handler: runs in interrupt context.
// Stores the raw scancode in the ring buffer and returns immediately.
// Translation happens in keyboard_poll, outside interrupt context.
static void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Bit 7 set means key release — only buffer key presses
    if (scancode & 0x80) {
        return;
    }

    uint8_t next = buf_head + 1; // wraps at 256 via uint8_t overflow
    if (next != buf_tail) { // drop scancode silently if buffer full
        scancode_buf[buf_head] = scancode;
        buf_head = next;
    }
}

void keyboard_poll(void) {
    while (buf_tail != buf_head) {
        uint8_t scancode = scancode_buf[buf_tail];
        buf_tail++; // wraps at 256 via uint8_t overflow

        char c = scancode_table[scancode];
        if (c) {
            terminal_putchar(c);
        }
    }
}

char keyboard_getchar(void) {
    if (buf_tail == buf_head)
        return 0;
    uint8_t scancode = scancode_buf[buf_tail++];
    return scancode_table[scancode];
}

void keyboard_init(void) {
    irq_install_handler(1, keyboard_handler);
    pic_unmask_irq(1);
}
