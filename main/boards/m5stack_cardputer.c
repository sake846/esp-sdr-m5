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
#define LCD_CLOCK_HZ 20000000
#define LCD_MADCTL 0x08
#define PLOT_LEFT 4
#define PLOT_WIDTH 127
#define SPECTRUM_TOP 38
#define SPECTRUM_BOTTOM 79
#define FREQUENCY_TOP 82
#define WATERFALL_TOP 94
#define WATERFALL_HEIGHT 122
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
#define LCD_CLOCK_HZ 40000000
#define LCD_MADCTL 0x6c
#define PLOT_LEFT 4
#define PLOT_WIDTH 232
#define SPECTRUM_TOP 24
#define SPECTRUM_BOTTOM 47
#define FREQUENCY_TOP 49
#define WATERFALL_TOP 60
#define WATERFALL_HEIGHT (SCREEN_HEIGHT - WATERFALL_TOP)
#endif
#define COLOR_SRDECK_YELLOW_GREEN 0xb726
#define PIN(n) (UINT64_C(1) << (n))

#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
enum {
    MATRIX_COMMA = 1u << 0, MATRIX_PERIOD = 1u << 1, MATRIX_SLASH = 1u << 2,
    MATRIX_SPACE = 1u << 3, MATRIX_T = 1u << 4, MATRIX_P = 1u << 5,
    MATRIX_F = 1u << 6, MATRIX_A = 1u << 7, MATRIX_DIGIT_0 = 1u << 8,
    MATRIX_SHIFT = 1u << 18,
    ACTION_LEFT = 1u << 0, ACTION_RIGHT = 1u << 1, ACTION_SPACE = 1u << 2,
    ACTION_T = 1u << 3, ACTION_P = 1u << 4, ACTION_F = 1u << 5, ACTION_A = 1u << 6,
    ACTION_DIGIT_0 = 1u << 7,
};
static bool keyboard_help_visible;
static unsigned keyboard_raw, keyboard_stable;
static uint32_t keyboard_changed[17];
static bool keyboard_reverse[7];
static char frequency_input[5];
static unsigned frequency_input_length, frequency_input_value;

static unsigned matrix_key_bit(unsigned matrix_row, unsigned matrix_col) {
    if (matrix_row == 0 && matrix_col >= 1 && matrix_col <= 10)
        return MATRIX_DIGIT_0 << (matrix_col == 10 ? 0 : matrix_col);
    if (matrix_row == 3 && matrix_col == 10) return MATRIX_COMMA;
    if (matrix_row == 3 && matrix_col == 11) return MATRIX_PERIOD;
    if (matrix_row == 3 && matrix_col == 12) return MATRIX_SLASH;
    if (matrix_row == 3 && matrix_col == 13) return MATRIX_SPACE;
    if (matrix_row == 1 && matrix_col == 5) return MATRIX_T;
    if (matrix_row == 1 && matrix_col == 10) return MATRIX_P;
    if (matrix_row == 2 && matrix_col == 1) return MATRIX_SHIFT;
    if (matrix_row == 2 && matrix_col == 5) return MATRIX_F;
    if (matrix_row == 2 && matrix_col == 2) return MATRIX_A;
    return 0;
}
#endif

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
    if (c == '<') return 0x2922;
    if (c == '>') return 0x224a;
    return 0;
}

static void text_row(unsigned y, unsigned left, const char *text, uint16_t color) {
    if (y >= 10) return;
    unsigned source_y = y / 2;
    for (; *text && left + 6 <= SCREEN_WIDTH; text++, left += 7) {
        unsigned pixels = (glyph(*text) >> (12 - 3 * source_y)) & 7;
        for (unsigned x = 0; x < 3; x++) {
            if (!(pixels & (4u >> x))) continue;
            row[left + 2 * x] = __builtin_bswap16(color);
            row[left + 2 * x + 1] = __builtin_bswap16(color);
        }
    }
}

static void title_row(unsigned y) {
    static const char title[] = "ESP-SDR-M5";
    text_row(y, 6, title, 0x0000);
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

static inline bool spectrum_vertical_grid(unsigned plot_x) {
    const unsigned divisions = 8, interval = PLOT_WIDTH - 1;
    unsigned division = (plot_x * divisions + interval / 2) / interval;
    unsigned position = (division * interval + divisions / 2) / divisions;
    return plot_x == position;
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
        unsigned bit = matrix_key_bit(row, col);
        if (!bit) continue;
        if (event & 0x80) adv_keyboard_state &= ~bit; /* TCA8418 release event. */
        else adv_keyboard_state |= bit;
    }
    return adv_keyboard_state;
#elif CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    unsigned pressed = 0;
    if (!gpio_get_level(GPIO_NUM_35)) pressed |= 1u; /* C, left: tune down / span */
    if (!gpio_get_level(GPIO_NUM_37)) pressed |= 2u; /* A, center: step / FFT */
    if (!gpio_get_level(GPIO_NUM_39)) pressed |= 4u; /* B, right: tune up / average */
    return pressed;
#else
    static const gpio_num_t out_pins[] = {GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_11};
    static const gpio_num_t in_pins[] = {GPIO_NUM_13, GPIO_NUM_15, GPIO_NUM_3,
                                         GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7};
    unsigned pressed = 0;
    for (unsigned address = 0; address < 8; address++) {
        for (unsigned bit = 0; bit < 3; bit++) gpio_set_level(out_pins[bit], (address >> bit) & 1);
        unsigned matrix_row = address < 4 ? 3 - address : 7 - address;
        for (unsigned input = 0; input < 7; input++) {
            if (gpio_get_level(in_pins[input])) continue;
            unsigned matrix_col = address < 4 ? input * 2 + 1 : input * 2;
            pressed |= matrix_key_bit(matrix_row, matrix_col);
        }
    }
    return pressed;
#endif
}

#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
static unsigned cardputer_actions(unsigned matrix_keys) {
    unsigned actions = 0;
    if (matrix_keys & MATRIX_COMMA) actions |= ACTION_LEFT;
    if (matrix_keys & (MATRIX_PERIOD | MATRIX_SLASH)) actions |= ACTION_RIGHT;
    if (matrix_keys & MATRIX_SPACE) actions |= ACTION_SPACE;
    if (matrix_keys & MATRIX_T) actions |= ACTION_T;
    if (matrix_keys & MATRIX_P) actions |= ACTION_P;
    if (matrix_keys & MATRIX_F) actions |= ACTION_F;
    if (matrix_keys & MATRIX_A) actions |= ACTION_A;
    for (unsigned digit = 0; digit < 10; digit++)
        if (matrix_keys & (MATRIX_DIGIT_0 << digit)) actions |= ACTION_DIGIT_0 << digit;
    return actions;
}

static unsigned cardputer_controls_update(unsigned mhz, unsigned matrix_keys, uint32_t now,
                                           bool pc_control) {
    static const unsigned action_bits[] = {
        ACTION_LEFT, ACTION_RIGHT, ACTION_SPACE, ACTION_T, ACTION_P, ACTION_F, ACTION_A,
        ACTION_DIGIT_0, ACTION_DIGIT_0 << 1, ACTION_DIGIT_0 << 2, ACTION_DIGIT_0 << 3,
        ACTION_DIGIT_0 << 4, ACTION_DIGIT_0 << 5, ACTION_DIGIT_0 << 6, ACTION_DIGIT_0 << 7,
        ACTION_DIGIT_0 << 8, ACTION_DIGIT_0 << 9,
    };
    unsigned actions = cardputer_actions(matrix_keys);
    if (pc_control) {
        keyboard_raw = keyboard_stable = actions;
        for (unsigned i = 0; i < sizeof(action_bits) / sizeof(action_bits[0]); i++)
            keyboard_changed[i] = now;
        frequency_input[0] = '\0';
        frequency_input_length = frequency_input_value = 0;
        return mhz;
    }
    for (unsigned i = 0; i < sizeof(action_bits) / sizeof(action_bits[0]); i++) {
        unsigned bit = action_bits[i];
        if ((actions ^ keyboard_raw) & bit) {
            keyboard_raw ^= bit;
            keyboard_changed[i] = now;
            if ((actions & bit) && i >= 3 && i <= 6)
                keyboard_reverse[i] = (matrix_keys & MATRIX_SHIFT) != 0;
        }
        if (!((keyboard_raw ^ keyboard_stable) & bit) || (uint32_t)(now - keyboard_changed[i]) < 30)
            continue;
        keyboard_stable ^= bit;
        if (!(keyboard_stable & bit)) continue;
        if (i >= 7) {
            unsigned digit = i - 7;
            frequency_input[frequency_input_length++] = (char)('0' + digit);
            frequency_input[frequency_input_length] = '\0';
            frequency_input_value = frequency_input_value * 10 + digit;
            if (frequency_input_length == 4) {
                if (rx_frequency_valid(frequency_input_value)) mhz = frequency_input_value;
                frequency_input[0] = '\0';
                frequency_input_length = frequency_input_value = 0;
            }
            continue;
        }
        switch (bit) {
        case ACTION_LEFT: {
            unsigned step = m5stack_steps[controls.step];
            mhz = mhz - RX_FREQ_MIN < step ? RX_FREQ_MIN : mhz - step;
            break;
        }
        case ACTION_RIGHT: {
            unsigned step = m5stack_steps[controls.step];
            mhz = RX_FREQ_MAX - mhz < step ? RX_FREQ_MAX : mhz + step;
            break;
        }
        case ACTION_SPACE: keyboard_help_visible = !keyboard_help_visible; break;
        case ACTION_T: {
            unsigned count = sizeof(m5stack_steps) / sizeof(m5stack_steps[0]);
            controls.step = (controls.step + (keyboard_reverse[i] ? count - 1 : 1)) % count;
            break;
        }
        case ACTION_P: {
            unsigned count = sizeof(m5stack_spans) / sizeof(m5stack_spans[0]);
            controls.rate = (controls.rate + (keyboard_reverse[i] ? count - 1 : 1)) % count;
            break;
        }
        case ACTION_F: {
            unsigned count = sizeof(m5stack_fft_sizes) / sizeof(m5stack_fft_sizes[0]);
            controls.fft = (controls.fft + (keyboard_reverse[i] ? count - 1 : 1)) % count;
            break;
        }
        case ACTION_A: {
            unsigned count = sizeof(m5stack_average_counts) / sizeof(m5stack_average_counts[0]);
            controls.average = (controls.average + (keyboard_reverse[i] ? count - 1 : 1)) % count;
            break;
        }
        }
    }
    return mhz;
}
#endif

void m5stack_basic_init(void) {
    controls.rate = 3;
    controls.fft = 2; /* Default to 1024 points. */
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
        .clock_speed_hz = LCD_CLOCK_HZ, .mode = 0, .spics_io_num = LCD_CS, .queue_size = 1,
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
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
    return cardputer_controls_update(mhz, keyboard_buttons(),
                                    (uint32_t)(esp_timer_get_time() / 1000), pc_control);
#else
    return m5stack_controls_update(&controls, mhz, pc_control ? 0 : keyboard_buttons(),
                                  (uint32_t)(esp_timer_get_time() / 1000), pc_control);
#endif
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
#if CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    char details[24];
#endif
    const char *status = pc_control ? "PC" : valid ? "LIVE" : "WAIT";
    uint8_t display_bins[PLOT_WIDTH] = {0}, trace[PLOT_WIDTH];
    unsigned span = m5stack_spans[controls.rate];
    if (!waterfall_tuned || waterfall_mhz != mhz || waterfall_span != span || waterfall_fft != fft_points) {
        waterfall_mhz = mhz;
        waterfall_span = span;
        waterfall_fft = fft_points;
        waterfall_head = waterfall_count = 0;
        waterfall_tuned = true;
    }
#if CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    snprintf(settings, sizeof(settings), "STP %uM SPN %uM", m5stack_steps[controls.step], span);
    snprintf(details, sizeof(details), "FFT %u AVE %u", fft_points, m5stack_basic_average_count());
#else
    snprintf(settings, sizeof(settings), "STP %uM SPN %uM FFT %u AVE %u", m5stack_steps[controls.step], span,
             fft_points, m5stack_basic_average_count());
#endif
    snprintf(edges[0], sizeof(edges[0]), "%u", mhz - span / 2);
    snprintf(edges[1], sizeof(edges[1]), "%u", mhz);
    snprintf(edges[2], sizeof(edges[2]), "%u", mhz + span / 2);
    snprintf(cpu, sizeof(cpu), "CPU %u%%", cpu_percent);
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
                    else if ((y - SPECTRUM_TOP) % 6 == 0 || spectrum_vertical_grid(plot_x))
                        color = 0x2104;
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
            if (y < 12 && x >= 4 && x < 78) color = COLOR_SRDECK_YELLOW_GREEN;
            row[x] = __builtin_bswap16(color);
        }
#if CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
        if (y >= 1 && y < 11) {
            title_row(y - 1);
            unsigned status_left = SCREEN_WIDTH - (unsigned)strlen(status) * 7 - 3;
            text_row(y - 1, status_left, status, pc_control ? 0xffe0 : 0xffff);
        }
        if (y >= 14 && y < 24) text_row(y - 14, 4, settings, 0xffff);
        if (y >= 26 && y < 36) text_row(y - 26, 4, details, 0xffff);
        if (y >= FREQUENCY_TOP && y < FREQUENCY_TOP + 10) {
            text_row(y - FREQUENCY_TOP, PLOT_LEFT, edges[0], 0xffff);
            text_row(y - FREQUENCY_TOP, SCREEN_WIDTH / 2 - (unsigned)strlen(edges[1]) * 7 / 2, edges[1], 0xffff);
            text_row(y - FREQUENCY_TOP, SCREEN_WIDTH - (unsigned)strlen(edges[2]) * 7 - 3, edges[2], 0xffff);
        }
        if (y >= 218 && y < 228) {
            text_row(y - 218, 4, "TUNE-", COLOR_SRDECK_YELLOW_GREEN);
            text_row(y - 218, 53, "STEP", COLOR_SRDECK_YELLOW_GREEN);
            text_row(y - 218, 96, "TUNE+", COLOR_SRDECK_YELLOW_GREEN);
        }
        if (y >= 230 && y < 240) {
            text_row(y - 230, 4, "SPAN", COLOR_SRDECK_YELLOW_GREEN);
            text_row(y - 230, 57, "FFT", COLOR_SRDECK_YELLOW_GREEN);
            text_row(y - 230, 110, "AVE", COLOR_SRDECK_YELLOW_GREEN);
        }
#else
        if (y >= 1 && y < 11) title_row(y - 1);
        if (y < 10) {
            unsigned cpu_left = SCREEN_WIDTH - (unsigned)strlen(cpu) * 7 - 3;
            unsigned status_left = cpu_left - (unsigned)strlen(status) * 7 - 7;
            text_row(y, status_left, status, pc_control ? 0xffe0 : 0xffff);
            text_row(y, cpu_left, cpu, 0xffff);
        }
        if (y >= 12 && y < 22) text_row(y - 12, 4, settings, 0xffff);
        if (y >= FREQUENCY_TOP && y < FREQUENCY_TOP + 10) {
            text_row(y - FREQUENCY_TOP, 4, edges[0], 0xffff);
            const char *center = frequency_input_length ? frequency_input : edges[1];
            text_row(y - FREQUENCY_TOP, SCREEN_WIDTH / 2 - (unsigned)strlen(center) * 7 / 2, center,
                     frequency_input_length ? COLOR_SRDECK_YELLOW_GREEN : 0xffff);
            text_row(y - FREQUENCY_TOP, SCREEN_WIDTH - (unsigned)strlen(edges[2]) * 7 - 3, edges[2], 0xffff);
        }
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
        if (keyboard_help_visible && y >= 115 && y < 135) {
            memset(row, 0, sizeof(row));
            text_row(y < 125 ? y - 115 : y - 125, 4,
                     y < 125 ? "<> 0-9 FREQ T STEP P SPAN" : "F FFT A AVE SHIFT REV SPACE HELP",
                     COLOR_SRDECK_YELLOW_GREEN);
        }
#endif
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
