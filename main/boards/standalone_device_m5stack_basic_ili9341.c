/* Early M5Stack Basic ILI9341 panel and button hardware. */
#include "standalone_device_m5stack_basic_ili9341.h"
#include "standalone_device_m5stack_display.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN(n) (UINT64_C(1) << (n))
#define LCD_DC 27
#define LCD_RST 33
#define LCD_BL 32
#define LCD_CS 14
#define LCD_HOST SPI3_HOST

static spi_device_handle_t lcd;

static void lcd_write(bool data, const void *bytes, size_t size) {
    ESP_ERROR_CHECK(gpio_set_level(LCD_DC, data));
    spi_transaction_t transaction = {.length = size * 8, .tx_buffer = bytes};
    ESP_ERROR_CHECK(spi_device_polling_transmit(lcd, &transaction));
}

static void lcd_command(uint8_t command, const uint8_t *data, size_t size) {
    lcd_write(false, &command, 1);
    if (size) lcd_write(true, data, size);
}

void standalone_device_m5stack_basic_ili9341_init(void) {
    const gpio_config_t outputs = {
        .pin_bit_mask = PIN(4) | PIN(25) | PIN(LCD_DC) | PIN(LCD_RST) | PIN(LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&outputs));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_4, 1)); /* Deselect the SD card. */
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_25, 0)); /* Mute the speaker. */
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 0));

    const gpio_config_t buttons = {
        .pin_bit_mask = PIN(37) | PIN(38) | PIN(39),
        .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&buttons));

    ESP_ERROR_CHECK(gpio_set_level(LCD_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    /* Early Basic revisions use the reset pad's pull-up to select inversion. */
    ESP_ERROR_CHECK(gpio_set_direction(LCD_RST, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(LCD_RST, GPIO_PULLDOWN_ONLY));
    vTaskDelay(pdMS_TO_TICKS(1));
    bool invert = gpio_get_level(LCD_RST);
    ESP_ERROR_CHECK(gpio_set_pull_mode(LCD_RST, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RST, 1));
    ESP_ERROR_CHECK(gpio_set_direction(LCD_RST, GPIO_MODE_OUTPUT));
    vTaskDelay(pdMS_TO_TICKS(120));

    const spi_bus_config_t bus = {
        .mosi_io_num = 23, .miso_io_num = -1, .sclk_io_num = 18,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = SCREEN_WIDTH * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));
    const spi_device_interface_config_t device = {
        .clock_speed_hz = 40000000, .mode = 0, .spics_io_num = LCD_CS, .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &device, &lcd));

    lcd_command(0xc0, (const uint8_t[]){0x23}, 1);
    lcd_command(0xc1, (const uint8_t[]){0x10}, 1);
    lcd_command(0xc5, (const uint8_t[]){0x3e, 0x28}, 2);
    lcd_command(0x36, (const uint8_t[]){0xa8}, 1); /* ILI9341 portrait panel, landscape output. */
    lcd_command(0x3a, (const uint8_t[]){0x55}, 1);
    lcd_command(invert ? 0x21 : 0x20, NULL, 0);
    lcd_command(0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_command(0x29, NULL, 0);
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 1));
}

unsigned standalone_device_m5stack_basic_ili9341_buttons(void) {
    return (unsigned)!gpio_get_level(GPIO_NUM_39) |
           ((unsigned)!gpio_get_level(GPIO_NUM_38) << 1) |
           ((unsigned)!gpio_get_level(GPIO_NUM_37) << 2);
}

void standalone_device_m5stack_basic_ili9341_row(unsigned y, const uint16_t *pixels) {
    if (!y) {
        const uint8_t columns[] = {0, 0, (uint8_t)((SCREEN_WIDTH - 1) >> 8),
                                   (uint8_t)(SCREEN_WIDTH - 1)};
        lcd_command(0x2a, columns, sizeof(columns));
    }
    const uint8_t address[] = {(uint8_t)(y >> 8), (uint8_t)y,
                               (uint8_t)(y >> 8), (uint8_t)y};
    lcd_command(0x2b, address, sizeof(address));
    lcd_command(0x2c, NULL, 0);
    lcd_write(true, pixels, SCREEN_WIDTH * sizeof(*pixels));
}

void standalone_device_m5stack_basic_ili9341_flush(void) {}
