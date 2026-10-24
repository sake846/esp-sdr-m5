#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "rx_tuning.h"

typedef struct {
    unsigned raw, stable, step, rate, fft, average;
    uint32_t changed[3], pressed_at[3], b_pressed;
    bool suppress_release[3];
    bool b_long;
} m5stack_controls_t;

static const unsigned m5stack_steps[] = {1, 5, 20, 100};
static const unsigned m5stack_rates[] = {6, 6, 6, 6, 1, 0};
static const unsigned m5stack_spans[] = {2, 4, 8, 16, 40, 80};
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
static const unsigned m5stack_fft_sizes[] = {256, 512, 1024, 2048, 4096};
#else
static const unsigned m5stack_fft_sizes[] = {256, 512, 1024, 2048, 4096, 8192};
#endif
static const unsigned m5stack_average_counts[] = {1, 2, 4, 8, 16};

/* GPIO37-39 have external pull-ups; debounce without blocking the serial loop. */
static inline unsigned m5stack_controls_update(m5stack_controls_t *s, unsigned mhz, unsigned pressed,
                                               uint32_t now, bool pc_control) {
    if (pc_control) {
        s->raw = s->stable = pressed;
        s->b_long = true;
        for (unsigned j = 0; j < 3; j++) {
            s->changed[j] = s->pressed_at[j] = now;
            s->suppress_release[j] = true;
        }
        return mhz;
    }
    for (unsigned j = 0; j < 3; j++) {
        unsigned bit = 1u << j;
        if ((pressed ^ s->raw) & bit) {
            s->raw ^= bit;
            s->changed[j] = now;
        }
        if (!((s->raw ^ s->stable) & bit) || (uint32_t)(now - s->changed[j]) < 30) continue;
        s->stable ^= bit;
        if (j == 1) {
            if (s->stable & bit) {
                s->b_pressed = now;
                s->b_long = false;
            } else if (!s->b_long) {
                s->step = (s->step + 1) % (sizeof(m5stack_steps) / sizeof(m5stack_steps[0]));
            }
        } else if (s->stable & bit) {
            s->pressed_at[j] = now;
            s->suppress_release[j] = false;
        } else if (!s->suppress_release[j]) {
            unsigned step = m5stack_steps[s->step];
            if (j == 0)
                mhz = mhz - RX_FREQ_MIN < step ? RX_FREQ_MIN : mhz - step;
            else
                mhz = RX_FREQ_MAX - mhz < step ? RX_FREQ_MAX : mhz + step;
        }
    }
    if ((s->stable & 2) && !s->b_long && (uint32_t)(now - s->b_pressed) >= 600) {
        s->fft = (s->fft + 1) % (sizeof(m5stack_fft_sizes) / sizeof(m5stack_fft_sizes[0]));
        s->b_long = true;
    }
    if ((s->stable & 1) && !s->suppress_release[0] && (uint32_t)(now - s->pressed_at[0]) >= 600) {
        s->rate = (s->rate + 1) % (sizeof(m5stack_spans) / sizeof(m5stack_spans[0]));
        s->suppress_release[0] = true;
    }
    if ((s->stable & 4) && !s->suppress_release[2] && (uint32_t)(now - s->pressed_at[2]) >= 600) {
        s->average = (s->average + 1) % (sizeof(m5stack_average_counts) / sizeof(m5stack_average_counts[0]));
        s->suppress_release[2] = true;
    }
    return mhz;
}
