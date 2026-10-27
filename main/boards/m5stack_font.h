#pragma once

/* Read the bundled M5GFX bitmaps directly without its C++ display driver. */
typedef struct {
    uint16_t bitmapOffset;
    uint8_t width, height, xAdvance;
    int8_t xOffset, yOffset;
} GFXglyph;

typedef struct {
    const uint8_t *bitmap;
    const GFXglyph *glyph;
    uint16_t first, last;
    uint8_t yAdvance;
} GFXfont;

#define PROGMEM
#include "../../components/M5GFX/src/lgfx/Fonts/Custom/DejaVu9.h"
#include "../../components/M5GFX/src/lgfx/Fonts/Custom/DejaVu12.h"
#include "../../components/M5GFX/src/lgfx/Fonts/GFXFF/FreeSansBold9pt7b.h"
#include "../../components/M5GFX/src/lgfx/Fonts/GFXFF/FreeSansBold12pt7b.h"
#undef PROGMEM

static unsigned m5stack_text_width(const char *text, const GFXfont *font) {
    unsigned width = 0;
    /* Tighter tracking keeps long menu values readable on the 135-pixel panel. */
    unsigned tighten = font == &FreeSansBold9pt7b;
    for (; *text; text++)
        if (*text >= font->first && *text <= font->last)
            width += font->glyph[*text - font->first].xAdvance - tighten;
    return width;
}

static void m5stack_text_row(unsigned y, int left, int baseline, const char *text,
                             const GFXfont *font, uint16_t color, unsigned margin) {
    unsigned tighten = font == &FreeSansBold9pt7b;
    for (; *text; text++) {
        if (*text < font->first || *text > font->last) continue;
        const GFXglyph *g = &font->glyph[*text - font->first];
        int source_y = (int)y - baseline - g->yOffset;
        if (source_y >= 0 && source_y < g->height) {
            for (unsigned x = 0; x < g->width; x++) {
                unsigned bit = source_y * g->width + x;
                int target_x = left + g->xOffset + (int)x;
                if (target_x >= (int)margin && target_x < (int)(SCREEN_WIDTH - margin) &&
                    (font->bitmap[g->bitmapOffset + bit / 8] & (0x80u >> (bit % 8))))
                    row[target_x] = __builtin_bswap16(color);
            }
        }
        left += g->xAdvance - tighten;
    }
}

static unsigned text_width(const char *text) {
    return m5stack_text_width(text, &DejaVu12);
}

static void text_row(unsigned y, int left, const char *text, uint16_t color) {
    if (y >= 12) return;
    m5stack_text_row(y, left, 9, text, &DejaVu12, color, 0);
}
