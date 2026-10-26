#pragma once
#include "m5stack_basic_controls.h"

/* Ignore impossible two-bit jumps rather than inventing encoder movement. */
static inline int lilygo_encoder_delta(unsigned previous, unsigned current) {
    static const int8_t transitions[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    return transitions[(previous & 3) * 4 + (current & 3)];
}

static inline unsigned lilygo_key(m5stack_controls_t *s, unsigned mhz, char key, bool pc_control) {
    if (pc_control) return mhz;
    if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
    switch (key) {
    case 'a': case 'h': case ',': case '-': return m5stack_menu_adjust(s, mhz, -1);
    case 'd': case 'l': case '.': case '+': return m5stack_menu_adjust(s, mhz, 1);
    case ' ': case 'm': case '\r': case '\n': m5stack_menu_move(s, false); break;
    case 'q': case 27: s->menu = M5_MENU_NONE; break;
    }
    return mhz;
}

/* TCA8418 press events have bit 7 set; columns run right to left. */
static inline char lilygo_tca_key(unsigned event) {
    static const char keys[4][10] = {
        {'q','w','e','r','t','y','u','i','o','p'},
        {'a','s','d','f','g','h','j','k','l','\n'},
        {0,'z','x','c','v','b','n','m',0,0},
        {' ',0,0,0,0,0,0,0,0,0},
    };
    unsigned code = event & 0x7f;
    if (!(event & 0x80) || !code || code > 40) return 0;
    return keys[(code - 1) / 10][9 - (code - 1) % 10];
}

/* Invert the dark RGB UI onto white e-paper, with ordered monochrome shading. */
static inline bool lilygo_epaper_ink(uint16_t rgb, unsigned x, unsigned y) {
    static const uint8_t thresholds[4][4] = {
        {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5},
    };
    unsigned red = (rgb >> 11) & 31, green = (rgb >> 6) & 31, blue = rgb & 31;
    unsigned level = red > green ? red : green;
    if (blue > level) level = blue;
    return level > thresholds[y & 3][x & 3] * 2;
}
