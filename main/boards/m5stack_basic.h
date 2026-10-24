#pragma once
#include <stdbool.h>
#include <stdint.h>

void m5stack_basic_init(void);
unsigned m5stack_basic_poll(unsigned mhz, bool pc_control);
unsigned m5stack_basic_rate(void);
unsigned m5stack_basic_fft_points(void);
unsigned m5stack_basic_average_count(void);
void m5stack_basic_draw(unsigned mhz, const uint8_t *bins, unsigned fft_points, bool pc_control,
                        bool valid, unsigned cpu_percent);
