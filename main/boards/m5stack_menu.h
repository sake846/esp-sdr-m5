#pragma once

/* Shared scanline overlay, using each display's row buffer and 3x5 glyphs. */
static const char *const m5stack_menu_names[] = {
    "", "FREQ STEP", "FREQ SPAN", "Y POS", "WF BRIGHT", "WF CONTRAST", "WF SPEED",
    "FFT NUM", "FFT AVERAGE", "AGC TYPE", "RF GAIN", "FILTER", "ABOUT", "RESET",
};

static void m5stack_menu_value(char *value, size_t size) {
    switch (controls.menu) {
    case M5_MENU_STEP: snprintf(value, size, "%u MHZ", m5stack_steps[controls.step]); break;
    case M5_MENU_SPAN: snprintf(value, size, "%u MHZ", m5stack_spans[controls.rate]); break;
    case M5_MENU_Y_POS: snprintf(value, size, "%+d DB", controls.y_pos); break;
    case M5_MENU_WF_BRIGHT: snprintf(value, size, "%+d DB", controls.wf_bright); break;
    case M5_MENU_WF_CONTRAST: snprintf(value, size, "%d%%", controls.wf_contrast); break;
    case M5_MENU_WF_SPEED:
        if (controls.wf_speed == 3) snprintf(value, size, "1X");
        else snprintf(value, size, "1/%uX", m5stack_wf_intervals[controls.wf_speed]);
        break;
    case M5_MENU_FFT: snprintf(value, size, "%u", m5stack_fft_sizes[controls.fft]); break;
    case M5_MENU_AVERAGE: snprintf(value, size, "%u", m5stack_average_counts[controls.average]); break;
    case M5_MENU_AGC:
        snprintf(value, size, "%s", controls.agc == M5_AGC_MANUAL ? "MANUAL" :
                 controls.agc == M5_AGC_SOFTWARE ? "SOFTWARE" : "HARDWARE"); break;
    case M5_MENU_GAIN: snprintf(value, size, "%u", controls.gain); break;
    case M5_MENU_FILTER:
        if (controls.filter < 0) snprintf(value, size, "AUTO");
        else if (!controls.filter) snprintf(value, size, "WIDE");
        else snprintf(value, size, "DCAP %d", controls.filter);
        break;
    case M5_MENU_ABOUT:
#if CONFIG_ESP_SDR_M5STACK_CORE2
        snprintf(value, size, "%s", controls.about ? "CORE2" : "ESP SDR M5");
#elif CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
        snprintf(value, size, "%s", controls.about ? "STICKC PLUS2" : "ESP SDR M5");
#else
        snprintf(value, size, "%s", controls.about ? "BASIC" : "ESP SDR M5");
#endif
        break;
    case M5_MENU_RESET: snprintf(value, size, "%s", controls.reset_done ? "DONE" : "RIGHT RESET"); break;
    default: value[0] = '\0'; break;
    }
}

static void m5stack_menu_text_row(unsigned y, unsigned top, const char *text, unsigned scale,
                                  uint16_t color) {
    if (y < top || y >= top + 5 * scale) return;
    unsigned width = (unsigned)strlen(text) * 4 * scale - scale;
    unsigned left = (SCREEN_WIDTH - width) / 2;
    unsigned shift = 12 - 3 * ((y - top) / scale);
    for (; *text; text++, left += 4 * scale) {
        unsigned pixels = (glyph(*text) >> shift) & 7;
        for (unsigned x = 0; x < 3 * scale; x++)
            if (pixels & (4u >> (x / scale))) row[left + x] = __builtin_bswap16(color);
    }
}

static void m5stack_menu_row(unsigned y, const char *value) {
    unsigned top = SCREEN_HEIGHT / 2 - 34, bottom = SCREEN_HEIGHT / 2 + 34;
    if (y < top || y > bottom) return;
    unsigned margin = SCREEN_WIDTH > 200 ? 32 : 4;
    for (unsigned x = margin; x < SCREEN_WIDTH - margin; x++)
        row[x] = __builtin_bswap16(y == top || y == bottom || x == margin ||
                                   x == SCREEN_WIDTH - margin - 1 ? COLOR_SRDECK_YELLOW_GREEN : 0x0861);
    unsigned scale = SCREEN_WIDTH > 200 ? 4 : 3;
    while ((strlen(value) * 4 - 1) * scale > SCREEN_WIDTH - 2 * margin - 8) scale--;
    m5stack_menu_text_row(y, SCREEN_HEIGHT / 2 - 23, m5stack_menu_names[controls.menu], 2, 0xffff);
    m5stack_menu_text_row(y, SCREEN_HEIGHT / 2 - 1, value, scale, COLOR_SRDECK_YELLOW_GREEN);
}
