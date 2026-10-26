/* M5Stack Basic: SPI LCD, three buttons and a muted GPIO25 speaker.
 * Pin map: https://docs.m5stack.com/en/core/Basic
 * LCD registers: https://github.com/m5stack/M5GFX (Panel_ILI9342).
 */
#include "m5stack_basic.h"
#include "m5stack_basic_controls.h"
#include "spectrum_math.h"
#include "driver/gpio.h"
#if CONFIG_ESP_SDR_M5STACK_CORE2
#include "driver/i2c_master.h"
#endif
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#if CONFIG_ESP_SDR_M5STACK_CORE2
#define LCD_DC 15
#define LCD_CS 5
#define LCD_HOST SPI3_HOST /* Core2 LCD uses VSPI pins GPIO18/23/5. */
#else
#define LCD_DC 27
#define LCD_RST 33
#define LCD_BL 32
#define LCD_CS 14
#define LCD_HOST SPI3_HOST
#endif
#define PIN(n)  (UINT64_C(1) << (n))
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define PLOT_LEFT 4
#define PLOT_WIDTH 312
#define SPECTRUM_TOP 21
#define SPECTRUM_BOTTOM 83
#define FREQUENCY_TOP 85
#define WATERFALL_TOP 97
#define WATERFALL_HEIGHT 120
#define BUTTON_HINT_TOP 223
#define COLOR_SRDECK_YELLOW_GREEN 0xb726 /* RGB565 equivalent of SRdeck #B4E632. */
static spi_device_handle_t lcd;
static m5stack_controls_t controls;
/* A single scanline avoids a 150 KiB framebuffer on the non-PSRAM Basic. */
static uint16_t row[320] __attribute__((aligned(4)));
/* Keep about 12 seconds of 10 Hz history using signed integer dBFS levels. */
static spectrum_dbfs_t waterfall[WATERFALL_HEIGHT][PLOT_WIDTH];
static spectrum_dbfs_t waterfall_floor[WATERFALL_HEIGHT];
static unsigned waterfall_head, waterfall_count, waterfall_mhz, waterfall_span, waterfall_fft;
static unsigned waterfall_frame;
static unsigned display_gain_index;
static bool waterfall_tuned;
static spectrum_noise_floor_t noise_floor_avg;

#include "m5stack_density.h"

#if CONFIG_ESP_SDR_M5STACK_CORE2
static i2c_master_dev_handle_t axp, touch;
static bool axp2101, ili9342e;

static uint8_t pmu_read(uint8_t reg) {
    uint8_t value = 0;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(axp, &reg, 1, &value, 1, 100));
    return value;
}

static void pmu_write(uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_transmit(axp, data, sizeof(data), 100));
}

static void pmu_bit(uint8_t reg, uint8_t bit, bool enabled) {
    uint8_t value = pmu_read(reg);
    pmu_write(reg, enabled ? value | bit : value & ~bit);
}

static void core2_power_init(void) {
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_1, .sda_io_num = 21, .scl_io_num = 22,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));
    const i2c_device_config_t device = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                        .device_address = 0x34, .scl_speed_hz = 400000};
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &device, &axp));
    uint8_t id = pmu_read(0x03);
    axp2101 = id == 0x4a;
    ESP_ERROR_CHECK(id == 0x03 || axp2101 ? ESP_OK : ESP_ERR_NOT_FOUND);

    if (axp2101) {
        pmu_write(0x23, pmu_read(0x23) & 0xe0);
        pmu_write(0x90, (pmu_read(0x90) & 0x7b) | 0x08); /* ALDO4 powers the LCD and touch. */
        pmu_write(0x80, 0x05);
        pmu_write(0x82, 0x12);
        pmu_write(0x84, 0x6a);
        vTaskDelay(pdMS_TO_TICKS(5));
        pmu_bit(0x90, 0x02, true); /* ALDO2 releases the shared LCD/touch reset. */
        pmu_write(0x96, 28);
        pmu_bit(0x90, 0x10, true); /* BLDO1 backlight. */
    } else {
        pmu_write(0x95, (pmu_read(0x95) & 0x72) | 0x84); /* AXP192 GPIO4 output. */
        pmu_write(0x28, (pmu_read(0x28) & 0x0f) | 0xf0); /* LCD/touch 3.3 V. */
        pmu_bit(0x12, 0x04, true); /* LDO2. */
        pmu_write(0x96, pmu_read(0x96) & ~0x02);
        vTaskDelay(pdMS_TO_TICKS(5));
        pmu_bit(0x96, 0x02, true); /* GPIO4 releases the shared reset. */
        pmu_write(0x27, (pmu_read(0x27) & 0x80) | 103);
        pmu_bit(0x12, 0x02, true); /* DC3 backlight. */
    }

    const i2c_device_config_t touch_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                              .device_address = 0x38, .scl_speed_hz = 400000};
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &touch_config, &touch));
    for (unsigned retry = 0; retry < 5; retry++) {
        const uint8_t work_mode[] = {0x00, 0x00};
        uint8_t reg = 0xa6, firmware = 0, vendor = 0;
        if (i2c_master_transmit(touch, work_mode, sizeof(work_mode), 10) == ESP_OK &&
            i2c_master_transmit_receive(touch, &reg, 1, &firmware, 1, 10) == ESP_OK) {
            reg = 0xa8;
            if (i2c_master_transmit_receive(touch, &reg, 1, &vendor, 1, 10) == ESP_OK &&
                vendor == 0x11 && (firmware == 0x10 || firmware == 0x12)) {
                ili9342e = firmware == 0x12;
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static unsigned core2_touch_buttons(void) {
    uint8_t reg = 0x02, count = 0;
    if (i2c_master_transmit_receive(touch, &reg, 1, &count, 1, 10) != ESP_OK || !count) return 0;
    reg = 0x03;
    uint8_t point[4];
    if (i2c_master_transmit_receive(touch, &reg, 1, point, sizeof(point), 10) != ESP_OK) return 0;
    unsigned x = ((point[0] & 0x0f) << 8) | point[1];
    unsigned y = ((point[2] & 0x0f) << 8) | point[3];
    if (y < 208) return 0;
    return 1u << (x < 107 ? 0 : x < 213 ? 1 : 2);
}
#endif

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
    m5stack_controls_defaults(&controls);
#if CONFIG_ESP_SDR_M5STACK_CORE2
    core2_power_init();
    const gpio_config_t outputs = {.pin_bit_mask = PIN(4) | PIN(LCD_DC), .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_set_level(4, 1)); /* Deselect the SD card on the shared SPI bus. */
    ESP_ERROR_CHECK(gpio_config(&outputs));
    bool invert = true;
#else
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
#endif

    const spi_bus_config_t bus = {
        .mosi_io_num = 23,
        .miso_io_num = -1,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(row),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));
    const spi_device_interface_config_t device = {
        .clock_speed_hz = 40000000,
        .mode = 0,
        .spics_io_num = LCD_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &device, &lcd));
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
#if CONFIG_ESP_SDR_M5STACK_CORE2
    if (ili9342e) {
        lcd_command(0xdd, (const uint8_t[]){0x01}, 1);
        lcd_command(0x3a, (const uint8_t[]){0x55}, 1);
        lcd_command(0x21, NULL, 0);
        lcd_command(0x36, (const uint8_t[]){0x08}, 1);
        lcd_command(0xd5, (const uint8_t[]){0x00}, 1);
        lcd_command(0xb1, (const uint8_t[]){0x22}, 1);
        lcd_command(0xc8, (const uint8_t[]){0x38}, 1);
        lcd_command(0xcb, (const uint8_t[]){0x1c}, 1);
        lcd_command(0xc9, (const uint8_t[]){0x1a}, 1);
        lcd_command(0xca, (const uint8_t[]){0x1a}, 1);
        lcd_command(0xb7, (const uint8_t[]){0x5a, 0x41, 0x11, 0x19}, 4);
        lcd_command(0xe4, (const uint8_t[]){0x04, 0x08, 0x11, 0x06, 0x12, 0x07, 0x3a, 0x76,
                                             0x47, 0x07, 0x0f, 0x0a, 0x11, 0x19, 0x05}, 15);
        lcd_command(0xe5, (const uint8_t[]){0x02, 0x03, 0x07, 0x06, 0x12, 0x07, 0x36, 0x5f,
                                             0x48, 0x06, 0x10, 0x0c, 0x16, 0x14, 0x09}, 15);
        lcd_command(0x11, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
        lcd_command(0x29, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
#endif
    m5stack_basic_draw(2412, NULL, m5stack_basic_fft_points(), false, false, 0);
    lcd_command(0x29, NULL, 0);
#if !CONFIG_ESP_SDR_M5STACK_CORE2
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 1));
#endif
}

unsigned m5stack_basic_poll(unsigned mhz, bool pc_control) {
#if CONFIG_ESP_SDR_M5STACK_CORE2
    unsigned pressed = pc_control ? 0 : core2_touch_buttons();
#else
    unsigned pressed = (!gpio_get_level(39)) | ((!gpio_get_level(38)) << 1) | ((!gpio_get_level(37)) << 2);
#endif
    mhz = m5stack_controls_update(&controls, mhz, pressed, (uint32_t)(esp_timer_get_time() / 1000), pc_control);
    density_mode(mhz, pc_control);
    return mhz;
}

bool m5stack_basic_sync_rx(unsigned *agc, unsigned *gain, unsigned gain_max,
                           int *filter, bool *follow_span) {
    return m5stack_controls_sync_rx(&controls, agc, gain, gain_max, filter, follow_span);
}

unsigned m5stack_basic_rate(void) {
    return m5stack_rates[controls.rate];
}

unsigned m5stack_basic_span(void) {
    return m5stack_spans[controls.rate];
}

unsigned m5stack_basic_fft_points(void) {
    return m5stack_fft_sizes[controls.fft];
}

unsigned m5stack_basic_average_count(void) {
    return m5stack_average_counts[controls.average];
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

#include "m5stack_menu.h"

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

static uint8_t title_glyph_row(char c, unsigned y) {
    static const uint8_t e[7] = {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f};
    static const uint8_t s[7] = {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
    static const uint8_t p[7] = {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
    static const uint8_t d[7] = {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e};
    static const uint8_t r[7] = {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    static const uint8_t m[7] = {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const uint8_t five[7] = {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e};
    static const uint8_t dash[7] = {0, 0, 0, 0x1f, 0, 0, 0};
    const uint8_t *rows = c == 'E' ? e : c == 'S' ? s : c == 'P' ? p : c == 'D' ? d :
                         c == 'R' ? r : c == 'M' ? m : c == '5' ? five : c == '-' ? dash : NULL;
    return rows ? rows[y] : 0;
}

static void title_row(unsigned y) {
    static const char title[] = "ESP SDR M5";
    if (y == 0 || y > 10) return;
    for (unsigned letter = 0; title[letter]; letter++)
        for (unsigned x = 0; x < 8; x++) {
            unsigned glyph_x = (x * 2 + 1) * 5 / 16;
            unsigned glyph_y = ((y - 1) * 2 + 1) * 7 / 20;
            if (title_glyph_row(title[letter], glyph_y) & (1u << (4 - glyph_x)))
                row[8 + letter * 9 + x] = __builtin_bswap16(0x0000);
        }
}

void m5stack_basic_set_gain_index(unsigned gain_index) {
    display_gain_index = gain_index;
}

static uint16_t waterfall_color(int delta_db) {
    static const uint16_t colors[] = {
    0x0000, 0x0003, 0x0006, 0x0009, 0x000c, 0x0010, 0x0013, 0x0016,
    0x0019, 0x001c, 0x001f, 0x00df, 0x01bf, 0x027f, 0x033f, 0x041f,
    0x04df, 0x059f, 0x065f, 0x073f, 0x07ff, 0x07fc, 0x07f9, 0x07f6,
    0x07f3, 0x07f0, 0x07ec, 0x07e9, 0x07e6, 0x07e3, 0x07e0, 0x1fe0,
    0x37e0, 0x4fe0, 0x67e0, 0x87e0, 0x9fe0, 0xb7e0, 0xcfe0, 0xe7e0,
    0xffe0, 0xff20, 0xfe40, 0xfd80, 0xfcc0, 0xfc00, 0xfb20, 0xfa60,
    0xf9a0, 0xf8c0, 0xf800, 0xf8c3, 0xf9a6, 0xfa69, 0xfb2c, 0xfc10,
    0xfcd3, 0xfd96, 0xfe59, 0xff3c, 0xffff,
    };
    delta_db = delta_db * controls.wf_contrast / 100 + controls.wf_bright;
    return colors[delta_db <= 0 ? 0 : delta_db >= 60 ? 60 : delta_db];
}

void m5stack_basic_draw(unsigned mhz, const uint8_t *bins, unsigned fft_points, bool pc_control,
                        bool valid, unsigned cpu_percent) {
    density_mode(mhz, pc_control);
    density_decay();
    static const char title[] = "ESP SDR M5";
    char edges[3][12], cpu[12], gain[8];
    char menu_value[24];
    m5stack_menu_value(menu_value, sizeof(menu_value));
    spectrum_dbfs_t display_bins[PLOT_WIDTH];
    uint8_t *spectrum_trace = (uint8_t *)display_bins;
    spectrum_dbfs_t noise_floor = noise_floor_avg.ready ?
        (spectrum_dbfs_t)((noise_floor_avg.q8 + (noise_floor_avg.q8 < 0 ? -128 : 128)) / 256) : SPECTRUM_DBFS_MIN;
    unsigned span = m5stack_spans[controls.rate];
    if (!waterfall_tuned || waterfall_mhz != mhz || waterfall_span != span || waterfall_fft != fft_points) {
        waterfall_mhz = mhz;
        waterfall_span = span;
        waterfall_fft = fft_points;
        waterfall_head = waterfall_count = waterfall_frame = 0;
        noise_floor_avg = (spectrum_noise_floor_t){0};
        waterfall_tuned = true;
    }
    snprintf(edges[0], sizeof(edges[0]), "%u", mhz - span / 2);
    snprintf(edges[1], sizeof(edges[1]), "%u", mhz);
    snprintf(edges[2], sizeof(edges[2]), "%u", mhz + span / 2);
    snprintf(cpu, sizeof(cpu), "CPU %u%%", cpu_percent);
    snprintf(gain, sizeof(gain), "%u", display_gain_index);
    unsigned cpu_left = 318 - (unsigned)strlen(cpu) * 8;
    const char *status = pc_control ? "PC" : controls.dpx_failed ? "RAM" :
                         valid ? (density ? "DPX" : "LIVE") : "WAIT";
    unsigned status_left = cpu_left - (unsigned)strlen(status) * 8 - 8;
    if (bins && valid) {
        for (unsigned plot_x = 0; plot_x < PLOT_WIDTH; plot_x++)
            display_bins[plot_x] = SPECTRUM_DBFS_MIN;
        unsigned capture_span = span < 16 ? 16 : span;
        unsigned visible_points = fft_points * span / capture_span;
        unsigned first_bin = (fft_points - visible_points) / 2 + fft_points / 2;
        spectrum_dbfs_t frame_minimum = SPECTRUM_DBFS_MAX;
        for (unsigned plot_x = 0; plot_x < PLOT_WIDTH; plot_x++) {
            unsigned first = plot_x * visible_points / PLOT_WIDTH;
            unsigned limit = (plot_x + 1) * visible_points / PLOT_WIDTH;
            if (limit <= first) limit = first + 1;
            if (limit > visible_points) limit = visible_points;
            for (unsigned shifted = first; shifted < limit; shifted++) {
                unsigned bin = first_bin + shifted;
                if (bin >= fft_points) bin -= fft_points;
                /* The radio produces negative FFT offsets for RF above the LO.
                 * Mirror the selected bins so higher RF is right. */
                bin = fft_points - 1 - bin;
                spectrum_dbfs_t dbfs = spectrum_code_to_dbfs(bins[bin]);
                if (dbfs < frame_minimum) frame_minimum = dbfs;
                if (dbfs > display_bins[plot_x]) display_bins[plot_x] = dbfs;
            }
        }
        noise_floor = spectrum_noise_floor_update(&noise_floor_avg, frame_minimum);
    }
    if (bins && valid && !pc_control &&
        waterfall_frame++ % m5stack_wf_intervals[controls.wf_speed] == 0) {
        waterfall_head = (waterfall_head + 1) % WATERFALL_HEIGHT;
        memcpy(waterfall[waterfall_head], display_bins, sizeof(display_bins));
        waterfall_floor[waterfall_head] = noise_floor;
        if (waterfall_count < WATERFALL_HEIGHT) waterfall_count++;
    }
    /* After saving waterfall levels, reuse the same bytes for the normal trace. */
    if (!density && bins && valid) {
        int graph_minimum = noise_floor - controls.y_pos;
        for (unsigned x = 0; x < PLOT_WIDTH; x++) {
            unsigned height = (spectrum_dbfs_to_display_value(display_bins[x], graph_minimum, 80) *
                               (SPECTRUM_BOTTOM - SPECTRUM_TOP)) / 255;
            spectrum_trace[x] = SPECTRUM_BOTTOM - height;
        }
    }
    lcd_command(0x2a, (const uint8_t[]){0, 0, 1, 0x3f}, 4);
    for (unsigned y = 0; y < 240; y++) {
        memset(row, 0, sizeof(row));
        if (y >= SPECTRUM_TOP && y <= SPECTRUM_BOTTOM) {
            bool horizontal_grid = (y - SPECTRUM_TOP) % 13 == 0;
            unsigned division = 0, next_grid = 0;
            for (unsigned x = 0; x < PLOT_WIDTH; x++) {
                bool vertical_grid = x == next_grid;
                if (vertical_grid) next_grid = (++division * (PLOT_WIDTH - 1) + 4) / 8;
                uint16_t color = y == SPECTRUM_BOTTOM ? 0x4208 :
                                 horizontal_grid || vertical_grid ? 0x2104 : 0;
                if (!density && bins && valid && y != SPECTRUM_BOTTOM) {
                    unsigned current = spectrum_trace[x], previous = x ? spectrum_trace[x - 1] : current;
                    if (y >= (current < previous ? current : previous) &&
                        y <= (current > previous ? current : previous)) color = 0x05ff;
                }
                row[PLOT_LEFT + x] = __builtin_bswap16(color);
            }
            if (density && y < SPECTRUM_BOTTOM) density_row(y, noise_floor - controls.y_pos);
        } else if (y >= WATERFALL_TOP && y < WATERFALL_TOP + WATERFALL_HEIGHT) {
            unsigned age = y - WATERFALL_TOP;
            if (age < waterfall_count) {
                unsigned index = waterfall_head + WATERFALL_HEIGHT - age;
                if (index >= WATERFALL_HEIGHT) index -= WATERFALL_HEIGHT;
                const spectrum_dbfs_t *history = waterfall[index];
                for (unsigned x = 0; x < PLOT_WIDTH; x++)
                    row[PLOT_LEFT + x] = __builtin_bswap16(waterfall_color(history[x] - waterfall_floor[index]));
            }
        }
        if (y < 12)
            for (unsigned x = 4; x < 12 + (sizeof(title) - 1) * 9; x++)
                row[x] = __builtin_bswap16(COLOR_SRDECK_YELLOW_GREEN);
        title_row(y);
        text_row(y, 160 - (unsigned)strlen(gain) * 4, 3, gain, 2, 0xffff);
        text_row(y, cpu_left, 3, cpu, 2, 0xffff);
        text_row(y, status_left, 3, status, 2, pc_control ? 0xffe0 : 0xffff);
        text_row(y, 4, FREQUENCY_TOP, edges[0], 2, 0xffff);
        text_row(y, 144, FREQUENCY_TOP, edges[1], 2, 0xffff);
        text_row(y, 284, FREQUENCY_TOP, edges[2], 2, 0xffff);
        text_row(y, controls.menu == M5_MENU_RESET ? 30 : controls.menu ? 50 : 34, BUTTON_HINT_TOP,
                 controls.menu == M5_MENU_RESET ? "CANCEL" : controls.menu ? "-" : "TUNE-",
                 2, COLOR_SRDECK_YELLOW_GREEN);
        text_row(y, 144, BUTTON_HINT_TOP, "MENU", 2, COLOR_SRDECK_YELLOW_GREEN);
        text_row(y, controls.menu && controls.menu != M5_MENU_RESET ? 264 : 248, BUTTON_HINT_TOP,
                 controls.menu == M5_MENU_RESET ? "RESET" : controls.menu ? "+" : "TUNE+",
                 2, COLOR_SRDECK_YELLOW_GREEN);
        if (!pc_control && controls.menu) m5stack_menu_row(y, menu_value);
        const uint8_t address[] = {0, y, 0, y};
        lcd_command(0x2b, address, 4);
        lcd_command(0x2c, NULL, 0);
        lcd_write(true, row, sizeof(row));
    }
}
