#pragma once
#include <stdlib.h>

/* Shared scanline density display; included after the board geometry/controls. */
static uint8_t *density;
static unsigned density_width;
static unsigned density_mhz, density_span, density_fft, density_average, density_gain, density_dc;
static int density_filter;
static int64_t density_decay_at;
static bool density_keyed, density_pc;

static void density_clear(void) {
    if (density) memset(density, 0, 64 * density_width);
    density_keyed = false;
    density_decay_at = esp_timer_get_time();
}

static void density_mode(unsigned mhz, bool pc_control) {
    if (!controls.dpx) {
        free(density);
        density = NULL;
        density_width = 0;
        density_keyed = false;
    } else {
        unsigned span = m5stack_basic_span(), width = m5stack_basic_fft_points();
        if (span < 16) width = width * span / 16;
        if (width > PLOT_WIDTH) width = PLOT_WIDTH;
        /* Zoomed pixels sharing one FFT bin also share one history column. */
        if (density && density_width != width) { free(density); density = NULL; }
        if (!density) {
            density = heap_caps_calloc(64, width,
#if (CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI || CONFIG_ESP_SDR_M5UNIFIED) && CONFIG_SPIRAM
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
            density_width = width;
            controls.dpx_failed = density == NULL;
            if (!density) controls.dpx = false;
            density_keyed = false;
            density_decay_at = esp_timer_get_time();
        }
    }
    if (density && ((density_pc && !pc_control) ||
        (density_keyed && (density_mhz != mhz || density_span != m5stack_basic_span() ||
         density_fft != m5stack_basic_fft_points() || density_average != m5stack_basic_average_count() ||
         controls.rx_dirty)))) density_clear();
    density_pc = pc_control;
}

static void density_decay(void) {
    if (!density || density_pc) return;
    int64_t now = esp_timer_get_time();
    unsigned ticks = (unsigned)((now - density_decay_at) / 100000);
    if (!ticks) return;
    density_decay_at += (int64_t)ticks * 100000;
    unsigned factor = 65536;
    while (ticks-- && factor) factor = factor * 15 / 16;
    for (unsigned i = 0; i < 64 * density_width; i++)
        density[i] = (uint8_t)((density[i] * factor) >> 16);
}

void m5stack_basic_spectrum(unsigned mhz, const uint8_t *bins, unsigned fft_points,
                             unsigned gain, int filter, unsigned dc) {
    if (!density || density_pc || !bins) return;
    unsigned span = m5stack_basic_span(), average = m5stack_basic_average_count();
    if (!density_keyed || density_mhz != mhz || density_span != span || density_fft != fft_points ||
        density_average != average || density_gain != gain || density_filter != filter || density_dc != dc) {
        density_clear();
        density_mhz = mhz;
        density_span = span;
        density_fft = fft_points;
        density_average = average;
        density_gain = gain;
        density_filter = filter;
        density_dc = dc;
        density_keyed = true;
    }
    density_decay();
    unsigned visible = span < 16 ? fft_points * span / 16 : fft_points;
    unsigned origin = visible / 2 - 1, mask = fft_points - 1;
    for (unsigned x = 0; x < density_width; x++) {
        unsigned first = density_width < PLOT_WIDTH ? x : x * visible / PLOT_WIDTH;
        unsigned limit = density_width < PLOT_WIDTH ? x + 1 : (x + 1) * visible / PLOT_WIDTH;
        unsigned peak = 0;
        for (unsigned j = first; j < limit; j++) {
            unsigned code = bins[(origin - j) & mask];
            if (code > peak) peak = code;
        }
        /* shortcut: weighted, saturating density; add wider counters for probability measurements. */
        uint8_t *cell = density + (peak >> 2) * density_width + x;
        unsigned value = *cell + 8;
        *cell = (uint8_t)(value > 255 ? 255 : value);
    }
}

static void density_row(unsigned y, int lower_dbfs) {
    static const uint16_t colors[] = {0, 0x001f, 0x021f, 0x041f, 0x061f, 0x07ff, 0x07f8,
        0x07f0, 0x07e0, 0x47e0, 0x87e0, 0xc7e0, 0xffe0, 0xfd20, 0xfa00, 0xf800, 0xffff};
    /* Bounds in tenths of a power code; intersect every bucket touched by this pixel row. */
    int lower = lower_dbfs * 20 + 1686, position = SPECTRUM_BOTTOM - (int)y;
    int low = lower + (position - 1) * 1600 / (SPECTRUM_BOTTOM - SPECTRUM_TOP);
    int high = lower + position * 1600 / (SPECTRUM_BOTTOM - SPECTRUM_TOP);
    if (high <= 0 || low >= 2560) return;
    unsigned first = (low < 0 ? 0 : low) / 40;
    unsigned last = ((high > 2560 ? 2560 : high) - 1) / 40;
    const uint8_t *first_row = density + first * density_width;
    const uint8_t *last_row = density + last * density_width;
    for (unsigned x = 0; x < PLOT_WIDTH; x++) {
        unsigned column = density_width == PLOT_WIDTH ? x : x * density_width / PLOT_WIDTH;
        unsigned value = first_row[column];
        for (const uint8_t *p = first_row + density_width; p <= last_row; p += density_width)
            if (p[column] > value) value = p[column];
        if (value) row[PLOT_LEFT + x] = __builtin_bswap16(colors[1 + (value >> 4)]);
    }
}
