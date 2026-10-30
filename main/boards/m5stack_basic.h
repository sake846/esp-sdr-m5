#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { M5_AGC_MANUAL, M5_AGC_SOFTWARE, M5_AGC_HARDWARE };

void m5stack_basic_init(void);
unsigned m5stack_basic_poll(unsigned mhz, bool pc_control);
unsigned m5stack_basic_rate(void);
unsigned m5stack_basic_span(void);
unsigned m5stack_basic_fft_points(void);
unsigned m5stack_basic_average_count(void);
bool m5stack_basic_sync_rx(unsigned *agc, unsigned *gain, unsigned gain_max,
                           int *filter, bool *follow_span);
void m5stack_basic_set_gain_index(unsigned gain_index);
/* frame_count is the number of successful FFTs included in the averaged frame. */
void m5stack_basic_set_density_context(unsigned gain, int filter, unsigned dc, unsigned frame_count);
/* `bins` are compact power codes; the renderer stores display levels as dBFS. */
void m5stack_basic_draw(unsigned mhz, const uint8_t *bins, unsigned fft_points, bool pc_control,
                        bool valid, unsigned cpu_percent);
