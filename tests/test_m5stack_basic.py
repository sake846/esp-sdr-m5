"""Exercise the actual Basic button state machine with a native C compiler."""
import unittest

from host_test import COMPILER, ROOT, compile_run


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class BasicControls(unittest.TestCase):
    def test_debounce_short_long_presses_limits_and_pc_lease(self):
        source = r'''
#include <assert.h>
#include "m5stack_basic_controls.h"
static m5stack_controls_t s;
static unsigned mhz = 2440;
static uint32_t now;
static void tick(unsigned keys, unsigned elapsed, bool pc) {
    now += elapsed;
    mhz = m5stack_controls_update(&s, mhz, keys, now, pc);
}
static void press(unsigned key) {
    tick(key, 1, false); tick(key, 30, false);
    tick(0, 1, false); tick(0, 30, false);
}
static void hold(unsigned key) {
    tick(key, 1, false); tick(key, 30, false);
    tick(key, 599, false); tick(key, 1, false);
    tick(key, 1000, false); /* Holding longer must not repeat. */
    tick(0, 1, false); tick(0, 30, false);
}
int main(void) {
    m5stack_controls_defaults(&s);
    assert(m5stack_steps[s.step] == 10 && m5stack_spans[s.rate] == 80);
    assert(sizeof(m5stack_fft_sizes) / sizeof(m5stack_fft_sizes[0]) == 5);
    assert(m5stack_fft_sizes[4] == 4096);
    s.gain_max = 72;
    tick(1, 1, false); tick(0, 10, false); tick(0, 30, false);
    assert(mhz == 2440 && s.menu == M5_MENU_NONE);
    press(1); assert(mhz == 2430);
    press(4); assert(mhz == 2440);
    press(2); assert(s.menu == M5_MENU_STEP && s.step == 2);
    press(4); assert(s.step == 3 && mhz == 2440);
    press(1); assert(s.step == 2);
    press(1); assert(s.step == 1);
    press(1); assert(s.step == 0);
    press(1); assert(s.step == 0); /* Values stop at the lower bound. */
    const m5stack_menu_t order[] = {M5_MENU_SPAN, M5_MENU_Y_POS, M5_MENU_WF_BRIGHT,
        M5_MENU_WF_CONTRAST, M5_MENU_WF_SPEED, M5_MENU_FFT, M5_MENU_AVERAGE,
        M5_MENU_SPECTRUM, M5_MENU_AGC, M5_MENU_GAIN, M5_MENU_FILTER, M5_MENU_ABOUT, M5_MENU_RESET, M5_MENU_STEP};
    for (unsigned j = 0; j < sizeof(order)/sizeof(order[0]); j++) {
        press(2); assert(s.menu == order[j]);
    }
    hold(2); assert(s.menu == M5_MENU_RESET); /* One reverse step, no release action. */
    hold(2); assert(s.menu == M5_MENU_ABOUT);
    press(4); assert(s.about == 1); press(1); assert(s.about == 0);
    s.menu = M5_MENU_SPAN; press(4); assert(s.rate == 5); press(1); assert(s.rate == 4);
    s.menu = M5_MENU_Y_POS; press(4); assert(s.y_pos == 5); press(1); assert(s.y_pos == 0);
    s.menu = M5_MENU_WF_BRIGHT; press(1); assert(s.wf_bright == -5);
    s.menu = M5_MENU_WF_CONTRAST; press(4); assert(s.wf_contrast == 125);
    s.menu = M5_MENU_WF_SPEED; press(1); assert(s.wf_speed == 2);
    s.menu = M5_MENU_FFT; press(4); assert(s.fft == 3);
    press(4); press(4); assert(s.fft == 4 && m5stack_fft_sizes[s.fft] == 4096);
    s.menu = M5_MENU_AVERAGE; press(1); assert(s.average == 3);
    s.menu = M5_MENU_SPECTRUM; press(4); assert(s.dpx); press(1); assert(!s.dpx);
    s.menu = M5_MENU_AGC; press(4); assert(s.agc == M5_AGC_HARDWARE);
    press(4); assert(s.agc == M5_AGC_MANUAL); press(1); assert(s.agc == M5_AGC_HARDWARE);
    unsigned agc = M5_AGC_SOFTWARE, gain = 35; int filter = 80; bool follow = true;
    assert(m5stack_controls_sync_rx(&s, &agc, &gain, 72, &filter, &follow));
    assert(agc == M5_AGC_HARDWARE && gain == 35 && follow && s.filter == -1);
    s.menu = M5_MENU_GAIN; press(4);
    assert(m5stack_controls_sync_rx(&s, &agc, &gain, 72, &filter, &follow));
    assert(agc == M5_AGC_MANUAL && gain == 36);
    s.gain = 72; press(4); assert(s.gain == 72);
    s.gain = 0; press(1); assert(s.gain == 0);
    s.menu = M5_MENU_FILTER; press(4); assert(s.filter == 0);
    assert(m5stack_controls_sync_rx(&s, &agc, &gain, 72, &filter, &follow));
    assert(filter == 0 && !follow);
    press(1); assert(s.filter == -1);
    m5stack_controls_sync_rx(&s, &agc, &gain, 72, &filter, &follow); assert(follow);
    gain = 42; agc = M5_AGC_SOFTWARE; filter = 73; follow = false;
    assert(!m5stack_controls_sync_rx(&s, &agc, &gain, 72, &filter, &follow));
    assert(s.gain == 42 && s.agc == agc && s.filter == 73); /* PC state remains authoritative. */
    s.menu = M5_MENU_RESET; press(4);
    assert(s.reset_done && mhz == 2440 && s.menu == M5_MENU_RESET);
    assert(s.rate == 5 && s.step == 2 && s.fft == 2 && s.average == 4);
    assert(s.y_pos == 0 && s.wf_bright == 0 && s.wf_contrast == 100 && s.wf_speed == 3);
    assert(!s.dpx && !s.dpx_failed);
    m5stack_controls_sync_rx(&s, &agc, &gain, 20, &filter, &follow);
    assert(agc == M5_AGC_SOFTWARE && gain == 20 && follow); /* Reset respects calibrated bounds. */
    press(1); assert(s.menu == M5_MENU_NONE);
    hold(1); assert(mhz == 2430 && s.rate == 5); /* Side holds tune once on release. */
    hold(4); assert(mhz == 2440 && s.average == 4);
    press(2); tick(0, 4999, false); assert(s.menu == M5_MENU_STEP);
    tick(0, 1, false); assert(s.menu == M5_MENU_NONE);
    press(2); tick(4, 4999, false); tick(4, 30, false);
    tick(4, 6000, false); assert(s.menu == M5_MENU_STEP); /* Held input prevents timeout. */
    tick(0, 1, false); tick(0, 30, false); assert(s.step == 3);
    tick(0, 4999, false); assert(s.menu == M5_MENU_STEP);
    tick(0, 1, false); assert(s.menu == M5_MENU_NONE);
    press(2); unsigned old_step = s.step;
    tick(4, 5000, false); tick(4, 30, false);
    tick(0, 1, false); tick(0, 30, false);
    assert(s.menu == M5_MENU_NONE && s.step == old_step && mhz == 2460);
    /* A new input after a polling gap cannot keep an expired menu alive. */
    mhz = RX_FREQ_MIN; press(1); assert(mhz == RX_FREQ_MIN);
    mhz = RX_FREQ_MAX; press(4); assert(mhz == RX_FREQ_MAX);
    press(2); tick(7, 100, true); tick(7, 1000, true);
    assert(s.menu == M5_MENU_NONE);
    tick(7, 1000, false); tick(0, 1, false); tick(0, 30, false);
    assert(mhz == RX_FREQ_MAX && s.menu == M5_MENU_NONE && s.step == 3);
    now = UINT32_MAX - 10;
    press(2); assert(s.menu == M5_MENU_STEP); /* Debounce crosses timer wrap. */
    now = UINT32_MAX - 100; s.last_input = now;
    press(4); tick(0, 4999, false); assert(s.menu == M5_MENU_STEP);
    tick(0, 1, false); assert(s.menu == M5_MENU_NONE); /* Timeout also crosses wrap. */
    return 0;
}
'''
        compile_run(source)


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class BasicDisplay(unittest.TestCase):
    def test_board_init_and_fft_shifted_scanlines(self):
        production = (ROOT / 'main/boards/m5stack_basic.c').read_text()
        for header in ['driver/gpio.h', 'driver/spi_master.h', 'esp_check.h', 'esp_timer.h', 'esp_heap_caps.h',
                       'freertos/FreeRTOS.h', 'freertos/task.h']:
            production = production.replace('#include "' + header + '"', '')
        stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
int board_battery_percent(void) { return -1; }
#include <stdlib.h>
#include <string.h>
#define GPIO_MODE_OUTPUT 2
#define GPIO_MODE_INPUT 1
#define GPIO_PULLDOWN_ONLY 2
#define GPIO_FLOATING 0
#define SPI3_HOST 3
#define SPI_DMA_CH_AUTO 1
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
static void *heap_caps_calloc(size_t n, size_t size, unsigned caps) { assert(caps == 3); return calloc(n, size); }
#define ESP_ERROR_CHECK(x) assert((x) == 0)
#define pdMS_TO_TICKS(x) (x)
typedef void *spi_device_handle_t;
typedef struct { uint64_t pin_bit_mask; int mode; } gpio_config_t;
typedef struct { int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num;
                 size_t max_transfer_sz; } spi_bus_config_t;
typedef struct { int clock_speed_hz, mode, spics_io_num, queue_size; } spi_device_interface_config_t;
typedef struct { size_t length; const void *tx_buffer; } spi_transaction_t;
static unsigned levels[40], dc, command, line, rows;
static uint64_t inputs;
static uint16_t pixels[320 * 240];
static int gpio_set_level(unsigned pin, unsigned value) { levels[pin] = value; if (pin == 27) dc = value; return 0; }
static int gpio_config(const gpio_config_t *c) { if (c->mode == GPIO_MODE_INPUT) inputs = c->pin_bit_mask; return 0; }
static int gpio_set_direction(unsigned pin, unsigned mode) { (void)pin; (void)mode; return 0; }
static int gpio_set_pull_mode(unsigned pin, unsigned pull) { assert(pin == 33); (void)pull; return 0; }
static int gpio_get_level(unsigned pin) { (void)pin; return 1; }
static void vTaskDelay(unsigned ticks) { (void)ticks; }
static int64_t esp_timer_get_time(void) { return 1000000; }
static int spi_bus_initialize(unsigned host, const spi_bus_config_t *b, unsigned dma) {
    assert(host == SPI3_HOST && dma == SPI_DMA_CH_AUTO);
    assert(b->mosi_io_num == 23 && b->sclk_io_num == 18 && b->max_transfer_sz >= 640);
    return 0;
}
static int spi_bus_add_device(unsigned host, const spi_device_interface_config_t *d, spi_device_handle_t *h) {
    assert(host == SPI3_HOST && d->spics_io_num == 14); *h = (void *)1; return 0;
}
static int spi_device_polling_transmit(spi_device_handle_t h, const spi_transaction_t *t) {
    const uint8_t *data = t->tx_buffer;
    assert(h && t->length <= 640 * 8);
    if (!dc) { assert(t->length == 8); command = data[0]; }
    else if (command == 0x2b) {
        assert(t->length == 32 && data[0] == 0 && data[2] == 0 && data[1] == data[3]);
        line = data[1]; assert(line < 240);
    } else if (command == 0x2c) {
        assert(t->length == 640 * 8);
        for (unsigned x = 0; x < 320; x++) pixels[line * 320 + x] = (uint16_t)((data[2*x] << 8) | data[2*x+1]);
        rows++;
    }
    return 0;
}
'''
        check = r'''
int main(void) {
    uint8_t bins[256] = {0};
    m5stack_basic_init();
    assert(levels[4] == 1 && levels[25] == 0 && levels[32] == 1 && levels[33] == 1);
    assert(inputs == ((UINT64_C(1) << 37) | (UINT64_C(1) << 38) | (UINT64_C(1) << 39)));
    assert(rows == 240 && m5stack_basic_rate() == 0 && m5stack_basic_span() == 80);
    assert(m5stack_basic_poll(2412, false) == 2412);
    bins[127] = 255; bins[255] = 0; bins[254] = 128; bins[0] = 64;
    m5stack_basic_draw(2412, bins, 256, false, true, 0);
    assert(rows == 480);
    assert(pixels[SPECTRUM_TOP * 320 + 4] == 0x2cbf); /* Thin trace starts at the left edge. */
    assert(pixels[50 * 320 + 6] == 0x2cbf); /* The steep peak is connected to its neighbor. */
    assert(pixels[33 * 320 + 4] == 0x11ae); /* Grid remains visible through the blue fill. */
    assert(pixels[33 * 320 + 5] == 0x012c); /* Same alpha fill over the black background. */
    assert(pixels[64 * 320 + 164] != 0x2cbf); /* The floor beyond the adjoining peak remains unfilled. */
    unsigned vertical_grid_lines = 0;
    for (unsigned x = PLOT_LEFT; x < PLOT_LEFT + PLOT_WIDTH; x++)
        if (pixels[33 * 320 + x] == 0x2104 || pixels[33 * 320 + x] == 0x11ae) vertical_grid_lines++;
    assert(vertical_grid_lines == 9); /* Eight equal intervals, 10 MHz each at 80 MHz span. */
    for (unsigned x = PLOT_LEFT; x < PLOT_LEFT + PLOT_WIDTH; x++)
        assert(pixels[SPECTRUM_BOTTOM * 320 + x] == 0x4208); /* Visible baseline across the plot. */
    for (unsigned column=0; column<3; column++) {
        unsigned frequency_ink=0, hint_ink=0;
        for (unsigned y=0; y<12; y++)
            for (unsigned x=column*320/3; x<(column+1)*320/3; x++) {
                frequency_ink+=pixels[(FREQUENCY_TOP+y)*320+x]==0xffff;
                hint_ink+=pixels[(BUTTON_HINT_TOP+y)*320+x]==COLOR_SRDECK_YELLOW_GREEN;
            }
        assert(frequency_ink>20); /* All three frequency labels remain between plots. */
        assert(hint_ink>20); /* Hints stay over their respective buttons. */
    }
    assert(pixels[97 * 320 + 4] == 0xffff); /* Newest waterfall row. */
    assert(pixels[97 * 320 + 160] == waterfall_color(0));
    memset(bins, 0, sizeof(bins));
    m5stack_basic_draw(2412, bins, 256, false, true, 0);
    assert(rows == 720);
    assert(pixels[97 * 320 + 4] == waterfall_color(0)); /* The tracked noise floor is black. */
    assert(pixels[98 * 320 + 4] == 0xffff); /* Previous capture moved down. */
    m5stack_basic_draw(2412, bins, 256, true, false, 0);
    assert(rows == 960 && pixels[97 * 320 + 4] == waterfall_color(0));
    assert(pixels[98 * 320 + 4] == 0xffff); /* PC lease freezes local history. */
    m5stack_basic_draw(2413, bins, 256, false, true, 0);
    assert(rows == 1200 && pixels[98 * 320 + 4] == 0x0000); /* Retune clears stale history. */
    for (unsigned frame = 0; frame < WATERFALL_HEIGHT; frame++) {
        unsigned peak = (64 + frame * 4) & 255;
        for (unsigned k = 0; k < 256; k++) {
            unsigned distance = (k + 256 - peak) & 255;
            if (distance > 128) distance = 256 - distance;
            bins[k] = distance < 28 ? (uint8_t)(255 - distance * 7) : 24;
        }
        m5stack_basic_draw(2413, bins, 256, false, true, 0);
    }
    unsigned latest_peak = (64 + (WATERFALL_HEIGHT - 1) * 4) & 255;
    unsigned latest_x = PLOT_LEFT + ((((127 - latest_peak) & 255) * PLOT_WIDTH + 255) / 256);
    unsigned oldest_x = PLOT_LEFT + ((63 * PLOT_WIDTH + 255) / 256);
    assert(rows == 1200 + WATERFALL_HEIGHT * 240);
    assert(pixels[WATERFALL_TOP * 320 + latest_x] == 0xffff); /* Newest moving peak. */
    assert(pixels[(WATERFALL_TOP + WATERFALL_HEIGHT - 1) * 320 + oldest_x] == 0xffff);
    memset(bins, 0, sizeof(bins));
    controls.y_pos = 40;
    m5stack_basic_draw(2413, bins, 256, false, true, 0);
    bool raised = false;
    for (unsigned y = SPECTRUM_TOP; y < SPECTRUM_BOTTOM; y++)
        if (pixels[y * 320 + PLOT_LEFT] == 0x2cbf) raised = true;
    assert(raised); /* A positive Y POS lifts the flat trace off the baseline. */
    controls.y_pos = 0;
    controls.wf_bright = 5; assert(waterfall_color(0) != 0);
    controls.wf_bright = 0; controls.wf_contrast = 200;
    assert(waterfall_color(20) == 0xffe0);
    controls.wf_contrast = 100;
    waterfall_frame = 0; controls.wf_speed = 0;
    unsigned before_head = waterfall_head;
    for (unsigned frame = 0; frame < 10; frame++) m5stack_basic_draw(2413, bins, 256, false, true, 0);
    assert(waterfall_head == (before_head + 1) % WATERFALL_HEIGHT);
    for (controls.menu = M5_MENU_STEP; controls.menu <= M5_MENU_RESET; controls.menu++) {
        m5stack_basic_draw(2413, bins, 256, false, false, 0);
        assert(pixels[86 * 320 + 32] == COLOR_SRDECK_YELLOW_GREEN);
        assert(pixels[120 * 320 + 33] == 0x0861);
    }
    controls.menu = M5_MENU_WF_BRIGHT;
    controls.wf_bright = 10;
    m5stack_basic_draw(2413, bins, 256, false, true, 0);
    const char *preview = getenv("M5STACK_PREVIEW");
    if (preview) {
        FILE *f = fopen(preview, "wb");
        assert(f); fprintf(f, "P6\n320 240\n255\n");
        for (unsigned p = 0; p < 320 * 240; p++) {
            unsigned c = pixels[p];
            fputc(((c >> 11) & 31) * 255 / 31, f);
            fputc(((c >> 5) & 63) * 255 / 63, f);
            fputc((c & 31) * 255 / 31, f);
        }
        fclose(f);
    }
    return 0;
}
'''
        compile_run(stub + production + check)
