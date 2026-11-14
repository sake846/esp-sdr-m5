#include "standalone_device_waveshare_amoled_206.h"
#include <M5GFX.h>
#include "lgfx/v1/panel/Panel_CO5300.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN(n) (UINT64_C(1) << (n))

static M5GFX display;
static lgfx::Bus_SPI display_bus;
static lgfx::Panel_CO5300 display_panel;
static i2c_master_bus_handle_t board_i2c;
static i2c_master_dev_handle_t touch_device, pmu_device;
static unsigned touch_buttons;
static uint32_t touch_next_poll;

static void add_i2c_device(uint8_t address, i2c_master_dev_handle_t *device) {
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(board_i2c, &config, device));
}

void waveshare_amoled_206_init(void) {
    const i2c_master_bus_config_t i2c_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_15,
        .scl_io_num = GPIO_NUM_14,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &board_i2c));
    ESP_ERROR_CHECK(i2c_master_probe(board_i2c, 0x34, 20)); /* AXP2101 */
    ESP_ERROR_CHECK(i2c_master_probe(board_i2c, 0x38, 20)); /* FT3168 */
    add_i2c_device(0x34, &pmu_device);
    add_i2c_device(0x38, &touch_device);

    const gpio_config_t gpio_configure = {
        .pin_bit_mask = PIN(18) | PIN(40),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_configure));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_40, 0));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_40, 1));
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_18, GPIO_MODE_INPUT));

    lgfx::Bus_SPI::config_t bus_config = {};
    bus_config.freq_write = 80000000;
    bus_config.freq_read = 1000000;
    bus_config.pin_sclk = GPIO_NUM_38;
    bus_config.pin_io0 = GPIO_NUM_4;
    bus_config.pin_io1 = GPIO_NUM_5;
    bus_config.pin_io2 = GPIO_NUM_6;
    bus_config.pin_io3 = GPIO_NUM_7;
    bus_config.spi_host = SPI2_HOST;
    bus_config.spi_mode = 0;
    bus_config.spi_3wire = true;
    display_bus.config(bus_config);
    ESP_ERROR_CHECK(display_bus.init() ? ESP_OK : ESP_FAIL);

    display_panel.bus(&display_bus);
    auto panel_config = display_panel.config();
    panel_config.pin_cs = GPIO_NUM_12;
    panel_config.pin_rst = GPIO_NUM_39;
    panel_config.pin_busy = -1;
    panel_config.memory_width = 432;
    panel_config.memory_height = 502;
    panel_config.panel_width = SCREEN_WIDTH;
    panel_config.panel_height = SCREEN_HEIGHT;
    panel_config.offset_x = 22;
    panel_config.offset_y = 0;
    panel_config.offset_rotation = 0;
    panel_config.readable = false;
    panel_config.invert = false;
    panel_config.bus_shared = false;
    display_panel.config(panel_config);
    ESP_ERROR_CHECK(display.init(&display_panel) ? ESP_OK : ESP_FAIL);
    display.setRotation(0);
    display.setSwapBytes(false);
    ESP_ERROR_CHECK(display.width() == SCREEN_WIDTH && display.height() == SCREEN_HEIGHT ? ESP_OK : ESP_FAIL);
    display.setBrightness(128);
}

unsigned waveshare_amoled_206_buttons(void) {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if ((int32_t)(now - touch_next_poll) >= 0) {
        touch_next_poll = now + 10;
        const uint8_t reg = 0x02;
        uint8_t data[5];
        if (i2c_master_transmit_receive(touch_device, &reg, 1, data, sizeof(data), 10) == ESP_OK) {
            touch_buttons = 0;
            if (data[0] & 0x0f) {
                unsigned x = ((data[1] & 0x0f) << 8) | data[2];
                unsigned y = ((data[3] & 0x0f) << 8) | data[4];
                if (x >= SCREEN_WIDTH && x < SCREEN_HEIGHT && y < SCREEN_WIDTH) {
                    unsigned swap = x;
                    x = y;
                    y = swap;
                }
                if (x < SCREEN_WIDTH && y < SCREEN_HEIGHT) {
                    unsigned zone = x * 3 / SCREEN_WIDTH;
                    touch_buttons = 1u << (zone > 2 ? 2 : zone);
                }
            }
        }
    }
    if (gpio_get_level(GPIO_NUM_18) == 0) touch_buttons |= 2;
    return touch_buttons;
}

void waveshare_amoled_206_row(unsigned y, const uint16_t *pixels) {
    display.pushImage(0, y, SCREEN_WIDTH, 1, pixels);
}

void waveshare_amoled_206_flush(void) {
    display.waitDisplay();
}

void waveshare_amoled_206_power_off(void) {
    const uint8_t shutdown[] = {0x10, 0x01};
    (void)i2c_master_transmit(pmu_device, shutdown, sizeof(shutdown), 20);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
}
