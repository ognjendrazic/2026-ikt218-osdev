# Assignment Two - GDT and Text Mode Terminal

## Overview

This assignment extends the kernel with two foundational components:

1. **Global Descriptor Table (GDT)**: tells the CPU how memory is segmented and what privilege level code runs at.
2. **VGA Text Mode Terminal**: writes characters directly to the hardware framebuffer so the kernel can display output.

Both must be in place before the kernel can do anything useful, so they are the first things initialised in `main`.

---

## Files Changed or Created

| File | Role |
|------|------|
| `src/multiboot2.asm` | Boot entry point - was already present but re-enabled and extended |
| `include/gdt.h` | GDT struct definitions and public API |
| `src/gdt.c` | Builds the three GDT entries and calls the assembly flush routine |
| `src/gdt.asm` | Executes `lgdt` and reloads all segment registers |
| `include/terminal.h` | Public interface for terminal and printf |
| `src/terminal.c` | VGA framebuffer driver and minimal printf |
| `src/kernel.c` | Kernel entry point - calls init routines and prints "Hello World" |
| `CMakeLists.txt` | Added all new source files to the build target |

---

## Global Descriptor Table

### Why the GDT exists

The x86 CPU in 32-bit protected mode does not use raw memory addresses directly. Every memory access goes through a **segment descriptor**, which tells the CPU the base address, size (limit), and access rights for that segment. The collection of all descriptors is the GDT.

Without a valid GDT, any memory access or privilege check will result in a General Protection Fault (#GP) or a Triple Fault, which resets the machine.

The Limine bootloader installs a temporary GDT to get the kernel running, but the kernel must replace it with its own as early as possible so it owns and understands its own memory map.

### The three descriptors

**Entry 0 - Null descriptor**  
The Intel architecture mandates that the first GDT entry is all zeros. Loading any segment register with selector `0x00` causes a #GP fault. This is by design and catches uninitialised segment registers.

**Entry 1 - Kernel Code segment (selector `0x08`)**  
- Base `0`, Limit `0xFFFFF` x 4 KB pages = 4 GB flat.
- Access byte `0x9A`: present, ring 0 (DPL=0), code descriptor, readable, non-conforming.
- Flags `0xC`: 4 KB granularity, 32-bit default operand size.
- The CPU fetches instructions through this segment. CS must always point here.

**Entry 2 - Kernel Data segment (selector `0x10`)**  
- Same base and limit as the code segment (flat model).
- Access byte `0x92`: present, ring 0, data descriptor, writable, grow-up.
- DS, ES, FS, GS, and SS all point here.

### Flat memory model

Both segments cover the full 4 GB address space starting at base 0. This is called the **flat memory model**: a logical address equals its physical address. It simplifies everything since no segment arithmetic is needed anywhere else in the kernel.

### Loading the GDT

The GDT is loaded in two steps:

1. **`lgdt`**: loads the 6-byte `gdt_ptr` structure (limit + base address) into the hidden GDTR register. The CPU now knows where the table is, but the segment register caches still hold the old values.

2. **Reload segment registers**: the CPU caches each segment's descriptor when the register is loaded. After `lgdt`, those caches are stale. All data registers (DS, ES, FS, GS, SS) can be updated with a `mov`. CS is special: it can only be changed via a control-transfer instruction. A **far jump** (`jmp 0x08:.label`) atomically loads CS with the new selector and flushes the instruction pipeline.

This two-step sequence is implemented in `src/gdt.asm` (`gdt_flush`), called from `gdt_init()` in `src/gdt.c`.

### Data layout

Each GDT entry is exactly 8 bytes. The fields are non-contiguous by hardware design (a historical artefact of 80286 to 80386 compatibility). The struct uses `__attribute__((packed))` to prevent the compiler from inserting padding, which would corrupt the layout the CPU expects.

---

## VGA Text Mode Terminal

### Why VGA text mode

The kernel has no operating system beneath it: no `printf`, no syscall, no standard library. The simplest way to display output on bare metal is the **VGA text mode framebuffer**, a region of memory at physical address `0xB8000` that the GPU reads and converts directly to pixels on screen.

### How it works

The buffer is 80 columns x 25 rows = 2000 cells. Each cell is 2 bytes:

- **Low byte**: ASCII character code.
- **High byte**: colour attribute. Bits 7:4 are the background colour, bits 3:0 are the foreground colour. `0x07` is light grey on black.

Writing a character is as simple as writing a 16-bit value to the correct offset in the buffer. There is no system call, no driver stack, no interrupt - just a memory write.

### Implementation

`terminal_init()` clears the screen by filling every cell with a space character.

`terminal_putchar()` places a character at the current cursor position and advances it. When the cursor reaches the end of a line it wraps to the next. When it passes the last row, `scroll()` copies every row up by one and blanks the bottom row.

`terminal_write()` calls `terminal_putchar` for each character in a string.

### printf

`printf` is implemented from scratch using the `va_list` / `va_arg` macros from `<libc/stdarg.h>` (backed by GCC builtins, which are safe in a freestanding environment).

Supported specifiers:

| Specifier | Meaning |
|-----------|---------|
| `%c` | Single character |
| `%s` | Null-terminated string |
| `%d` | Signed 32-bit decimal |
| `%u` | Unsigned 32-bit decimal |
| `%x` | Unsigned 32-bit hexadecimal (lowercase) |
| `%%` | Literal `%` |

Integer-to-string conversion works by repeatedly dividing by the base and collecting remainders into a local buffer, then printing the buffer in reverse.

---

## Boot Flow After This Assignment

```
Limine bootloader
    |
    +-> _start  (multiboot2.asm)
            cli                     ; disable interrupts
            mov esp, stack_top      ; set up boot stack
            push ebx / push eax     ; pass magic + mbi to main
            call main
            |
            +-> main  (kernel.c)
                    gdt_init()
                    |   set_entry x 3   ; build null / code / data
                    |   gdt_flush()
                    |       lgdt [eax]  ; load GDTR
                    |       jmp 0x08    ; reload CS
                    |       mov ds/es/fs/gs/ss, 0x10
                    |
                    terminal_init()     ; clear VGA framebuffer
                    printf("Hello World\n")
```

---

## Key References

- Intel SDM Vol. 3A 3.4 - Segment Descriptors
- Intel SDM Vol. 3A 3.5 - System Descriptor Types (null entry)
- OSDev Wiki - [GDT Tutorial](https://wiki.osdev.org/GDT_Tutorial)
- OSDev Wiki - [Printing to Screen](https://wiki.osdev.org/Printing_to_Screen)
