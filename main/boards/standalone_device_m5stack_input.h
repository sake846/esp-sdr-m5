#pragma once
#include <stdbool.h>

/* Keep a two-button menu chord pressed until both keys are released. */
static inline unsigned m5stack_device_input_two_buttons(unsigned pressed, bool *chord) {
    if (pressed == 3) *chord = true;
    if (*chord) {
        if (pressed) return 2;
        *chord = false;
        return 0;
    }
    return (pressed & 1) | ((pressed & 2) << 1);
}

static inline unsigned m5stack_device_input_touch_button(int x, int y, unsigned width, unsigned height) {
    if (x < 0 || y < 0 || (unsigned)x >= width || (unsigned)y >= height ||
        (unsigned)y < height * 3 / 4) return 0;
    return 1u << ((unsigned)x * 3 / width);
}
