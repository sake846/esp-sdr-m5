#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { M5_AGC_MANUAL, M5_AGC_SOFTWARE, M5_AGC_HARDWARE };

void standalone_display_init(void);
unsigned standalone_display_poll(unsigned mhz, bool pc_control);
unsigned standalone_display_rate(void);
unsigned standalone_display_span(void);
unsigned standalone_display_fft_points(void);
unsigned standalone_display_average_count(void);
bool standalone_display_sync_rx(unsigned *agc, unsigned *gain, unsigned gain_max,
                           int *filter, bool *follow_span);
void standalone_display_set_gain_index(unsigned gain_index);
/* frame_count is the number of successful FFTs included in the averaged frame. */
void standalone_display_set_density_context(unsigned gain, int filter, unsigned dc, unsigned frame_count);
/* `bins` are compact power codes; the renderer stores display levels as dBFS. */
void standalone_display_draw(unsigned mhz, const uint8_t *bins, unsigned fft_points, bool pc_control,
                        bool valid, unsigned cpu_percent);
