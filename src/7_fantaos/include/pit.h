#pragma once

#include <libc/stdint.h>

#define PIT_CMD_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

#define PIT_BASE_FREQUENCY 1193180
#define TARGET_FREQUENCY 1000 // 1000 Hz - 1 tick per ms
#define DIVIDER (PIT_BASE_FREQUENCY / TARGET_FREQUENCY)
#define TICKS_PER_MS (TARGET_FREQUENCY / TARGET_FREQUENCY) // = 1

void init_pit(void);
uint32_t get_tick(void);
void sleep_busy(uint32_t milliseconds);
void sleep_interrupt(uint32_t milliseconds);
