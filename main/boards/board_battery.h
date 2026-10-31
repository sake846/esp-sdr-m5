#pragma once
#include <stdint.h>
#include <stdio.h>

/* Returns -1 when the battery cannot be measured. */
int board_battery_percent(void);

static inline void board_status_text(char *text, size_t size, unsigned cpu_percent,
                                     unsigned gain_index, int64_t now_us) {
    static int64_t sampled_phase = -1;
    static int battery = -1;
    int64_t phase = now_us / 5000000;
    unsigned slot = (unsigned)(phase % 3);
    if (slot == 1 && sampled_phase != phase) {
        battery = board_battery_percent();
        sampled_phase = phase;
    }
    switch (slot) {
    case 0:
        if (cpu_percent >= 100) snprintf(text, size, "CPU100%%");
        else snprintf(text, size, "CPU %02u%%", cpu_percent);
        break;
    case 1:
        if (battery < 0 || battery > 100) snprintf(text, size, "BAT --%%");
        else if (battery == 100) snprintf(text, size, "BAT100%%");
        else snprintf(text, size, "BAT %02u%%", (unsigned)battery);
        break;
    default:
        snprintf(text, size, "GAIN  %02u", gain_index);
        break;
    }
}
