/* M5Stack Basic: SPI LCD, three buttons and a muted GPIO25 speaker.
 * Pin map: https://docs.m5stack.com/en/core/Basic
 * LCD registers: https://github.com/m5stack/M5GFX (Panel_ILI9342).
 */
#include "m5stack_basic.h"
#include "m5stack_basic_controls.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define LCD_DC  27
#define LCD_RST 33
#define LCD_BL  32
#define PIN(n)  (UINT64_C(1) << (n))
#define PLOT_LEFT 4
#define PLOT_WIDTH 312
#define SPECTRUM_TOP 32
#define SPECTRUM_BOTTOM 83
#define FREQUENCY_TOP 85
#define WATERFALL_TOP 97
#define WATERFALL_HEIGHT 120
#define BUTTON_HINT_TOP 223
static spi_device_handle_t lcd;
static m5stack_controls_t controls;
/* A single scanline avoids a 150 KiB framebuffer on the non-PSRAM Basic. */
static uint16_t row[320] __attribute__((aligned(4)));
/* Keep about 12 seconds of 10 Hz history using the byte-sized power codes. */
static uint8_t waterfall[WATERFALL_HEIGHT][PLOT_WIDTH];
static unsigned waterfall_head, waterfall_count, waterfall_mhz, waterfall_span, waterfall_fft;
static bool waterfall_tuned;

static void lcd_write(bool data, const void *bytes, size_t size) {
    ESP_ERROR_CHECK(gpio_set_level(LCD_DC, data));
    spi_transaction_t transaction = {.length = size * 8, .tx_buffer = bytes};
    ESP_ERROR_CHECK(spi_device_polling_transmit(lcd, &transaction));
}

static void lcd_command(uint8_t command, const uint8_t *data, size_t size) {
    lcd_write(false, &command, 1);
    if (size) lcd_write(true, data, size);
}

void m5stack_basic_init(void) {
    const gpio_config_t outputs = {
        .pin_bit_mask = PIN(4) | PIN(25) | PIN(LCD_DC) | PIN(LCD_RST) | PIN(LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_set_level(4, 1)); /* Deselect the SD card on the shared SPI bus. */
    ESP_ERROR_CHECK(gpio_set_level(25, 0));
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 0));
    ESP_ERROR_CHECK(gpio_config(&outputs));
    const gpio_config_t buttons = {
        .pin_bit_mask = PIN(37) | PIN(38) | PIN(39),
        .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&buttons));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    /* Basic revisions use the reset pad's external pull-up to select inversion. */
    ESP_ERROR_CHECK(gpio_set_direction(LCD_RST, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(LCD_RST, GPIO_PULLDOWN_ONLY));
    vTaskDelay(pdMS_TO_TICKS(1));
    bool invert = gpio_get_level(LCD_RST);
    ESP_ERROR_CHECK(gpio_set_pull_mode(LCD_RST, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RST, 1));
    ESP_ERROR_CHECK(gpio_set_direction(LCD_RST, GPIO_MODE_OUTPUT));
    vTaskDelay(pdMS_TO_TICKS(120));

    const spi_bus_config_t bus = {
        .mosi_io_num = 23,
        .miso_io_num = -1,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(row),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO));
    const spi_device_interface_config_t device = {
        .clock_speed_hz = 40000000,
        .mode = 0,
        .spics_io_num = 14,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &device, &lcd));
    /* ILI9342C is native landscape; early ILI9341 Basics need exchanged axes. */
#if CONFIG_ESP_SDR_M5STACK_LCD_ILI9341
    lcd_command(0xc0, (const uint8_t[]){0x23}, 1);
    lcd_command(0xc1, (const uint8_t[]){0x10}, 1);
    lcd_command(0xc5, (const uint8_t[]){0x3e, 0x28}, 2);
    lcd_command(0x36, (const uint8_t[]){0xa8}, 1);
#else
    lcd_command(0xc8, (const uint8_t[]){0xff, 0x93, 0x42}, 3);
    lcd_command(0xc0, (const uint8_t[]){0x12, 0x12}, 2);
    lcd_command(0xc1, (const uint8_t[]){0x03}, 1);
    lcd_command(0xc5, (const uint8_t[]){0xf2}, 1);
    lcd_command(0xb0, (const uint8_t[]){0xe0}, 1);
    lcd_command(0xf6, (const uint8_t[]){0x01, 0x00, 0x00}, 3);
    lcd_command(0xb6, (const uint8_t[]){0x08, 0x82, 0x1d, 0x04}, 4);
    lcd_command(0x36, (const uint8_t[]){0x08}, 1);
#endif
    lcd_command(0x3a, (const uint8_t[]){0x55}, 1);
    lcd_command(invert ? 0x21 : 0x20, NULL, 0);
    lcd_command(0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    m5stack_basic_draw(2412, NULL, 256, false, false, 0);
    lcd_command(0x29, NULL, 0);
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 1));
}

unsigned m5stack_basic_poll(unsigned mhz, bool pc_control) {
    unsigned pressed = (!gpio_get_level(39)) | ((!gpio_get_level(38)) << 1) | ((!gpio_get_level(37)) << 2);
    return m5stack_controls_update(&controls, mhz, pressed, (uint32_t)(esp_timer_get_time() / 1000),
                                   pc_control);
}

unsigned m5stack_basic_rate(void) {
    return m5stack_rates[controls.rate];
}

unsigned m5stack_basic_fft_points(void) {
    return m5stack_fft_sizes[controls.fft];
}

/* Original 3x5 glyphs, each row encoded in three bits, top row first. */
static uint16_t glyph(char c) {
    static const uint16_t digits[] = {
        0x7b6f, 0x2492, 0x73e7, 0x73cf, 0x5bc9, 0x79cf, 0x79ef, 0x7249, 0x7bef, 0x7bcf,
    };
    static const uint16_t letters[] = {
        0x2bed, 0x6bae, 0x7927, 0x6b6e, 0x79a7, 0x79a4, 0x796f, 0x5bed, 0x7497,
        0x124e, 0x5bad, 0x4927, 0x5fed, 0x5ffd, 0x7b6f, 0x7be4, 0x7b7b, 0x7bed,
        0x79cf, 0x7492, 0x5b6f, 0x5b6a, 0x5bfd, 0x5aad, 0x5a92, 0x72a7,
    };
    if (c >= '0' && c <= '9') return digits[c - '0'];
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c == '%') return 0x52a5;
    if (c == '-') return 0x01c0;
    if (c == '+') return 0x05d0;
    if (c == '/') return 0x12a4;
    return 0;
}

static void text_row(unsigned y, unsigned left, unsigned top, const char *text, unsigned scale,
                     uint16_t color) {
    if (y < top || y >= top + 5 * scale) return;
    unsigned shift = 12 - 3 * ((y - top) / scale);
    for (; *text && left + 3 * scale <= 320; text++, left += 4 * scale) {
        unsigned pixels = (glyph(*text) >> shift) & 7;
        for (unsigned x = 0; x < 3 * scale; x++)
            if (pixels & (4u >> (x / scale))) row[left + x] = __builtin_bswap16(color);
    }
}

static uint16_t waterfall_color(uint8_t value) {
    unsigned red, green, blue;
    if (value < 64) {
        red = green = 0;
        blue = value * 4;
    } else if (value < 128) {
        red = 0;
        green = (value - 64) * 4;
        blue = 255;
    } else if (value < 192) {
        red = (value - 128) * 4;
        green = 255;
        blue = 255 - (value - 128) * 4;
    } else {
        red = 255;
        green = blue = (value - 192) * 4;
    }
    return (uint16_t)(((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3));
}

void m5stack_basic_draw(unsigned mhz, const uint8_t *bins, unsigned fft_points, bool pc_control,
                        bool valid, unsigned cpu_percent) {
    char settings[40], edges[3][12];
    uint8_t display_bins[PLOT_WIDTH] = {0};
    uint8_t spectrum_trace[PLOT_WIDTH];
    unsigned span = m5stack_spans[controls.rate];
    if (!waterfall_tuned || waterfall_mhz != mhz || waterfall_span != span || waterfall_fft != fft_points) {
        waterfall_mhz = mhz;
        waterfall_span = span;
        waterfall_fft = fft_points;
        waterfall_head = waterfall_count = 0;
        waterfall_tuned = true;
    }
    snprintf(settings, sizeof(settings), "STEP %u SPAN %u FFT %u", m5stack_steps[controls.step], span,
             fft_points);
    snprintf(edges[0], sizeof(edges[0]), "%u", mhz - span / 2);
    snprintf(edges[1], sizeof(edges[1]), "%u", mhz);
    snprintf(edges[2], sizeof(edges[2]), "%u", mhz + span / 2);
    if (bins && valid) {
        for (unsigned plot_x = 0; plot_x < PLOT_WIDTH; plot_x++) {
            unsigned first = plot_x * fft_points / PLOT_WIDTH;
            unsigned limit = (plot_x + 1) * fft_points / PLOT_WIDTH;
            if (limit <= first) limit = first + 1;
            if (limit > fft_points) limit = fft_points;
            for (unsigned shifted = first; shifted < limit; shifted++) {
                unsigned bin = (shifted + fft_points / 2) % fft_points;
                if (bins[bin] > display_bins[plot_x]) display_bins[plot_x] = bins[bin];
            }
            unsigned height = (display_bins[plot_x] * (SPECTRUM_BOTTOM - SPECTRUM_TOP)) / 255;
            spectrum_trace[plot_x] = SPECTRUM_BOTTOM - height;
        }
    }
    if (bins && valid && !pc_control) {
        waterfall_head = (waterfall_head + 1) % WATERFALL_HEIGHT;
        memcpy(waterfall[waterfall_head], display_bins, sizeof(display_bins));
        if (waterfall_count < WATERFALL_HEIGHT) waterfall_count++;
    }
    for (unsigned y = 0; y < 240; y++) {
        for (unsigned x = 0; x < 320; x++) {
            uint16_t color = 0x0000;
            if (x >= PLOT_LEFT && x < PLOT_LEFT + PLOT_WIDTH) {
                unsigned plot_x = x - PLOT_LEFT;
                if (y >= SPECTRUM_TOP && y <= SPECTRUM_BOTTOM) {
                    if (y == SPECTRUM_BOTTOM)
                        color = 0x4208;
                    else if ((y - SPECTRUM_TOP) % 13 == 0 || plot_x % (PLOT_WIDTH / 8) == 0 ||
                             plot_x == PLOT_WIDTH - 1)
                        color = 0x2104;
                    if (bins && valid && y != SPECTRUM_BOTTOM) {
                        unsigned trace_y = spectrum_trace[plot_x];
                        unsigned previous_y = plot_x ? spectrum_trace[plot_x - 1] : trace_y;
                        if (y >= (trace_y < previous_y ? trace_y : previous_y) &&
                            y <= (trace_y > previous_y ? trace_y : previous_y))
                            color = 0x05ff;
                    }
                } else if (y >= WATERFALL_TOP && y < WATERFALL_TOP + WATERFALL_HEIGHT) {
                    unsigned age = y - WATERFALL_TOP;
                    if (age < waterfall_count) {
                        unsigned index = (waterfall_head + WATERFALL_HEIGHT - age) % WATERFALL_HEIGHT;
                        color = waterfall_color(waterfall[index][plot_x]);
                    }
                }
            }
            row[x] = __builtin_bswap16(color);
        }
        text_row(y, 4, 3, "ESP-SDR M5 BASIC", 2, 0x07ff);
        text_row(y, 4, 21, settings, 2, 0xffff);
        char cpu[12];
        snprintf(cpu, sizeof(cpu), "CPU %u%%", cpu_percent);
        text_row(y, 208, 23, cpu, 1, 0x07ff);
        text_row(y, 270, 23, pc_control ? "PC" : valid ? "LIVE" : "WAIT", 1,
                 pc_control ? 0xffe0 : 0xffff);
        text_row(y, 4, FREQUENCY_TOP, edges[0], 2, 0xffff);
        text_row(y, 144, FREQUENCY_TOP, edges[1], 2, 0xffff);
        text_row(y, 284, FREQUENCY_TOP, edges[2], 2, 0xffff);
        text_row(y, 29, BUTTON_HINT_TOP, "TUNE-", 2, 0x07ff);
        text_row(y, 120, BUTTON_HINT_TOP, "STEP/SPAN", 2, 0x07ff);
        text_row(y, 231, BUTTON_HINT_TOP, "TUNE+/FFT+", 2, 0x07ff);
        lcd_command(0x2a, (const uint8_t[]){0, 0, 1, 0x3f}, 4);
        const uint8_t address[] = {0, y, 0, y};
        lcd_command(0x2b, address, 4);
        lcd_command(0x2c, NULL, 0);
        lcd_write(true, row, sizeof(row));
    }
}
