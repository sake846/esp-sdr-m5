"""Check native glyph detail, small text, and scanline clipping."""
import unittest

from host_test import COMPILER, compile_run


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class MenuFont(unittest.TestCase):
    def test_value_glyph_detail_and_bounds(self):
        source = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "m5stack_basic_controls.h"
#define SCREEN_WIDTH TEST_WIDTH
#define SCREEN_HEIGHT 80
#define COLOR_SRDECK_YELLOW_GREEN 0xb726
static m5stack_controls_t controls;
static uint16_t guarded_row[SCREEN_WIDTH+2];
#define row (guarded_row+1)
#include "m5stack_menu.h"

int main(void) {
    const char *values[] = {"0", "100 MHZ", "80 MHZ", "+40 DB", "-40 DB", "200%",
        "1/10X", "8192", "16", "NO RAM", "NORMAL", "DPX", "MANUAL", "SOFTWARE",
        "HARDWARE", "30", "AUTO", "WIDE", "DCAP 3", "ESP SDR M5", "STICKC PLUS2",
        "RIGHT RESET", "DONE", "", "\x01\xff"};
    unsigned margin = SCREEN_WIDTH > 200 ? 32 : 4;
    for (unsigned i=0; i<sizeof(values)/sizeof(*values); i++) {
        assert(m5stack_text_width(values[i], &FreeSansBold9pt7b) <=
               SCREEN_WIDTH-2*margin-4);
        unsigned ink=0, unique=0;
        uint16_t patterns[SCREEN_HEIGHT];
        for (unsigned y=0; y<SCREEN_HEIGHT; y++) {
            for (unsigned x=0; x<SCREEN_WIDTH+2; x++) guarded_row[x]=0x1234;
            m5stack_menu_value_row(y, values[i], margin);
            assert(guarded_row[0]==0x1234 && guarded_row[SCREEN_WIDTH+1]==0x1234);
            for (unsigned x=0; x<SCREEN_WIDTH; x++) {
                if (x<=margin || x>=SCREEN_WIDTH-margin-1) assert(row[x]==0x1234);
                if (row[x]==0x1234) continue;
                assert(row[x]==__builtin_bswap16(COLOR_SRDECK_YELLOW_GREEN));
                assert(y>SCREEN_HEIGHT/2 && y<SCREEN_HEIGHT/2+34);
                ink++;
            }
            if (!i) {
                uint16_t pattern=0;
                for (unsigned x=0; x<16; x++)
                    pattern=(pattern<<1)|(row[SCREEN_WIDTH/2-8+x]!=0x1234);
                unsigned j=0;
                while (j<unique && patterns[j]!=pattern) j++;
                if (j==unique) patterns[unique++]=pattern;
            }
        }
        if (i<sizeof(values)/sizeof(*values)-2) assert(ink>0);
        else assert(ink==0);
        if (!i) assert(unique>6); /* More detail than five enlarged bitmap rows. */
    }
    assert(text_width("III") < text_width("WWW"));
    const char *labels[] = {"CPU 100%", "BAT 100%", "2412", "ESP SDR M5", "WF CONTRAST",
        "CANCEL", "MENU", "RESET", "<> FREQ 0-9 T STEP P SPAN", "Q%+-/"};
    for (unsigned i=0; i<sizeof(labels)/sizeof(*labels); i++) {
        for (int left=-8; left<SCREEN_WIDTH; left+=SCREEN_WIDTH-8) {
            unsigned ink=0;
            for (unsigned y=0; y<14; y++) {
                for (unsigned x=0; x<SCREEN_WIDTH+2; x++) guarded_row[x]=0x1234;
                text_row(y, left, labels[i], 0xffff);
                assert(guarded_row[0]==0x1234 && guarded_row[SCREEN_WIDTH+1]==0x1234);
                for (unsigned x=0; x<SCREEN_WIDTH; x++) {
                    if (row[x]==0x1234) continue;
                    assert(row[x]==0xffff && y<12);
                    ink++;
                }
            }
            assert(ink>0);
        }
    }
    const char *reset_hints[] = {"CANCEL", "MENU", "RESET"};
    unsigned previous_end=0;
    for (unsigned i=0; i<3; i++) {
        unsigned width=m5stack_text_width(reset_hints[i], &DejaVu9);
        unsigned left=(2*i+1)*SCREEN_WIDTH/6-width/2;
        assert(left>=previous_end && left+width<=SCREEN_WIDTH);
        previous_end=left+width;
    }
    if (SCREEN_WIDTH==135 || SCREEN_WIDTH==240) {
        unsigned title_end=12+m5stack_text_width(SCREEN_WIDTH==135 ? "ESP SDR" : "ESP SDR M5", &DejaVu9);
        const char *telemetry[] = {"CPU 100%", "BAT 100%"};
        for (unsigned i=0; i<2; i++) {
            unsigned status_left=SCREEN_WIDTH-3-text_width("WAIT");
            if (SCREEN_WIDTH==240) status_left-=text_width(telemetry[i])+7;
            unsigned gain_width=text_width("127");
            unsigned gain_left=(title_end+status_left)/2-gain_width/2;
            assert(gain_left>=title_end+3 && gain_left+gain_width+3<=status_left);
        }
    }
    if (SCREEN_WIDTH>=160) {
        const GFXfont *font=SCREEN_WIDTH<200 ? &DejaVu9 : &DejaVu12;
        unsigned left=SCREEN_WIDTH-m5stack_text_width("CPU 100%",font)-4;
        unsigned status_left=left-m5stack_text_width("WAIT",font)-7;
        assert(12+m5stack_text_width("ESP SDR",&DejaVu9)+3<=status_left);
    }
    return 0;
}
'''
        for width in [135, 160, 240, 300, 320, 480]:
            with self.subTest(width=width):
                compile_run(source, [f'TEST_WIDTH={width}'])
