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
static unsigned mhz = 2412;
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
    tick(1, 1, false); tick(0, 10, false); tick(0, 30, false);
    assert(mhz == 2412); /* Contact bounce is not a press. */
    press(1); assert(mhz == 2411);
    press(4); assert(mhz == 2412);
    press(2); assert(s.step == 1);
    press(4); assert(mhz == 2417);
    press(2); assert(s.step == 2);
    press(1); assert(mhz == 2397);
    press(2); assert(s.step == 3);
    press(4); assert(mhz == 2497); /* Fourth tuning step is 100 MHz. */
    press(2); assert(s.step == 0);
    tick(2, 1, false); tick(2, 30, false); tick(2, 600, false);
    assert(s.fft == 1 && s.rate == 0 && s.step == 0);
    tick(2, 1000, false); assert(s.fft == 1);
    tick(0, 1, false); tick(0, 30, false); assert(s.step == 0);
    for (unsigned j = 1; j <= 6; j++) {
        hold(1); assert(s.rate == j % 6 && mhz == 2497);
    }
    for (unsigned j = 1; j <= 5; j++) {
        hold(4); assert(s.average == j % 5 && mhz == 2497);
    }
    for (unsigned j = 2; j <= 6; j++) {
        hold(2); assert(s.fft == j % 6 && s.step == 0);
    }
    assert(m5stack_spans[0] == 2 && m5stack_spans[5] == 80);
    assert(m5stack_rates[3] == 6 && m5stack_rates[4] == 1 && m5stack_rates[5] == 0);
    assert(m5stack_fft_sizes[5] == 8192 && m5stack_average_counts[4] == 16);
    mhz = RX_FREQ_MIN; press(1); assert(mhz == RX_FREQ_MIN);
    mhz = RX_FREQ_MAX; press(4); assert(mhz == RX_FREQ_MAX);
    tick(7, 100, true); tick(7, 1000, true);
    assert(mhz == RX_FREQ_MAX && s.rate == 0 && s.step == 0);
    tick(7, 1000, false); tick(0, 1, false); tick(0, 30, false);
    assert(mhz == RX_FREQ_MAX && s.rate == 0 && s.step == 0);
    /* A button held across a PC lease cannot become a local action. */
    press(1); assert(mhz == RX_FREQ_MAX - 1);
    now = UINT32_MAX - 10;
    tick(4, 0, false); tick(4, 30, false);
    tick(0, 1, false); tick(0, 30, false);
    assert(mhz == RX_FREQ_MAX); /* Debouncing survives timer wraparound. */
    return 0;
}
'''
        compile_run(source)


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class BasicDisplay(unittest.TestCase):
    def test_board_init_and_fft_shifted_scanlines(self):
        production = (ROOT / 'main/boards/m5stack_basic.c').read_text()
        for header in ['driver/gpio.h', 'driver/spi_master.h', 'esp_check.h', 'esp_timer.h',
                       'freertos/FreeRTOS.h', 'freertos/task.h']:
            production = production.replace('#include "' + header + '"', '')
        stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GPIO_MODE_OUTPUT 2
#define GPIO_MODE_INPUT 1
#define GPIO_PULLDOWN_ONLY 2
#define GPIO_FLOATING 0
#define SPI3_HOST 3
#define SPI_DMA_CH_AUTO 1
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
    assert(rows == 240 && m5stack_basic_rate() == 6);
    assert(m5stack_basic_poll(2412, false) == 2412);
    bins[128] = 255; bins[0] = 0; bins[1] = 128; bins[255] = 64;
    m5stack_basic_draw(2412, bins, 256, false, true, 0);
    assert(rows == 480);
    assert(pixels[32 * 320 + 4] == 0x05ff); /* Thin trace starts at the left edge. */
    assert(pixels[50 * 320 + 6] == 0x05ff); /* The steep peak is connected to its neighbor. */
    assert(pixels[64 * 320 + 160] != 0x05ff); /* Center bin is displayed without interpolation. */
    unsigned vertical_grid_lines = 0;
    for (unsigned x = PLOT_LEFT; x < PLOT_LEFT + PLOT_WIDTH; x++)
        if (pixels[33 * 320 + x] == 0x2104) vertical_grid_lines++;
    assert(vertical_grid_lines == 9); /* Eight equal intervals, 2 MHz each at 16 MHz span. */
    for (unsigned x = PLOT_LEFT; x < PLOT_LEFT + PLOT_WIDTH; x++)
        assert(pixels[SPECTRUM_BOTTOM * 320 + x] == 0x4208); /* Visible baseline across the plot. */
    assert(pixels[FREQUENCY_TOP * 320 + 4] == 0xffff); /* Frequency axis between plots. */
    assert(pixels[BUTTON_HINT_TOP * 320 + 13] == 0xb726);
    assert(pixels[BUTTON_HINT_TOP * 320 + 128] == 0xb726);
    assert(pixels[BUTTON_HINT_TOP * 320 + 231] == 0xb726); /* Hints stay over the buttons. */
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
    unsigned latest_x = PLOT_LEFT + ((((latest_peak + 128) & 255) * PLOT_WIDTH + 255) / 256);
    unsigned oldest_x = PLOT_LEFT + ((192 * PLOT_WIDTH + 255) / 256);
    assert(rows == 1200 + WATERFALL_HEIGHT * 240);
    assert(pixels[WATERFALL_TOP * 320 + latest_x] == 0xffff); /* Newest moving peak. */
    assert(pixels[(WATERFALL_TOP + WATERFALL_HEIGHT - 1) * 320 + oldest_x] == 0xffff);
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
