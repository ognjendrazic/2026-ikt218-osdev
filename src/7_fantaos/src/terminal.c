#include <terminal.h>
#include <libc/stdint.h>
#include <libc/stdarg.h>

// VGA text mode: 80 columns × 25 rows.
// The hardware maps its framebuffer to physical address 0xB8000.
// Each cell is 2 bytes: low byte = ASCII character, high byte = colour attribute.
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_BUFFER ((volatile uint16_t *)0xB8000)

// Colour attribute: light grey (7) text on black (0) background.
// Format: bits 7:4 = background colour, bits 3:0 = foreground colour.
#define VGA_COLOR 0x07

static int cursor_row;
static int cursor_col;

// Packs a character and the default colour into the 16-bit VGA cell format.
static uint16_t make_cell(char c) {
    return (uint16_t)((VGA_COLOR << 8) | (uint8_t)c);
}

void terminal_init(void) {
    cursor_row = 0;
    cursor_col = 0;
    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            VGA_BUFFER[row * VGA_WIDTH + col] = make_cell(' ');
        }
    }
}

// Moves every row up by one line and blanks the last row.
// Called automatically when the cursor advances past row 24.
static void scroll(void) {
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            VGA_BUFFER[(row - 1) * VGA_WIDTH + col] =
                VGA_BUFFER[row * VGA_WIDTH + col];
        }
    }
    for (int col = 0; col < VGA_WIDTH; col++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = make_cell(' ');
    }
    cursor_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else {
        VGA_BUFFER[cursor_row * VGA_WIDTH + cursor_col] = make_cell(c);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    if (cursor_row >= VGA_HEIGHT) {
        scroll();
    }
}

void terminal_write(const char *str) {
    while (*str) {
        terminal_putchar(*str++);
    }
}

// printf helpers

static uint32_t strlen_local(const char *s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

// Writes the decimal representation of an unsigned 32-bit integer.
static void print_uint(uint32_t value, int base) {
    static const char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;

    if (value == 0) {
        terminal_putchar('0');
        return;
    }
    while (value) {
        buf[i++] = digits[value % (uint32_t)base];
        value   /= (uint32_t)base;
    }
    // buf holds digits in reverse order; print them right-to-left.
    while (i--) {
        terminal_putchar(buf[i]);
    }
}

// Writes the decimal representation of a signed 32-bit integer.
static void print_int(int32_t value) {
    if (value < 0) {
        terminal_putchar('-');
        // Cast to uint32_t to handle INT32_MIN safely (no UB on negation).
        print_uint((uint32_t)(-value), 10);
    } else {
        print_uint((uint32_t)value, 10);
    }
}

// printf
// Supported conversion specifiers:
// - %c - single character
// - %s - null-terminated string
// - %d - signed 32-bit decimal integer
// - %u - unsigned 32-bit decimal integer
// - %x - unsigned 32-bit hexadecimal integer (lowercase, no prefix)
// - %% - literal percent sign

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int count = 0;

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            terminal_putchar(*p);
            count++;
            continue;
        }

        p++; // Consume the '%' and look at the specifier.
        switch (*p) {
            case 'c': {
                // char is promoted to int when passed through '...'.
                char c = (char)va_arg(args, int);
                terminal_putchar(c);
                count++;
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                count += (int)strlen_local(s);
                terminal_write(s);
                break;
            }
            case 'd': {
                int32_t val = va_arg(args, int32_t);
                print_int(val);
                count++;
                break;
            }
            case 'u': {
                uint32_t val = va_arg(args, uint32_t);
                print_uint(val, 10);
                count++;
                break;
            }
            case 'x': {
                uint32_t val = va_arg(args, uint32_t);
                print_uint(val, 16);
                count++;
                break;
            }
            case '%': {
                terminal_putchar('%');
                count++;
                break;
            }
            default: {
                // Unknown specifier: echo it literally so nothing is silently lost.
                terminal_putchar('%');
                terminal_putchar(*p);
                count += 2;
                break;
            }
        }
    }

    va_end(args);
    return count;
}
