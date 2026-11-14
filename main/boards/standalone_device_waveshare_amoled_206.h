#pragma once
#include <stdint.h>

#define SCREEN_WIDTH 410
#define SCREEN_HEIGHT 502
#define LILYGO_FOOTER_HEIGHT 60
#define LILYGO_NAME "WAVESHARE AMOLED 2.06"

#define PLOT_LEFT 4
#define PLOT_WIDTH (SCREEN_WIDTH - 8)
#define SPECTRUM_TOP 14
#define SPECTRUM_BOTTOM (SCREEN_HEIGHT / 3)
#define FREQUENCY_TOP (SPECTRUM_BOTTOM + 3)
#define WATERFALL_TOP (FREQUENCY_TOP + 13)
#define WATERFALL_HEIGHT (SCREEN_HEIGHT - WATERFALL_TOP - LILYGO_FOOTER_HEIGHT)

#ifdef __cplusplus
extern "C" {
#endif
void waveshare_amoled_206_init(void);
unsigned waveshare_amoled_206_buttons(void);
void waveshare_amoled_206_row(unsigned y, const uint16_t *pixels);
void waveshare_amoled_206_flush(void);
void waveshare_amoled_206_power_off(void);
#ifdef __cplusplus
}
#endif
