#pragma once
#include <stdbool.h>
#include <stdint.h>

void m5stack_basic_init(void);
unsigned m5stack_basic_poll(unsigned mhz, bool pc_control);
unsigned m5stack_basic_rate(void);
void m5stack_basic_draw(unsigned mhz, const uint8_t bins[256], bool pc_control, bool valid);
