/* Compact S3 display/control glue. Wiring: docs/lilygo.md and docs/ats-mini.md. */
#include "lilygo.h"
#include "lilygo_controls.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#define PIN(n) (UINT64_C(1) << (n))
#if !CONFIG_ESP_SDR_ATS_MINI
static spi_device_handle_t display;
#endif
static i2c_master_dev_handle_t keyboard;

static void output(unsigned pin, unsigned level) {
    ESP_ERROR_CHECK(gpio_set_level(pin, level));
    ESP_ERROR_CHECK(gpio_set_direction(pin, GPIO_MODE_OUTPUT));
}

#if !CONFIG_ESP_SDR_ATS_MINI
static void write_display(bool data, const void *bytes, size_t count) {
    ESP_ERROR_CHECK(gpio_set_level(LCD_DC, data));
    spi_transaction_t transaction = {.length = count * 8, .tx_buffer = bytes};
    ESP_ERROR_CHECK(spi_device_polling_transmit(display, &transaction));
}

static void command(uint8_t cmd, const uint8_t *bytes, size_t count) {
    write_display(false, &cmd, 1);
    if (count) write_display(true, bytes, count);
}

static void reg_write(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_transmit(device, data, sizeof(data), 20));
}
#endif

static void input_init(void) {
#if !CONFIG_ESP_SDR_ATS_MINI
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0, .sda_io_num = BOARD_SDA, .scl_io_num = BOARD_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&config, &bus));
#if CONFIG_ESP_SDR_LILYGO_T_PAGER
    i2c_master_dev_handle_t expander;
    const i2c_device_config_t io_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x20, .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &io_config, &expander));
    /* XL9555: keep unused radios, amplifier and SD off; release keyboard reset. */
    reg_write(expander, 2, 0x80);
    reg_write(expander, 3, 0x01);
    reg_write(expander, 6, 0x40);
    reg_write(expander, 7, 0xee);
    vTaskDelay(pdMS_TO_TICKS(20));
    reg_write(expander, 2, 0x84);
    vTaskDelay(pdMS_TO_TICKS(20));
#endif
#if CONFIG_ESP_SDR_LILYGO_T_PAGER || CONFIG_ESP_SDR_LILYGO_T_DECK_PRO || CONFIG_ESP_SDR_LILYGO_T_DECK_PRO_V11
    const i2c_device_config_t keyboard_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x34, .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &keyboard_config, &keyboard));
    /* TCA8418: four rows, ten columns; polling drains its bounded event FIFO. */
    reg_write(keyboard, 0x1d, 0x0f);
    reg_write(keyboard, 0x1e, 0xff);
    reg_write(keyboard, 0x1f, 0x03);
    reg_write(keyboard, 0x02, 0x1f);
    reg_write(keyboard, 0x01, 0x03);
#elif CONFIG_ESP_SDR_LILYGO_T_DECK
    const i2c_device_config_t keyboard_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x55, .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &keyboard_config, &keyboard));
#endif
#endif
    const gpio_config_t button = {
        .pin_bit_mask = PIN(MENU_BUTTON), .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button));
}

#ifdef ENCODER_A
static portMUX_TYPE encoder_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile int encoder_count;
static unsigned encoder_state;

static void encoder_interrupt(void *unused) {
    unsigned state = (unsigned)gpio_get_level(ENCODER_A) | ((unsigned)gpio_get_level(ENCODER_B) << 1);
    portENTER_CRITICAL_ISR(&encoder_lock);
    int delta = lilygo_encoder_delta(encoder_state, state);
    if (encoder_count + delta >= -128 && encoder_count + delta <= 128) encoder_count += delta;
    encoder_state = state;
    portEXIT_CRITICAL_ISR(&encoder_lock);
}

static void encoder_init(void) {
    const gpio_config_t config = {
        .pin_bit_mask = PIN(ENCODER_A) | PIN(ENCODER_B), .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    encoder_state = (unsigned)gpio_get_level(ENCODER_A) | ((unsigned)gpio_get_level(ENCODER_B) << 1);
    esp_err_t result = gpio_install_isr_service(0);
    ESP_ERROR_CHECK(result == ESP_ERR_INVALID_STATE ? ESP_OK : result);
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENCODER_A, encoder_interrupt, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENCODER_B, encoder_interrupt, NULL));
}
#endif

unsigned lilygo_poll(m5stack_controls_t *s, unsigned mhz, bool pc_control) {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    bool was_long = s->b_long;
    mhz = m5stack_controls_update(s, mhz, gpio_get_level(MENU_BUTTON) ? 0 : 2, now, pc_control);
    if (!pc_control && !was_long && s->b_long && (s->stable & 2)) s->menu = M5_MENU_NONE;
#ifdef ENCODER_A
    portENTER_CRITICAL(&encoder_lock);
    int steps = encoder_count / 4;
    encoder_count = pc_control ? 0 : encoder_count % 4;
    portEXIT_CRITICAL(&encoder_lock);
    if (!pc_control && steps) {
        s->last_input = now;
        while (steps) {
            mhz = m5stack_menu_adjust(s, mhz, steps < 0 ? -1 : 1);
            steps += steps < 0 ? 1 : -1;
        }
    }
#endif
    static uint32_t next_keyboard;
    if (keyboard && (int32_t)(now - next_keyboard) >= 0) {
        next_keyboard = now + 10;
        uint8_t count = 0;
#if CONFIG_ESP_SDR_LILYGO_T_DECK
        if (i2c_master_receive(keyboard, &count, 1, 10) == ESP_OK && count) {
            mhz = lilygo_key(s, mhz, (char)count, pc_control);
            if (!pc_control) s->last_input = now;
        }
#else
        uint8_t reg = 0x03;
        if (i2c_master_transmit_receive(keyboard, &reg, 1, &count, 1, 10) == ESP_OK) {
            count &= 0x0f;
            for (unsigned i = 0; i < count && i < 10; i++) {
                uint8_t event;
                reg = 0x04;
                if (i2c_master_transmit_receive(keyboard, &reg, 1, &event, 1, 10) != ESP_OK) break;
                char key = lilygo_tca_key(event);
                if (key) {
                    mhz = lilygo_key(s, mhz, key, pc_control);
                    if (!pc_control) s->last_input = now;
                }
            }
        }
#endif
    }
    /* The S3 analog filter has six programmable bits. */
    if (s->filter > 63) s->filter = 63;
    return mhz;
}

#if LILYGO_EPAPER
/* UC8253 sequence follows GxEPD2_310_GDEQ031T10 by Jean-Marc Zingg (GPLv3).
 * Display images live in PSRAM, outside the RF capture SRAM aperture. */
#define EPAPER_BYTES (SCREEN_WIDTH * SCREEN_HEIGHT / 8)
static uint8_t *epaper_frame, *epaper_pending, *epaper_tx;
static SemaphoreHandle_t epaper_lock;
static TaskHandle_t epaper_task;

static bool epaper_wait(void) {
    vTaskDelay(pdMS_TO_TICKS(10));
    int64_t deadline = esp_timer_get_time() + 10000000;
    while (!gpio_get_level(37)) {
        if (esp_timer_get_time() >= deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

static void epaper_plane(uint8_t plane) {
    command(plane, NULL, 0);
    uint8_t chunk[240] __attribute__((aligned(4)));
    for (unsigned i = 0; i < EPAPER_BYTES; i += sizeof(chunk)) {
        memcpy(chunk, epaper_tx + i, sizeof(chunk));
        write_display(true, chunk, sizeof(chunk));
    }
}

static void epaper_worker(void *unused) {
    unsigned refresh = 0;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xSemaphoreTake(epaper_lock, portMAX_DELAY);
        memcpy(epaper_tx, epaper_pending, EPAPER_BYTES);
        xSemaphoreGive(epaper_lock);
        bool full = refresh % 6 == 0;
        command(0x00, (const uint8_t[]){0x1e, 0x0d}, 2);
        vTaskDelay(pdMS_TO_TICKS(2));
        command(0x00, (const uint8_t[]){0x1f, 0x0d}, 2);
        if (!full) command(0x91, NULL, 0);
        command(0x90, (const uint8_t[]){0, 239, 0, 0, 1, 63, 1}, 7);
        if (full) epaper_plane(0x10);
        epaper_plane(0x13);
        command(0xe0, (const uint8_t[]){0x02}, 1);
        command(0xe5, (const uint8_t[]){full ? 0x5a : 0x79}, 1);
        command(0x50, (const uint8_t[]){full ? 0x97 : 0xd7}, 1);
        command(0x04, NULL, 0);
        bool ok = epaper_wait();
        if (ok) {
            command(0x12, NULL, 0);
            ok = epaper_wait();
        }
        if (ok) {
            epaper_plane(0x10);
            if (!full) command(0x92, NULL, 0);
            command(0x02, NULL, 0);
            ok = epaper_wait();
        }
        if (!ok) vTaskSuspend(NULL); /* A stuck BUSY must not stop USB or tuning. */
        refresh++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

void lilygo_init(void) {
#if CONFIG_ESP_SDR_ATS_MINI
    output(10, 0); /* Speaker amplifier off. */
    output(3, 1);  /* Clamp/mute analog audio. */
    output(16, 0); /* SI4732 reset; do not pull up its unpowered I2C bus. */
    output(15, 0); /* SI4732 LDO off. */
#elif CONFIG_ESP_SDR_LILYGO_T_EMBED_SI4732
    output(46, 1);
    output(16, 0); /* Hold SI4732 in reset; the ESP32 is the RF source. */
    output(17, 1); /* Mute the SI4732 analog amplifier. */
    output(39, 1); /* SD chip select. */
#elif CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101 || CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101_PLUS
    output(15, 1);
    output(12, 1); /* CC1101 CS. */
    output(13, 1); /* SD CS. */
#if CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101_PLUS
    output(44, 1); /* nRF24 CS. */
    output(43, 0); /* nRF24 CE. */
#endif
#elif CONFIG_ESP_SDR_LILYGO_T_DECK
    output(9, 1); /* LoRa CS, before enabling the peripheral rail. */
    output(39, 1); /* SD CS. */
    output(LCD_CS, 1);
    output(10, 1);
    vTaskDelay(pdMS_TO_TICKS(500)); /* Keyboard controller power-up. */
#elif CONFIG_ESP_SDR_LILYGO_T_PAGER
    output(21, 1); /* Shared SPI: SD, LoRa and NFC must be deselected. */
    output(36, 1);
    output(39, 1);
    output(47, 1);
#else
    output(3, 1); /* LoRa CS. */
    output(48, 1); /* SD CS. */
    output(39, 0); /* GPS power. */
    output(41, 0); /* Modem power. */
    output(46, 0); /* LoRa power. */
    output(42, 0); /* Keyboard light. */
#endif
    input_init();
#ifdef ENCODER_A
    encoder_init();
#endif
#if CONFIG_ESP_SDR_ATS_MINI
    ats_mini_lcd_init();
#else
    output(LCD_DC, 0);
    output(LCD_CS, 1);
#if LCD_BL >= 0
    output(LCD_BL, 0);
#endif
#if LCD_RST >= 0
    output(LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    output(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
#endif
    const spi_bus_config_t bus = {
        .mosi_io_num = LCD_MOSI, .miso_io_num = -1, .sclk_io_num = LCD_SCLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = SCREEN_WIDTH * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    const spi_device_interface_config_t device = {
        .clock_speed_hz = LILYGO_EPAPER ? 10000000 : 40000000,
        .mode = 0, .spics_io_num = LCD_CS, .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &device, &display));
#if LILYGO_EPAPER
    const gpio_config_t busy = {.pin_bit_mask = PIN(37), .mode = GPIO_MODE_INPUT};
    ESP_ERROR_CHECK(gpio_config(&busy));
    epaper_frame = heap_caps_calloc(3, EPAPER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(epaper_frame ? ESP_OK : ESP_ERR_NO_MEM);
    epaper_pending = epaper_frame + EPAPER_BYTES;
    epaper_tx = epaper_pending + EPAPER_BYTES;
    memset(epaper_frame, 0xff, 3 * EPAPER_BYTES);
    epaper_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(epaper_lock ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(epaper_worker, "epaper", 3072, NULL, 1, &epaper_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
#else
    command(0x01, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(150));
    command(0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
#if CONFIG_ESP_SDR_LILYGO_T_PAGER
    command(0xf0, (const uint8_t[]){0xc3}, 1);
    command(0xf0, (const uint8_t[]){0x96}, 1);
    command(0xb4, (const uint8_t[]){0x01}, 1);
    command(0xb6, (const uint8_t[]){0x80, 0x02, 0x3b}, 3);
    command(0xe8, (const uint8_t[]){0x40, 0x8a, 0, 0, 0x29, 0x19, 0xa5, 0x33}, 8);
    command(0xc1, (const uint8_t[]){0x06}, 1);
    command(0xc2, (const uint8_t[]){0xa7}, 1);
    command(0xc5, (const uint8_t[]){0x18}, 1);
    command(0xe0, (const uint8_t[]){0xf0, 0x09, 0x0b, 0x06, 0x04, 0x15, 0x2f, 0x54, 0x42, 0x3c, 0x17, 0x14, 0x18, 0x1b}, 14);
    command(0xe1, (const uint8_t[]){0xe0, 0x09, 0x0b, 0x06, 0x04, 0x03, 0x2b, 0x43, 0x42, 0x3b, 0x16, 0x14, 0x17, 0x1b}, 14);
    command(0xf0, (const uint8_t[]){0x3c}, 1);
    command(0xf0, (const uint8_t[]){0x69}, 1);
#else
    command(0xb2, (const uint8_t[]){0x0b, 0x0b, 0, 0x33, 0x33}, 5);
    command(0xb7, (const uint8_t[]){0x75}, 1);
    command(0xbb, (const uint8_t[]){0x28}, 1);
    command(0xc0, (const uint8_t[]){0x2c}, 1);
    command(0xc2, (const uint8_t[]){0x01}, 1);
    command(0xc3, (const uint8_t[]){0x1f}, 1);
    command(0xc4, (const uint8_t[]){0x20}, 1);
    command(0xc6, (const uint8_t[]){0x13}, 1);
    command(0xd0, (const uint8_t[]){0xa4, 0xa1}, 2);
#endif
    command(0x36, (const uint8_t[]){LCD_MADCTL}, 1);
    command(0x3a, (const uint8_t[]){0x55}, 1);
    command(0x21, NULL, 0);
    command(0x29, NULL, 0);
    output(LCD_BL, 1);
#endif
#endif
}

void lilygo_display_row(unsigned y, const uint16_t *pixels) {
#if CONFIG_ESP_SDR_ATS_MINI
    ats_mini_lcd_row(y, pixels);
#elif LILYGO_EPAPER
    for (unsigned x = 0; x < SCREEN_WIDTH; x++) {
        uint8_t bit = 0x80u >> (x & 7);
        uint8_t *byte = &epaper_frame[y * (SCREEN_WIDTH / 8) + x / 8];
        if (lilygo_epaper_ink(__builtin_bswap16(pixels[x]), x, y)) *byte &= ~bit;
        else *byte |= bit;
    }
#else
    if (!y) {
        unsigned end = LCD_X_OFFSET + SCREEN_WIDTH - 1;
        command(0x2a, (const uint8_t[]){LCD_X_OFFSET >> 8, LCD_X_OFFSET & 255, end >> 8, end & 255}, 4);
    }
    unsigned address = y + LCD_Y_OFFSET;
    command(0x2b, (const uint8_t[]){address >> 8, address & 255, address >> 8, address & 255}, 4);
    command(0x2c, NULL, 0);
    write_display(true, pixels, SCREEN_WIDTH * sizeof(*pixels));
#endif
}

void lilygo_display_flush(void) {
#if LILYGO_EPAPER
    xSemaphoreTake(epaper_lock, portMAX_DELAY);
    memcpy(epaper_pending, epaper_frame, EPAPER_BYTES);
    xSemaphoreGive(epaper_lock);
    xTaskNotifyGive(epaper_task);
#endif
}
