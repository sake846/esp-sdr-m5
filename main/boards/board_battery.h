#pragma once
#include <stdint.h>
#include <stdio.h>

/* Returns -1 when the battery cannot be measured. */
int board_battery_percent(void);

static inline void board_status_text(char *text, size_t size, unsigned cpu_percent, int64_t now_us) {
    static int64_t sampled_phase = -1;
    static int battery = -1;
    int64_t phase = now_us / 5000000;
    if (phase % 2 && sampled_phase != phase) {
        battery = board_battery_percent();
        sampled_phase = phase;
    }
    if (phase % 2 && battery >= 0 && battery <= 100)
        snprintf(text, size, "BAT %u%%", (unsigned)battery);
    else
        snprintf(text, size, "CPU %u%%", cpu_percent);
}
