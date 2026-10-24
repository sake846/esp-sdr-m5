#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "rx_tuning.h"

typedef struct {
    unsigned raw, stable, step, rate;
    uint32_t changed[3], b_pressed;
    bool b_long;
} m5stack_controls_t;

static const unsigned m5stack_steps[] = {1, 5, 20};
static const unsigned m5stack_rates[] = {6, 1, 0};
static const unsigned m5stack_spans[] = {16, 40, 80};

/* GPIO37-39 have external pull-ups; debounce without blocking the serial loop. */
static inline unsigned m5stack_controls_update(m5stack_controls_t *s, unsigned mhz, unsigned pressed,
                                               uint32_t now, bool pc_control) {
    if (pc_control) {
        s->raw = s->stable = pressed;
        s->b_long = true;
        for (unsigned j = 0; j < 3; j++) s->changed[j] = now;
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
                s->step = (s->step + 1) % 3;
            }
        } else if (s->stable & bit) {
            unsigned step = m5stack_steps[s->step];
            if (j == 0)
                mhz = mhz - RX_FREQ_MIN < step ? RX_FREQ_MIN : mhz - step;
            else
                mhz = RX_FREQ_MAX - mhz < step ? RX_FREQ_MAX : mhz + step;
        }
    }
    if ((s->stable & 2) && !s->b_long && (uint32_t)(now - s->b_pressed) >= 600) {
        s->rate = (s->rate + 1) % 3;
        s->b_long = true;
    }
    return mhz;
}
