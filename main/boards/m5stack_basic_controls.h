#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "rx_tuning.h"
#include "m5stack_basic.h"

typedef enum {
    M5_MENU_NONE, M5_MENU_STEP, M5_MENU_SPAN, M5_MENU_Y_POS, M5_MENU_WF_BRIGHT,
    M5_MENU_WF_CONTRAST, M5_MENU_WF_SPEED, M5_MENU_FFT, M5_MENU_AVERAGE,
    M5_MENU_SPECTRUM, M5_MENU_AGC, M5_MENU_GAIN, M5_MENU_FILTER, M5_MENU_ABOUT, M5_MENU_RESET,
} m5stack_menu_t;
enum { M5_RX_AGC = 1, M5_RX_GAIN = 2, M5_RX_FILTER = 4 };

typedef struct {
    unsigned raw, stable, step, rate, fft, average;
    m5stack_menu_t menu;
    int y_pos, wf_bright, wf_contrast, filter;
    unsigned wf_speed, agc, gain, gain_max, rx_dirty, about;
    uint32_t changed[3], b_pressed, last_input;
    bool suppress_release[3];
    bool b_long, reset_done, dpx, dpx_failed;
} m5stack_controls_t;

static const unsigned m5stack_steps[] = {1, 5, 20, 100};
static const unsigned m5stack_rates[] = {6, 6, 6, 6, 1, 0};
static const unsigned m5stack_spans[] = {2, 4, 8, 16, 40, 80};
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV || (CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI)
static const unsigned m5stack_fft_sizes[] = {256, 512, 1024, 2048, 4096};
#else
static const unsigned m5stack_fft_sizes[] = {256, 512, 1024, 2048, 4096, 8192};
#endif
static const unsigned m5stack_average_counts[] = {1, 2, 4, 8, 16};
static const unsigned m5stack_wf_intervals[] = {10, 5, 2, 1};

static inline void m5stack_controls_defaults(m5stack_controls_t *s) {
    s->step = 0;
    s->rate = 3;
    s->fft = 2;
    s->average = 4;
    s->y_pos = s->wf_bright = 0;
    s->wf_contrast = 100;
    s->wf_speed = 3;
    s->agc = M5_AGC_SOFTWARE;
    s->gain = 30;
    s->filter = -1; /* Follow the selected span. */
    s->about = 0;
    s->dpx = s->dpx_failed = false;
}

static inline int m5stack_adjust(int value, int direction, int step, int minimum, int maximum) {
    value += direction * step;
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static inline void m5stack_menu_move(m5stack_controls_t *s, bool reverse) {
    s->menu = reverse ? (s->menu <= M5_MENU_STEP ? M5_MENU_RESET : s->menu - 1) :
                       (s->menu == M5_MENU_RESET ? M5_MENU_STEP : s->menu + 1);
    s->reset_done = false;
}

static inline unsigned m5stack_menu_adjust(m5stack_controls_t *s, unsigned mhz, int direction) {
    switch (s->menu) {
    case M5_MENU_NONE: {
        unsigned step = m5stack_steps[s->step];
        return direction < 0 ? (mhz - RX_FREQ_MIN < step ? RX_FREQ_MIN : mhz - step) :
                               (RX_FREQ_MAX - mhz < step ? RX_FREQ_MAX : mhz + step);
    }
    case M5_MENU_STEP: s->step = m5stack_adjust(s->step, direction, 1, 0, 3); break;
    case M5_MENU_SPAN: s->rate = m5stack_adjust(s->rate, direction, 1, 0, 5); break;
    case M5_MENU_Y_POS: s->y_pos = m5stack_adjust(s->y_pos, direction, 5, -40, 40); break;
    case M5_MENU_WF_BRIGHT: s->wf_bright = m5stack_adjust(s->wf_bright, direction, 5, -30, 30); break;
    case M5_MENU_WF_CONTRAST: s->wf_contrast = m5stack_adjust(s->wf_contrast, direction, 25, 25, 200); break;
    case M5_MENU_WF_SPEED: s->wf_speed = m5stack_adjust(s->wf_speed, direction, 1, 0, 3); break;
    case M5_MENU_FFT:
        s->fft = m5stack_adjust(s->fft, direction, 1, 0,
                               sizeof(m5stack_fft_sizes) / sizeof(m5stack_fft_sizes[0]) - 1);
        break;
    case M5_MENU_AVERAGE: s->average = m5stack_adjust(s->average, direction, 1, 0, 4); break;
    case M5_MENU_SPECTRUM: s->dpx = !s->dpx; s->dpx_failed = false; break;
    case M5_MENU_AGC:
        s->agc = (s->agc + (direction < 0 ? 2 : 1)) % 3;
        s->rx_dirty |= M5_RX_AGC;
        break;
    case M5_MENU_GAIN:
        s->gain = m5stack_adjust(s->gain, direction, 1, 0, s->gain_max);
        s->agc = M5_AGC_MANUAL;
        s->rx_dirty |= M5_RX_AGC | M5_RX_GAIN;
        break;
    case M5_MENU_FILTER:
        s->filter = m5stack_adjust(s->filter, direction, 1, -1, 127);
        s->rx_dirty |= M5_RX_FILTER;
        break;
    case M5_MENU_ABOUT: s->about ^= 1; break;
    case M5_MENU_RESET:
        if (direction < 0) s->menu = M5_MENU_NONE;
        else {
            m5stack_controls_defaults(s);
            s->rx_dirty = M5_RX_AGC | M5_RX_GAIN | M5_RX_FILTER;
            s->reset_done = true;
            mhz = 2412;
        }
        break;
    }
    return mhz;
}

/* Synchronize PC/AGC changes without reapplying stale local values. */
static inline bool m5stack_controls_sync_rx(m5stack_controls_t *s, unsigned *agc, unsigned *gain,
                                           unsigned gain_max, int *filter, bool *follow_span) {
    unsigned dirty = s->rx_dirty;
    s->gain_max = gain_max;
    if (dirty & M5_RX_AGC) *agc = s->agc;
    if (dirty & M5_RX_GAIN) *gain = s->gain > gain_max ? gain_max : s->gain;
    if (dirty & M5_RX_FILTER) {
        *follow_span = s->filter < 0;
        *filter = s->filter;
    }
    s->agc = *agc;
    s->gain = *gain;
    s->filter = *follow_span ? -1 : *filter;
    s->rx_dirty = 0;
    return dirty != 0;
}

/* GPIO37-39 have external pull-ups; debounce without blocking the serial loop. */
static inline unsigned m5stack_controls_update(m5stack_controls_t *s, unsigned mhz, unsigned pressed,
                                               uint32_t now, bool pc_control) {
    if (pc_control) {
        s->raw = s->stable = pressed;
        s->menu = M5_MENU_NONE;
        s->rx_dirty = 0;
        s->b_long = true;
        for (unsigned j = 0; j < 3; j++) {
            s->changed[j] = now;
            s->suppress_release[j] = true;
        }
        return mhz;
    }
    if (!s->raw && !s->stable && (uint32_t)(now - s->last_input) >= 5000) s->menu = M5_MENU_NONE;
    if (pressed || s->raw || s->stable) s->last_input = now;
    for (unsigned j = 0; j < 3; j++) {
        unsigned bit = 1u << j;
        if ((pressed ^ s->raw) & bit) {
            s->raw ^= bit;
            s->changed[j] = now;
        }
        if (!((s->raw ^ s->stable) & bit) || (uint32_t)(now - s->changed[j]) < 30) continue;
        s->stable ^= bit;
        s->last_input = now;
        if (j == 1) {
            if (s->stable & bit) {
                s->b_pressed = now;
                s->b_long = false;
            } else if (!s->b_long) {
                m5stack_menu_move(s, false);
            }
        } else if (s->stable & bit) {
            s->suppress_release[j] = false;
        } else if (!s->suppress_release[j]) {
            mhz = m5stack_menu_adjust(s, mhz, j == 0 ? -1 : 1);
        }
    }
    if ((s->stable & 2) && !s->b_long && (uint32_t)(now - s->b_pressed) >= 600) {
        m5stack_menu_move(s, true);
        s->b_long = true;
    }
    return mhz;
}
