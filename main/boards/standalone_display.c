/* Shared scanline spectrum renderer for standalone display devices. */
#include "standalone_display.h"
#include "standalone_display_backend.h"
#include "board_battery.h"
#include "standalone_display_controls.h"
#include "spectrum_math.h"
#include "driver/gpio.h"
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
#include "driver/i2c_master.h"
#endif
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

#ifndef M5_DISPLAY_LAYOUT_BASIC
#define M5_DISPLAY_LAYOUT_BASIC 0
#endif
#ifndef M5_DISPLAY_LAYOUT_STICK
#define M5_DISPLAY_LAYOUT_STICK 0
#endif
#define COLOR_SRDECK_YELLOW_GREEN 0xb726
#define PIN(n) (UINT64_C(1) << (n))

#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
enum {
    MATRIX_COMMA = 1u << 0, MATRIX_PERIOD = 1u << 1, MATRIX_SLASH = 1u << 2,
    MATRIX_SPACE = 1u << 3, MATRIX_T = 1u << 4, MATRIX_P = 1u << 5,
    MATRIX_F = 1u << 6, MATRIX_A = 1u << 7, MATRIX_DIGIT_0 = 1u << 9,
    MATRIX_SHIFT = 1u << 19,
    ACTION_LEFT = 1u << 0, ACTION_RIGHT = 1u << 1, ACTION_SPACE = 1u << 2,
    ACTION_T = 1u << 3, ACTION_P = 1u << 4, ACTION_F = 1u << 5, ACTION_A = 1u << 6,
    ACTION_DIGIT_0 = 1u << 8,
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

static m5stack_controls_t controls;
static uint16_t row[SCREEN_WIDTH] __attribute__((aligned(4)));
static spectrum_dbfs_t (*waterfall)[PLOT_WIDTH];
static spectrum_dbfs_t waterfall_floor[WATERFALL_HEIGHT];
static unsigned waterfall_head, waterfall_count, waterfall_mhz, waterfall_span, waterfall_fft;
static unsigned waterfall_frame;
static bool waterfall_tuned;
static spectrum_noise_floor_t noise_floor_avg;

#include "standalone_display_density.h"
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

#include "standalone_display_font.h"

#if CONFIG_ESP_SDR_M5STACK || CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI
#include "standalone_display_menu.h"
#endif

static void draw_global_tune_hints(unsigned y) {
    const char *hints[] = {"TUNE-", "MENU", "TUNE+"};
    if (SCREEN_WIDTH < 100) {
        if (y < SCREEN_HEIGHT - 21) return;
        m5stack_text_row(y, 20 - m5stack_text_width(hints[0], &DejaVu9) / 2,
                         SCREEN_HEIGHT - 12, hints[0], &DejaVu9, COLOR_SRDECK_YELLOW_GREEN, 0);
        m5stack_text_row(y, 60 - m5stack_text_width(hints[1], &DejaVu9) / 2,
                         SCREEN_HEIGHT - 12, hints[1], &DejaVu9, COLOR_SRDECK_YELLOW_GREEN, 0);
        m5stack_text_row(y, (SCREEN_WIDTH - m5stack_text_width(hints[2], &DejaVu9)) / 2,
                         SCREEN_HEIGHT - 2, hints[2], &DejaVu9, COLOR_SRDECK_YELLOW_GREEN, 0);
        return;
    }
    if (y < SCREEN_HEIGHT - 17) return;
    const int baseline = SCREEN_HEIGHT >= 200 ? SCREEN_HEIGHT - 9 : SCREEN_HEIGHT - 3;
    for (unsigned i = 0; i < 3; i++) {
        int center = (2 * (int)i + 1) * SCREEN_WIDTH / 6;
        m5stack_text_row(y, center - m5stack_text_width(hints[i], &DejaVu9) / 2,
                         baseline, hints[i], &DejaVu9, COLOR_SRDECK_YELLOW_GREEN, 0);
    }
}

void standalone_display_set_gain_index(unsigned gain_index) {
    (void)gain_index;
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

#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
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
            if ((actions & bit) && i >= 2 && i <= 6)
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
        case ACTION_SPACE:
            if (keyboard_reverse[i]) { controls.density_mode = !controls.density_mode; controls.density_failed = false; }
            else keyboard_help_visible = !keyboard_help_visible;
            break;
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

#endif

void standalone_display_init(void) {
    m5stack_controls_defaults(&controls);
    waterfall = heap_caps_calloc(WATERFALL_HEIGHT, PLOT_WIDTH,
#if (CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI || CONFIG_ESP_SDR_M5STACK) && CONFIG_SPIRAM
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
                                 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
    ESP_ERROR_CHECK(waterfall ? ESP_OK : ESP_ERR_NO_MEM);
    standalone_display_backend_init();
#if CONFIG_ESP_SDR_M5STACK
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER
    const gpio_config_t outputs = {
        .pin_bit_mask = PIN(8) | PIN(9) | PIN(11), .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&outputs));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_8, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_9, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_11, 0));
    const gpio_config_t inputs = {
        .pin_bit_mask = PIN(13) | PIN(15) | PIN(3) | PIN(4) | PIN(5) | PIN(6) | PIN(7),
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&inputs));
#elif CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
    const gpio_config_t inputs = {
        .pin_bit_mask = PIN(11), .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&inputs));
    adv_keyboard_init();
#endif
#endif
#if CONFIG_ESP_SDR_M5STACK || CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI
    standalone_display_draw(2440, NULL, standalone_display_fft_points(), false, false, 0);
#endif
}

unsigned standalone_display_poll(unsigned mhz, bool pc_control) {
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
    mhz = cardputer_controls_update(mhz, keyboard_buttons(),
                                    (uint32_t)(esp_timer_get_time() / 1000), pc_control);
#elif CONFIG_ESP_SDR_M5STACK
    unsigned buttons = standalone_display_backend_buttons();
    int steps = standalone_display_backend_encoder();
    /* Suppress the individual releases when two keys become the menu chord. */
#if CONFIG_ESP_SDR_M5STACK_STICKC || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS || CONFIG_ESP_SDR_M5STACK_STICKS3
    if (buttons & 2) controls.suppress_release[0] = controls.suppress_release[2] = true;
#endif
    mhz = m5stack_controls_update(&controls, mhz, buttons,
                                  (uint32_t)(esp_timer_get_time() / 1000), pc_control);
    if (!pc_control && steps) {
        controls.last_input = (uint32_t)(esp_timer_get_time() / 1000);
        while (steps) {
            mhz = m5stack_menu_adjust(&controls, mhz, steps < 0 ? -1 : 1);
            steps += steps < 0 ? 1 : -1;
        }
    }
#if CONFIG_IDF_TARGET_ESP32S3
    if (controls.filter > 63) controls.filter = 63;
#endif
#elif (CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI)
    mhz = standalone_device_lilygo_ats_mini_poll(&controls, mhz, pc_control);
#else
    mhz = m5stack_controls_update(&controls, mhz, pc_control ? 0 : keyboard_buttons(),
                                  (uint32_t)(esp_timer_get_time() / 1000), pc_control);
#endif
    density_mode(mhz, pc_control);
    return mhz;
}

bool standalone_display_sync_rx(unsigned *agc, unsigned *gain, unsigned gain_max,
                           int *filter, bool *follow_span) {
    return m5stack_controls_sync_rx(&controls, agc, gain, gain_max, filter, follow_span);
}

unsigned standalone_display_rate(void) {
    return m5stack_rates[controls.rate];
}

unsigned standalone_display_span(void) {
    return m5stack_spans[controls.rate];
}

unsigned standalone_display_fft_points(void) {
    return m5stack_fft_sizes[controls.fft];
}

unsigned standalone_display_average_count(void) {
    return m5stack_average_counts[controls.average];
}

void standalone_display_draw(unsigned mhz, const uint8_t *bins, unsigned fft_points, bool pc_control,
                        bool valid, unsigned cpu_percent) {
    density_mode(mhz, pc_control);
    density_decay();
    char edges[3][12], cpu[12];
    static const char title[] = "ESP SDR M5";
    unsigned title_end = 12 + m5stack_text_width(title, &DejaVu9);
    const char *status = pc_control ? "PC" : controls.density_failed ? "RAM" :
                         valid ? (density ? "DENS" : "LIVE") : "WAIT";
#if CONFIG_ESP_SDR_M5STACK || CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI
    char menu_value[24];
    m5stack_menu_value(menu_value, sizeof(menu_value));
#endif
    spectrum_dbfs_t display_bins[PLOT_WIDTH];
    uint8_t *trace = (uint8_t *)display_bins;
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
#if CONFIG_ESP_SDR_M5STACK_PAPER_COLOR
    /* A full color refresh exceeds five seconds; alternate on completed refreshes. */
    board_status_text(cpu, sizeof(cpu), cpu_percent, (int64_t)m5stack_device_adapter_refresh_count() * 5000000);
#else
    board_status_text(cpu, sizeof(cpu), cpu_percent, esp_timer_get_time());
#endif
    if (bins && valid) {
        for (unsigned x = 0; x < PLOT_WIDTH; x++) display_bins[x] = SPECTRUM_DBFS_MIN;
        unsigned capture_span = span < 16 ? 16 : span;
        unsigned visible = fft_points * span / capture_span;
        unsigned first_bin = (fft_points - visible) / 2 + fft_points / 2;
        spectrum_dbfs_t frame_minimum = SPECTRUM_DBFS_MAX;
        if (density && !pc_control) density_begin_frame(mhz, fft_points);
        int last_density_column = -1;
        for (unsigned x = 0; x < PLOT_WIDTH; x++) {
            unsigned first = x * visible / PLOT_WIDTH, limit = (x + 1) * visible / PLOT_WIDTH;
            if (limit <= first) limit = first + 1;
            if (limit > visible) limit = visible;
            uint8_t peak = 0;
            for (unsigned j = first; j < limit; j++) {
                unsigned bin = first_bin + j;
                if (bin >= fft_points) bin -= fft_points;
                /* The radio produces negative FFT offsets for RF above the LO.
                 * Mirror the selected bins so higher RF is right. */
                bin = fft_points - 1 - bin;
                uint8_t code = bins[bin];
                spectrum_dbfs_t dbfs = spectrum_code_to_dbfs(code);
                if (dbfs < frame_minimum) frame_minimum = dbfs;
                if (code > peak) peak = code;
            }
            display_bins[x] = spectrum_code_to_dbfs(peak);
            if (density && !pc_control) {
                unsigned column = density_width < PLOT_WIDTH ? x * density_width / PLOT_WIDTH : x;
                if ((int)column != last_density_column) {
                    density_add_column(column, peak);
                    last_density_column = (int)column;
                }
            }
        }
        noise_floor = spectrum_noise_floor_update(&noise_floor_avg, frame_minimum);
        if (!pc_control && waterfall_frame++ % m5stack_wf_intervals[controls.wf_speed] == 0) {
            waterfall_head = (waterfall_head + 1) % WATERFALL_HEIGHT;
            memcpy(waterfall[waterfall_head], display_bins, sizeof(display_bins));
            waterfall_floor[waterfall_head] = noise_floor;
            if (waterfall_count < WATERFALL_HEIGHT) waterfall_count++;
        }
    }

    /* After saving waterfall levels, reuse the same bytes for the normal trace. */
    if (!density && bins && valid) {
        int graph_minimum = noise_floor - controls.y_pos;
        for (unsigned x = 0; x < PLOT_WIDTH; x++) {
            unsigned height = (spectrum_dbfs_to_display_value(display_bins[x], graph_minimum, 80) *
                               (SPECTRUM_BOTTOM - SPECTRUM_TOP)) / 255;
            trace[x] = SPECTRUM_BOTTOM - height;
        }
    }
    for (unsigned y = 0; y < SCREEN_HEIGHT; y++) {
        memset(row, 0, sizeof(row));
        if (y >= SPECTRUM_TOP && y <= SPECTRUM_BOTTOM) {
            bool horizontal_grid = (y - SPECTRUM_TOP) % 6 == 0;
            unsigned division = 0, next_grid = 0;
            for (unsigned x = 0; x < PLOT_WIDTH; x++) {
                bool vertical_grid = x == next_grid;
                if (vertical_grid) next_grid = (++division * (PLOT_WIDTH - 1) + 4) / 8;
                uint16_t color = y == SPECTRUM_BOTTOM ? 0x4208 :
                                 horizontal_grid || vertical_grid ? 0x2104 : 0;
                if (!density && bins && valid && y != SPECTRUM_BOTTOM) {
                    unsigned current = trace[x], previous = x ? trace[x - 1] : current;
                    /* SRdeck #2896ff stroke; #0050d2 at alpha 120/255 over black/grid. */
                    if (y >= (current < previous ? current : previous) &&
                        y <= (current > previous ? current : previous)) color = 0x2cbf;
                    else if (y > current) color = color ? 0x11ae : 0x012c;
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
        if (y < 12) {
            for (unsigned x = 4; x < title_end && x < SCREEN_WIDTH; x++)
                row[x] = __builtin_bswap16(COLOR_SRDECK_YELLOW_GREEN);
            m5stack_text_row(y, 6, 10, title, &DejaVu9, 0x0000, 0);
        }
#if CONFIG_ESP_SDR_M5STACK && M5_DISPLAY_LAYOUT_BASIC
        if (y >= 1 && y < 12) {
            text_row(y - 1, SCREEN_WIDTH / 2 - text_width(status) / 2,
                     status, pc_control ? 0xffe0 : 0xffff);
            text_row(y - 1, SCREEN_WIDTH - text_width(cpu) - 4, cpu, 0xffff);
        }
        if (y >= FREQUENCY_TOP && y < FREQUENCY_TOP + 12) {
            text_row(y - FREQUENCY_TOP, 4, edges[0], 0xffff);
            text_row(y - FREQUENCY_TOP, SCREEN_WIDTH / 2 - text_width(edges[1]) / 2, edges[1], 0xffff);
            text_row(y - FREQUENCY_TOP, SCREEN_WIDTH - text_width(edges[2]) - 4, edges[2], 0xffff);
        }
        if (!pc_control && controls.menu) m5stack_menu_row(y, menu_value);
#elif M5_DISPLAY_LAYOUT_STICK
        if (SCREEN_WIDTH < 200) {
            if (y >= 13 && y < 24)
                m5stack_text_row(y, (SCREEN_WIDTH - m5stack_text_width(status, &DejaVu9)) / 2,
                                 22, status, &DejaVu9, pc_control ? 0xffe0 : 0xffff, 0);
            if (y >= 25 && y < 36)
                m5stack_text_row(y, SCREEN_WIDTH - m5stack_text_width(cpu, &DejaVu9) - 4,
                                 34, cpu, &DejaVu9, 0xffff, 0);
        } else if (y >= 1 && y < 12) {
            text_row(y - 1, SCREEN_WIDTH / 2 - text_width(status) / 2,
                     status, pc_control ? 0xffe0 : 0xffff);
            text_row(y - 1, SCREEN_WIDTH - text_width(cpu) - 4, cpu, 0xffff);
        }
        if (y >= FREQUENCY_TOP && y < FREQUENCY_TOP + 12) {
            if (SCREEN_WIDTH < 100) {
                text_row(y - FREQUENCY_TOP, (SCREEN_WIDTH - text_width(edges[1])) / 2,
                         edges[1], 0xffff);
            } else {
                text_row(y - FREQUENCY_TOP, PLOT_LEFT, edges[0], 0xffff);
                text_row(y - FREQUENCY_TOP, SCREEN_WIDTH / 2 - text_width(edges[1]) / 2,
                         edges[1], 0xffff);
                text_row(y - FREQUENCY_TOP, SCREEN_WIDTH - text_width(edges[2]) - 3,
                         edges[2], 0xffff);
            }
        }
        if (!pc_control && controls.menu) m5stack_menu_row(y, menu_value);
#elif (CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI || CONFIG_ESP_SDR_M5STACK)
        if (y >= 1 && y < 12) {
            const GFXfont *font = &DejaVu12;
            m5stack_text_row(y - 1, SCREEN_WIDTH / 2 - m5stack_text_width(status, font) / 2,
                             9, status, font, pc_control ? 0xffe0 : 0xffff, 0);
            m5stack_text_row(y - 1, SCREEN_WIDTH - m5stack_text_width(cpu, font) - 4,
                             9, cpu, font, 0xffff, 0);
        }
        if (y >= FREQUENCY_TOP && y < FREQUENCY_TOP + 12) {
            text_row(y - FREQUENCY_TOP, 4, edges[0], 0xffff);
            const char *center = edges[1];
            uint16_t center_color = 0xffff;
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
            if (frequency_input_length) {
                center = frequency_input;
                center_color = COLOR_SRDECK_YELLOW_GREEN;
            }
#endif
            text_row(y - FREQUENCY_TOP, SCREEN_WIDTH / 2 - text_width(center) / 2, center,
                     center_color);
            text_row(y - FREQUENCY_TOP, SCREEN_WIDTH - text_width(edges[2]) - 4, edges[2], 0xffff);
        }
        if (!pc_control && controls.menu) m5stack_menu_row(y, menu_value);
#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
        if (keyboard_help_visible && y >= 87 && y < 111) {
            memset(row, 0, sizeof(row));
            text_row(y < 99 ? y - 87 : y - 99, 4,
                     y < 99 ? "<> FREQ 0-9 T STEP P SPAN" : "F FFT A AVE SHIFT SPC DENS",
                     COLOR_SRDECK_YELLOW_GREEN);
        }
#endif
#endif
        draw_global_tune_hints(y);
        standalone_display_backend_row(y, row);
    }
    standalone_display_backend_flush();
}
