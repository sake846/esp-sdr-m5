#pragma once
#include "standalone_display_controls.h"

/* Ignore impossible two-bit jumps rather than inventing encoder movement. */
static inline int standalone_device_lilygo_ats_mini_encoder_delta(unsigned previous, unsigned current) {
    static const int8_t transitions[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    return transitions[(previous & 3) * 4 + (current & 3)];
}

static inline unsigned lilygo_key_digit(m5stack_controls_t *s, unsigned mhz, unsigned digit) {
    if (s->keyboard_frequency_length == 4) {
        s->keyboard_frequency_length = s->keyboard_frequency_value = 0;
    }
    s->keyboard_frequency_value = s->keyboard_frequency_value * 10 + digit;
    if (++s->keyboard_frequency_length == 4) {
        if (rx_frequency_valid(s->keyboard_frequency_value)) mhz = s->keyboard_frequency_value;
        s->keyboard_frequency_length = s->keyboard_frequency_value = 0;
    }
    return mhz;
}

static inline unsigned lilygo_key(m5stack_controls_t *s, unsigned mhz, char key, bool pc_control) {
    if (pc_control) return mhz;
#if CONFIG_ESP_SDR_LILYGO_T_PAGER || CONFIG_ESP_SDR_LILYGO_T_DECK
    bool reverse = key >= 'A' && key <= 'Z';
#endif
    if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
#if CONFIG_ESP_SDR_LILYGO_T_PAGER || CONFIG_ESP_SDR_LILYGO_T_DECK
    if (key >= '0' && key <= '9') return lilygo_key_digit(s, mhz, (unsigned)(key - '0'));
    switch (key) {
    case 'h': case ',': case '-': return m5stack_menu_adjust(s, mhz, -1);
    case 'd': case 'l': case '.': case '+': return m5stack_menu_adjust(s, mhz, 1);
    case 't': s->step = (s->step + (reverse ? 4 : 1)) % 5; break;
    case 'p': s->rate = (s->rate + (reverse ? 5 : 1)) % 6; break;
    case 'f': s->fft = (s->fft + (reverse ? 4 : 1)) % 5; break;
    case 'a': s->average = (s->average + (reverse ? 4 : 1)) % 5; break;
    }
#else
    switch (key) {
    case 'a': case 'h': case ',': case '-': return m5stack_menu_adjust(s, mhz, -1);
    case 'd': case 'l': case '.': case '+': return m5stack_menu_adjust(s, mhz, 1);
    }
#endif
    switch (key) {
    case ' ': case 'm': case '\r': case '\n': m5stack_menu_move(s, false); break;
    case 'q': case 27: s->menu = M5_MENU_NONE; break;
    }
    return mhz;
}

static inline void lilygo_keyboard_cancel(m5stack_controls_t *s) {
    s->keyboard_frequency_length = s->keyboard_frequency_value = 0;
    s->keyboard_space_pending = s->keyboard_space_down = s->keyboard_space_used = false;
}

static inline unsigned lilygo_keyboard_chord_digit(char key) {
    switch (key) {
    case 'q': return 1;
    case 'w': return 2;
    case 'e': return 3;
    case 'r': return 4;
    case 't': return 5;
    case 'y': return 6;
    case 'u': return 7;
    case 'i': return 8;
    case 'o': return 9;
    case 'p': return 0;
    default: return 10;
    }
}

static inline unsigned lilygo_key_event(m5stack_controls_t *s, unsigned mhz, char key,
                                         bool pc_control, uint32_t now) {
#if CONFIG_ESP_SDR_LILYGO_T_PAGER || CONFIG_ESP_SDR_LILYGO_T_DECK
    if (pc_control) {
        lilygo_keyboard_cancel(s);
        return mhz;
    }
    if (s->keyboard_space_pending && !s->keyboard_space_down &&
        (int32_t)(now - s->keyboard_space_deadline) >= 0) {
        s->keyboard_space_pending = false;
        mhz = lilygo_key(s, mhz, ' ', false);
    }
    if (key == ' ') {
        s->keyboard_space_pending = true;
        s->keyboard_space_used = false;
        s->keyboard_space_deadline = now + 300;
        return mhz;
    }
    char chord_key = key >= 'A' && key <= 'Z' ? key + ('a' - 'A') : key;
    unsigned digit = lilygo_keyboard_chord_digit(chord_key);
    if (s->keyboard_space_down && digit < 10) {
        s->keyboard_space_used = true;
        return lilygo_key_digit(s, mhz, digit);
    }
    if (s->keyboard_space_pending && digit < 10) {
        s->keyboard_space_pending = false;
        return lilygo_key_digit(s, mhz, digit);
    }
    if (s->keyboard_space_pending) {
        s->keyboard_space_pending = false;
        mhz = lilygo_key(s, mhz, ' ', false);
    }
    return lilygo_key(s, mhz, key, false);
#else
    return lilygo_key(s, mhz, key, pc_control);
#endif
}

static inline unsigned lilygo_key_expire(m5stack_controls_t *s, unsigned mhz, uint32_t now) {
    if (s->keyboard_space_pending && !s->keyboard_space_down &&
        (int32_t)(now - s->keyboard_space_deadline) >= 0) {
        s->keyboard_space_pending = false;
        s->last_input = now;
        return lilygo_key(s, mhz, ' ', false);
    }
    return mhz;
}

/* TCA8418 press events have bit 7 set; columns run right to left. */
static inline char lilygo_tca_matrix_key(unsigned event) {
    static const char keys[4][10] = {
        {'q','w','e','r','t','y','u','i','o','p'},
        {'a','s','d','f','g','h','j','k','l','\n'},
        {0,'z','x','c','v','b','n','m',0,0},
        {' ',0,0,0,0,0,0,0,0,0},
    };
    unsigned code = event & 0x7f;
    if (!code || code > 40) return 0;
    return keys[(code - 1) / 10][9 - (code - 1) % 10];
}

static inline unsigned lilygo_tca_key_event(m5stack_controls_t *s, unsigned mhz, unsigned event,
                                             bool pc_control, uint32_t now) {
#if CONFIG_ESP_SDR_LILYGO_T_PAGER || CONFIG_ESP_SDR_LILYGO_T_DECK
    char key = lilygo_tca_matrix_key(event);
    if (!key) return mhz;
    bool pressed = (event & 0x80) != 0;
    if (pc_control) {
        lilygo_keyboard_cancel(s);
        return mhz;
    }
    if (key == ' ') {
        if (pressed) {
            s->keyboard_space_pending = s->keyboard_space_down = true;
            s->keyboard_space_used = false;
            s->keyboard_space_deadline = now + 300;
        } else {
            if (s->keyboard_space_pending && !s->keyboard_space_used)
                mhz = lilygo_key(s, mhz, ' ', false);
            s->keyboard_space_pending = s->keyboard_space_down = s->keyboard_space_used = false;
        }
        return mhz;
    }
    return pressed ? lilygo_key_event(s, mhz, key, false, now) : mhz;
#else
    char key = event & 0x80 ? lilygo_tca_matrix_key(event) : 0;
    return key ? lilygo_key(s, mhz, key, pc_control) : mhz;
#endif
}

static inline char lilygo_tca_key(unsigned event) {
    return event & 0x80 ? lilygo_tca_matrix_key(event) : 0;
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
