"""Run the production keyboard scanner and controls for both Cardputers."""
import unittest

from host_test import COMPILER, ROOT, compile_run

SOURCE = (ROOT / 'main/boards/standalone_display.c').read_text()
KEYS = SOURCE[SOURCE.index('enum {'):SOURCE.index('\n#endif', SOURCE.index('enum {'))] + '\n'
SCANNER = SOURCE[SOURCE.index('static unsigned keyboard_buttons('):
                 SOURCE.index('#if CONFIG_ESP_SDR_M5STACK_CARDPUTER ||',
                              SOURCE.index('static unsigned keyboard_buttons('))]
CONTROLS = SOURCE[SOURCE.index('static unsigned cardputer_actions('):
                  SOURCE.index('\n#endif', SOURCE.index('static unsigned cardputer_actions('))]
PREAMBLE = r'''
#include <assert.h>
#include <string.h>
#include "standalone_display_controls.h"
static m5stack_controls_t controls;
'''


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class CardputerControls(unittest.TestCase):
    def test_debounce_shift_cycles_frequency_entry_and_pc_lease(self):
        check = r'''
static uint32_t now;
static unsigned mhz = 2412;
static void tick(unsigned keys, unsigned elapsed, bool pc) {
    now += elapsed;
    mhz = cardputer_controls_update(mhz, keys, now, pc);
}
static void press(unsigned key) {
    tick(key, 1, false); tick(key, 30, false);
    tick(key, 100, false); /* No repeat while held. */
    tick(0, 1, false); tick(0, 30, false);
}
static void enter(const char *digits) {
    for (; *digits; digits++) press(MATRIX_DIGIT_0 << (*digits - '0'));
    assert(!frequency_input_length && !frequency_input[0]);
}
int main(void) {
    tick(MATRIX_COMMA, 1, false); tick(0, 29, false); tick(0, 30, false);
    assert(mhz == 2412);
    press(MATRIX_COMMA); assert(mhz == 2411);
    press(MATRIX_PERIOD); press(MATRIX_SLASH); assert(mhz == 2413);
    for (unsigned j = 1; j <= 5; j++) {
        press(MATRIX_T); assert(controls.step == j % 5);
    }
    press(MATRIX_SHIFT | MATRIX_T); assert(controls.step == 4);
    press(MATRIX_COMMA); assert(mhz == 2313);
    mhz = 100; press(MATRIX_COMMA); assert(mhz == 100);
    mhz = 6000; press(MATRIX_PERIOD); assert(mhz == 6000);
    const unsigned keys[] = {MATRIX_P, MATRIX_F, MATRIX_A};
    unsigned *settings[] = {&controls.rate, &controls.fft, &controls.average};
    const unsigned counts[] = {6, 5, 5};
    for (unsigned k = 0; k < 3; k++) {
        press(MATRIX_SHIFT | keys[k]); assert(*settings[k] == counts[k] - 1);
        for (unsigned j = 0; j < counts[k]; j++) {
            press(keys[k]); assert(*settings[k] == j);
        }
        press(keys[k]); assert(*settings[k] == 0);
    }
    assert(m5stack_fft_sizes[4] == 4096);
    press(MATRIX_SPACE); assert(keyboard_help_visible);
    press(MATRIX_SPACE); assert(!keyboard_help_visible);
    press(MATRIX_SHIFT | MATRIX_SPACE); assert(controls.density_mode && !keyboard_help_visible);
    press(MATRIX_SHIFT | MATRIX_SPACE); assert(!controls.density_mode && !keyboard_help_visible);
    enter("2412"); assert(mhz == 2412);
    enter("0100"); assert(mhz == 100);
    enter("6000"); assert(mhz == 6000);
    enter("0099"); enter("6001"); enter("9999"); assert(mhz == 6000);
    press(MATRIX_DIGIT_0 << 2); press(MATRIX_DIGIT_0 << 4);
    assert(!strcmp(frequency_input, "24") && mhz == 6000);
    tick(MATRIX_T | MATRIX_COMMA, 1, true);
    tick(MATRIX_T | MATRIX_COMMA, 1000, true);
    tick(MATRIX_T | MATRIX_COMMA, 1000, false);
    assert(mhz == 6000 && controls.step == 4 && !frequency_input_length);
    tick(0, 1, false); tick(0, 30, false);
    press(MATRIX_COMMA); assert(mhz == 5900);
    /* Shift is captured on key-down, even if released during debounce. */
    tick(MATRIX_SHIFT | MATRIX_T, 1, false); tick(MATRIX_T, 30, false);
    assert(controls.step == 3);
    tick(0, 1, false); tick(0, 30, false);
    now = UINT32_MAX - 10;
    tick(MATRIX_PERIOD, 0, false); tick(MATRIX_PERIOD, 30, false);
    assert(mhz == 5920);
    return 0;
}
'''
        for profile in ['CARDPUTER', 'CARDPUTER_ADV']:
            with self.subTest(profile=profile):
                compile_run(PREAMBLE + KEYS + CONTROLS + check,
                            [f'CONFIG_ESP_SDR_M5STACK_{profile}=1'])

    def test_matrix_scan_and_adv_press_release_events(self):
        stub = r'''
typedef unsigned gpio_num_t;
enum {GPIO_NUM_3=3, GPIO_NUM_4=4, GPIO_NUM_5=5, GPIO_NUM_6=6,
      GPIO_NUM_7=7, GPIO_NUM_8=8, GPIO_NUM_9=9, GPIO_NUM_11=11,
      GPIO_NUM_13=13, GPIO_NUM_15=15};
static unsigned address;
static bool down[4][14];
static int gpio_set_level(unsigned pin, unsigned value) {
    unsigned bit = pin == 8 ? 0 : pin == 9 ? 1 : 2;
    address = (address & ~(1u << bit)) | (value << bit); return 0;
}
static int gpio_get_level(unsigned pin) {
    const unsigned pins[] = {13, 15, 3, 4, 5, 6, 7};
    for (unsigned i = 0; i < 7; i++) if (pin == pins[i])
        return !down[address < 4 ? 3-address : 7-address][2*i+(address < 4)];
    assert(0); return 1;
}
static unsigned adv_keyboard_state, events[16], count, cursor;
static unsigned tca8418_read(unsigned reg) {
    if (reg == 3) return count - cursor;
    assert(reg == 4 && cursor < count); return events[cursor++];
}
'''
        check = r'''
int main(void) {
    /* Physical row, column and TCA8418 event code, independent of the scanner. */
    const unsigned positions[][3] = {
        {3,10,54}, {3,11,58}, {3,12,64}, {3,13,68},
        {1,5,26}, {1,10,52}, {2,5,27}, {2,2,13},
        {0,10,51}, {0,1,5}, {0,2,11}, {0,3,15}, {0,4,21},
        {0,5,25}, {0,6,31}, {0,7,35}, {0,8,41}, {0,9,45}, {2,1,7}
    };
    for (unsigned j = 0; j < 19; j++) {
        down[positions[j][0]][positions[j][1]] = true;
        events[0] = positions[j][2]; count = 1; cursor = 0;
        unsigned bit = 1u << (j < 8 ? j : j + 1); /* Bit 8 is unused; digits start at 9. */
        assert(keyboard_buttons() == bit);
        assert(keyboard_buttons() == bit); /* State survives empty FIFO. */
        events[0] |= 0x80; cursor = 0;
        down[positions[j][0]][positions[j][1]] = false;
        assert(keyboard_buttons() == 0);
    }
    down[3][10] = down[2][1] = true;
    events[0] = 54; events[1] = 7; count = 2; cursor = 0;
    assert(keyboard_buttons() == (MATRIX_COMMA | MATRIX_SHIFT));
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
    events[0] = 0; events[1] = 81; events[2] = 80; events[3] = 9;
    count = 4; cursor = 0;
    assert(keyboard_buttons() == (MATRIX_COMMA | MATRIX_SHIFT));
#endif
    return 0;
}
'''
        for profile in ['CARDPUTER', 'CARDPUTER_ADV']:
            with self.subTest(profile=profile):
                compile_run(PREAMBLE + KEYS + stub + SCANNER + check,
                            [f'CONFIG_ESP_SDR_M5STACK_{profile}=1'])
