/* Compact M5Stack displays: Cardputer/K132, Cardputer-ADV and StickC Plus2. */
#include "m5stack_basic.h"
#include "m5stack_basic_controls.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
#include "driver/i2c_master.h"
#endif
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#if CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
#define SCREEN_WIDTH 135
#define SCREEN_HEIGHT 240
#define LCD_MOSI 15
#define LCD_SCLK 13
#define LCD_CS 5
#define LCD_DC 14
#define LCD_RST 12
#define LCD_BL 27
#define LCD_X_OFFSET 52
#define LCD_Y_OFFSET 40
#define LCD_HOST SPI2_HOST
#define LCD_MADCTL 0x08
#define PLOT_LEFT 4
#define PLOT_WIDTH 127
#define SPECTRUM_TOP 24
#define SPECTRUM_BOTTOM 65
#define FREQUENCY_TOP 67
#define WATERFALL_TOP 75
#define WATERFALL_HEIGHT 157
#else
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135
#define LCD_MOSI 35
#define LCD_SCLK 36
#define LCD_CS 37
#define LCD_DC 34
#define LCD_RST 33
#define LCD_BL 38
#define LCD_X_OFFSET 40
#define LCD_Y_OFFSET 53
#define LCD_HOST SPI3_HOST
#define LCD_MADCTL 0x6c
#define PLOT_LEFT 4
#define PLOT_WIDTH 232
#define SPECTRUM_TOP 20
#define SPECTRUM_BOTTOM 45
#define FREQUENCY_TOP 48
#define WATERFALL_TOP 56
#define WATERFALL_HEIGHT 64
#endif
#define COLOR_SRDECK_YELLOW_GREEN 0xb726
#define PIN(n) (UINT64_C(1) << (n))

static spi_device_handle_t lcd;
static m5stack_controls_t controls;
static uint16_t row[SCREEN_WIDTH] __attribute__((aligned(4)));
static uint8_t (*waterfall)[PLOT_WIDTH];
static unsigned waterfall_head, waterfall_count, waterfall_mhz, waterfall_span, waterfall_fft;
static bool waterfall_tuned;
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
static i2c_master_dev_handle_t keyboard_i2c;
static unsigned adv_keyboard_state;

static void tca8418_write(uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_transmit(keyboard_i2c, data, sizeof(data), 20));
}

static uint8_t tca8418_read(uint8_t reg) {
    uint8_t value;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(keyboard_i2c, &reg, 1, &value, 1, 20));
    return value;
}

static void adv_keyboard_init(void) {
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0, .sda_io_num = 8, .scl_io_num = 9,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));
    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x34, .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &device, &keyboard_i2c));

    /* Match M5Stack's TCA8418 setup for the 7-row by 8-column keyboard. */
    for (uint8_t reg = 0x23; reg <= 0x25; reg++) tca8418_write(reg, 0x00);
    for (uint8_t reg = 0x20; reg <= 0x22; reg++) tca8418_write(reg, 0xff);
    for (uint8_t reg = 0x26; reg <= 0x28; reg++) tca8418_write(reg, 0x00);
    for (uint8_t reg = 0x1a; reg <= 0x1c; reg++) tca8418_write(reg, 0xff);
    tca8418_write(0x1d, 0x7f);
    tca8418_write(0x1e, 0xff);
    while (tca8418_read(0x03) & 0x0f) (void)tca8418_read(0x04);
    (void)tca8418_read(0x14);
    (void)tca8418_read(0x15);
    (void)tca8418_read(0x16);
    tca8418_write(0x02, 0x03);
    tca8418_write(0x01, 0x03);
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

static void text_row(unsigned y, unsigned left, const char *text, uint16_t color) {
    if (y >= 5) return;
    unsigned shift = 12 - 3 * y;
    for (; *text && left + 3 <= SCREEN_WIDTH; text++, left += 4) {
        unsigned pixels = (glyph(*text) >> shift) & 7;
        for (unsigned x = 0; x < 3; x++)
            if (pixels & (4u >> x)) row[left + x] = __builtin_bswap16(color);
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

static unsigned keyboard_buttons(void) {
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
    unsigned count = tca8418_read(0x03) & 0x0f;
    while (count--) {
        unsigned event = tca8418_read(0x04);
        unsigned code = event & 0x7f;
        if (!code || code > 0x50) continue;
        unsigned position = code - 1;
        unsigned matrix_row = position / 10, matrix_col = position % 10;
        if (matrix_row >= 7 || matrix_col >= 8) continue;
        unsigned col = matrix_row * 2 + (matrix_col > 3), row = (matrix_col + 4) % 4;
        unsigned bit = row == 3 && col == 10 ? 1u : row == 3 && col == 13 ? 2u :
                       row == 3 && col == 12 ? 4u : 0u;
        if (!bit) continue;
        if (event & 0x80) adv_keyboard_state &= ~bit; /* TCA8418 release event. */
        else adv_keyboard_state |= bit;
    }
    return adv_keyboard_state;
#elif CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    unsigned pressed = 0;
    if (!gpio_get_level(GPIO_NUM_37)) pressed |= 1u; /* A */
    if (!gpio_get_level(GPIO_NUM_39)) pressed |= 2u; /* B */
    if (!gpio_get_level(GPIO_NUM_35)) pressed |= 4u; /* C */
    return pressed;
#else
    static const gpio_num_t out_pins[] = {GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_11};
    unsigned pressed = 0;
    for (unsigned address = 0; address < 8; address++) {
        for (unsigned bit = 0; bit < 3; bit++) gpio_set_level(out_pins[bit], (address >> bit) & 1);
        if (address == 4 && !gpio_get_level(GPIO_NUM_6)) pressed |= 1u; /* comma: tune down/span */
        if (address == 0 && !gpio_get_level(GPIO_NUM_7)) pressed |= 2u; /* space: step/FFT */
        if (address == 4 && !gpio_get_level(GPIO_NUM_7)) pressed |= 4u; /* slash: tune up/average */
    }
    return pressed;
#endif
}

void m5stack_basic_init(void) {
    controls.rate = 3;
    controls.average = 3;
#if CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    const gpio_config_t hold = {.pin_bit_mask = PIN(4), .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&hold));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_4, 1)); /* Keep the Plus2 power rail on. */
#endif
    waterfall = heap_caps_calloc(WATERFALL_HEIGHT, PLOT_WIDTH, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(waterfall ? ESP_OK : ESP_ERR_NO_MEM);
    const gpio_config_t outputs = {
#if CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2 || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
        .pin_bit_mask = PIN(LCD_DC) | PIN(LCD_RST) | PIN(LCD_BL),
#else
        .pin_bit_mask = PIN(LCD_DC) | PIN(LCD_RST) | PIN(LCD_BL) | PIN(8) | PIN(9) | PIN(11),
#endif
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&outputs));
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_8, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_9, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_11, 0));
#endif
    const gpio_config_t inputs = {
#if CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
        .pin_bit_mask = PIN(35) | PIN(37) | PIN(39),
        .mode = GPIO_MODE_INPUT,
#elif CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
        .pin_bit_mask = PIN(11),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
#else
        .pin_bit_mask = PIN(13) | PIN(15) | PIN(3) | PIN(4) | PIN(5) | PIN(6) | PIN(7),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
#endif
    };
    ESP_ERROR_CHECK(gpio_config(&inputs));
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
    adv_keyboard_init();
#endif
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 0));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(120));

    const spi_bus_config_t bus = {.mosi_io_num = LCD_MOSI, .miso_io_num = -1, .sclk_io_num = LCD_SCLK,
                                  .quadwp_io_num = -1, .quadhd_io_num = -1,
                                  .max_transfer_sz = sizeof(row)};
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));
    const spi_device_interface_config_t device = {
        .clock_speed_hz = 40000000, .mode = 0, .spics_io_num = LCD_CS, .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &device, &lcd));
    lcd_command(0x01, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_command(0x36, (const uint8_t[]){LCD_MADCTL}, 1);
    lcd_command(0x3a, (const uint8_t[]){0x55}, 1);
    lcd_command(0xb0, (const uint8_t[]){0x00, 0xc0}, 2);
    lcd_command(0xb7, (const uint8_t[]){0x35}, 1);
    lcd_command(0xbb, (const uint8_t[]){0x28}, 1);
    lcd_command(0xc0, (const uint8_t[]){0x0c}, 1);
    lcd_command(0xc2, (const uint8_t[]){0x01, 0xff}, 2);
    lcd_command(0xc3, (const uint8_t[]){0x10}, 1);
    lcd_command(0xc4, (const uint8_t[]){0x20}, 1);
    lcd_command(0xd0, (const uint8_t[]){0xa4, 0xa1}, 2);
    lcd_command(0xe0, (const uint8_t[]){0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x32,
                                         0x44, 0x42, 0x06, 0x0e, 0x12, 0x14, 0x17}, 14);
    lcd_command(0xe1, (const uint8_t[]){0xd0, 0x00, 0x02, 0x07, 0x0a, 0x28, 0x31,
                                         0x54, 0x47, 0x0e, 0x1c, 0x17, 0x1b, 0x1e}, 14);
    lcd_command(0x21, NULL, 0);
    lcd_command(0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(130));
    lcd_command(0x38, NULL, 0);
    m5stack_basic_draw(2412, NULL, 256, false, false, 0);
    lcd_command(0x29, NULL, 0);
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 1));
}

unsigned m5stack_basic_poll(unsigned mhz, bool pc_control) {
    return m5stack_controls_update(&controls, mhz, pc_control ? 0 : keyboard_buttons(),
                                  (uint32_t)(esp_timer_get_time() / 1000), pc_control);
}

unsigned m5stack_basic_rate(void) {
    return m5stack_rates[controls.rate];
}

unsigned m5stack_basic_fft_points(void) {
    return m5stack_fft_sizes[controls.fft];
}

unsigned m5stack_basic_average_count(void) {
    return m5stack_average_counts[controls.average];
}

void m5stack_basic_draw(unsigned mhz, const uint8_t *bins, unsigned fft_points, bool pc_control,
                        bool valid, unsigned cpu_percent) {
    char settings[40], edges[3][12], cpu[12];
    uint8_t display_bins[PLOT_WIDTH] = {0}, trace[PLOT_WIDTH];
    unsigned span = m5stack_spans[controls.rate];
    if (!waterfall_tuned || waterfall_mhz != mhz || waterfall_span != span || waterfall_fft != fft_points) {
        waterfall_mhz = mhz;
        waterfall_span = span;
        waterfall_fft = fft_points;
        waterfall_head = waterfall_count = 0;
        waterfall_tuned = true;
    }
    snprintf(settings, sizeof(settings), "STEP%uM SPAN%uM FFT%u AVE%u", m5stack_steps[controls.step], span,
             fft_points, m5stack_basic_average_count());
    snprintf(edges[0], sizeof(edges[0]), "%u", mhz - span / 2);
    snprintf(edges[1], sizeof(edges[1]), "%u", mhz);
    snprintf(edges[2], sizeof(edges[2]), "%u", mhz + span / 2);
    snprintf(cpu, sizeof(cpu), "CPU%u%%", cpu_percent);
    if (bins && valid) {
        unsigned capture_span = span < 16 ? 16 : span;
        unsigned visible = fft_points * span / capture_span;
        unsigned first_point = (fft_points - visible) / 2;
        for (unsigned x = 0; x < PLOT_WIDTH; x++) {
            unsigned first = x * visible / PLOT_WIDTH, limit = (x + 1) * visible / PLOT_WIDTH;
            if (limit <= first) limit = first + 1;
            if (limit > visible) limit = visible;
            for (unsigned j = first; j < limit; j++) {
                unsigned bin = (first_point + j + fft_points / 2) % fft_points;
                if (bins[bin] > display_bins[x]) display_bins[x] = bins[bin];
            }
            trace[x] = SPECTRUM_BOTTOM -
                       (display_bins[x] * (SPECTRUM_BOTTOM - SPECTRUM_TOP)) / 255;
        }
        if (!pc_control) {
            waterfall_head = (waterfall_head + 1) % WATERFALL_HEIGHT;
            memcpy(waterfall[waterfall_head], display_bins, sizeof(display_bins));
            if (waterfall_count < WATERFALL_HEIGHT) waterfall_count++;
        }
    }

    for (unsigned y = 0; y < SCREEN_HEIGHT; y++) {
        for (unsigned x = 0; x < SCREEN_WIDTH; x++) {
            uint16_t color = 0;
            if (x >= PLOT_LEFT && x < PLOT_LEFT + PLOT_WIDTH) {
                unsigned plot_x = x - PLOT_LEFT;
                if (y >= SPECTRUM_TOP && y <= SPECTRUM_BOTTOM) {
                    if (y == SPECTRUM_BOTTOM) color = 0x4208;
                    else if ((y - SPECTRUM_TOP) % 6 == 0 || plot_x % (PLOT_WIDTH / 8) == 0 ||
                             plot_x == PLOT_WIDTH - 1) color = 0x2104;
                    if (bins && valid && y != SPECTRUM_BOTTOM) {
                        unsigned current = trace[plot_x], previous = plot_x ? trace[plot_x - 1] : current;
                        if (y >= (current < previous ? current : previous) &&
                            y <= (current > previous ? current : previous)) color = 0x05ff;
                    }
                } else if (y >= WATERFALL_TOP && y < WATERFALL_TOP + WATERFALL_HEIGHT) {
                    unsigned age = y - WATERFALL_TOP;
                    if (age < waterfall_count) {
                        unsigned index = (waterfall_head + WATERFALL_HEIGHT - age) % WATERFALL_HEIGHT;
                        color = waterfall_color(waterfall[index][plot_x]);
                    }
                }
            }
            if (y < 7 && x >= 4 && x < 36) color = COLOR_SRDECK_YELLOW_GREEN;
            row[x] = __builtin_bswap16(color);
        }
#if CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
        if (y < 5) {
            text_row(y, 4, "SDR", 0x0000);
            unsigned cpu_left = SCREEN_WIDTH - (unsigned)strlen(cpu) * 4;
            text_row(y, cpu_left, cpu, 0xffff);
        }
        if (y >= 8 && y < 13) text_row(y - 8, 4, settings, 0xffff);
        if (y >= 16 && y < 21) {
            text_row(y - 16, 36, pc_control ? "PC" : valid ? "LIVE" : "WAIT", 0xffff);
        }
        if (y >= FREQUENCY_TOP && y < FREQUENCY_TOP + 5) {
            text_row(y - FREQUENCY_TOP, PLOT_LEFT, edges[0], 0xffff);
            text_row(y - FREQUENCY_TOP, PLOT_LEFT + PLOT_WIDTH / 2 - 8, edges[1], 0xffff);
            text_row(y - FREQUENCY_TOP, PLOT_LEFT + PLOT_WIDTH - 16, edges[2], 0xffff);
        }
        if (y >= 234 && y < 239)
            text_row(y - 234, 39, "SPAN  FFT  AVE", COLOR_SRDECK_YELLOW_GREEN);
#else
        if (y < 5) {
            text_row(y, 4, "SDR", 0x0000);
            text_row(y, 185, cpu, 0xffff);
        }
        if (y >= 8 && y < 13) {
            text_row(y - 8, 4, settings, 0xffff);
            text_row(y - 8, 222, pc_control ? "PC" : valid ? "LIVE" : "WAIT", pc_control ? 0xffe0 : 0xffff);
        }
        if (y >= FREQUENCY_TOP && y < FREQUENCY_TOP + 5) {
            text_row(y - FREQUENCY_TOP, 4, edges[0], 0xffff);
            text_row(y - FREQUENCY_TOP, 112, edges[1], 0xffff);
            text_row(y - FREQUENCY_TOP, 208, edges[2], 0xffff);
        }
        if (y >= 128 && y < 133) {
            text_row(y - 128, 4, "LEFT SPAN SPACE FFT RIGHT AVE", COLOR_SRDECK_YELLOW_GREEN);
        }
#endif
        const uint16_t x_end = LCD_X_OFFSET + SCREEN_WIDTH - 1;
        lcd_command(0x2a, (const uint8_t[]){0, LCD_X_OFFSET, (uint8_t)(x_end >> 8), (uint8_t)x_end}, 4);
        unsigned address = y + LCD_Y_OFFSET;
        const uint8_t window[] = {(uint8_t)(address >> 8), (uint8_t)address,
                                  (uint8_t)(address >> 8), (uint8_t)address};
        lcd_command(0x2b, window, sizeof(window));
        lcd_command(0x2c, NULL, 0);
        lcd_write(true, row, sizeof(row));
    }
}
