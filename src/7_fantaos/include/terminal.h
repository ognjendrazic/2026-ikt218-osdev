#pragma once

// Clears the VGA text-mode screen and resets the cursor to (0, 0).
void terminal_init(void);

// Writes a single character to the current cursor position and advances it.
// Handles '\n' (newline) and scrolls when the bottom row is exceeded.
void terminal_putchar(char c);

// Writes a null-terminated string using terminal_putchar.
void terminal_write(const char *str);

// Minimal printf supporting: %c, %s, %d, %u, %x, %%.
// Returns the number of characters written.
int printf(const char *fmt, ...);
