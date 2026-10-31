/* ATS-mini parallel LCD. Pinout/IDs: esp32-si4732/ats-mini, Display.h.
 * GC9307 register sequence adapted from LovyanGFX, Panel_GC9A01.hpp.
 *
 * Copyright (c) 2020 lovyan03 (https://github.com/lovyan03)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */
#include "standalone_device_ats_mini_lcd.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_io_i80.h"
#include "esp_lcd_panel_io.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>

static const gpio_num_t ats_data_pins[] = {39, 40, 41, 42, 45, 46, 47, 48};
static esp_lcd_panel_io_handle_t ats_lcd;
static SemaphoreHandle_t ats_row_done;
static bool ats_gc9307;

/* Read once before LCD_CAM owns the bus; the i80 driver supports writes only. */
static uint32_t ats_lcd_read_id(void) {
    ESP_ERROR_CHECK(gpio_set_level(LCD_DC, 0));
    ESP_ERROR_CHECK(gpio_set_level(LCD_CS, 0));
    for (unsigned i = 0; i < 8; i++)
        ESP_ERROR_CHECK(gpio_set_level(ats_data_pins[i], (0x04u >> i) & 1));
    ESP_ERROR_CHECK(gpio_set_level(LCD_WR, 0));
    esp_rom_delay_us(1);
    ESP_ERROR_CHECK(gpio_set_level(LCD_WR, 1));
    ESP_ERROR_CHECK(gpio_set_level(LCD_DC, 1));
    for (unsigned i = 0; i < 8; i++)
        ESP_ERROR_CHECK(gpio_set_direction(ats_data_pins[i], GPIO_MODE_INPUT));
    uint32_t id = 0;
    for (unsigned byte = 0; byte < 4; byte++) {
        ESP_ERROR_CHECK(gpio_set_level(LCD_RD, 0));
        esp_rom_delay_us(1);
        unsigned value = 0;
        for (unsigned bit = 0; bit < 8; bit++)
            value |= (unsigned)gpio_get_level(ats_data_pins[bit]) << bit;
        ESP_ERROR_CHECK(gpio_set_level(LCD_RD, 1));
        esp_rom_delay_us(1);
        if (byte) id = (id << 8) | value; /* Discard the dummy byte. */
    }
    ESP_ERROR_CHECK(gpio_set_level(LCD_CS, 1));
    for (unsigned i = 0; i < 8; i++)
        ESP_ERROR_CHECK(gpio_set_direction(ats_data_pins[i], GPIO_MODE_OUTPUT));
    return id;
}

static bool ats_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *context) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &woken);
    return woken == pdTRUE;
}

static void ats_command(uint8_t cmd, const uint8_t *data, size_t size) {
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(ats_lcd, cmd, data, size));
}

typedef struct { uint8_t cmd, length, data[14]; } ats_lcd_command_t;

static void ats_panel_init(uint32_t id) {
    static const ats_lcd_command_t st7789[] = {
        {0x13, 0, {0}},
        {0xb2, 5, {0x0b, 0x0b, 0, 0x33, 0x33}},
        {0xb7, 1, {0x75}}, {0xbb, 1, {0x28}}, {0xc0, 1, {0x2c}},
        {0xc2, 1, {0x01}}, {0xc3, 1, {0x1f}}, {0xc6, 1, {0x13}},
        {0xd0, 2, {0xa4, 0xa1}}, {0xd6, 1, {0xa1}},
        {0xe0, 14, {0xf0, 0x05, 0x0a, 0x06, 0x06, 0x03, 0x2b, 0x32, 0x43, 0x36, 0x11, 0x10, 0x2b, 0x32}},
        {0xe1, 14, {0xf0, 0x08, 0x0c, 0x0b, 0x09, 0x24, 0x2b, 0x22, 0x43, 0x38, 0x15, 0x16, 0x2f, 0x37}},
    };
    static const ats_lcd_command_t gc9307[] = {
        {0xfe, 0, {0}}, {0xef, 0, {0}},
        {0x85, 1, {0xc0}}, {0x86, 1, {0x98}}, {0x87, 1, {0x28}},
        {0x89, 1, {0x33}}, {0x8b, 1, {0x84}}, {0x8d, 1, {0x3b}},
        {0x8e, 1, {0x0f}}, {0x8f, 1, {0x70}}, {0xe8, 2, {0x13, 0x17}},
        {0xec, 3, {0x57, 0x07, 0xff}}, {0xed, 2, {0x18, 0x09}},
        {0xc9, 1, {0x10}}, {0xff, 1, {0x61}}, {0x99, 1, {0x3a}},
        {0x9d, 1, {0x43}}, {0x98, 1, {0x3e}}, {0x9c, 1, {0x4b}},
        {0xf0, 6, {0x06, 0x08, 0x08, 0x06, 0x05, 0x1d}},
        {0xf2, 6, {0x00, 0x01, 0x09, 0x07, 0x04, 0x23}},
        {0xf1, 6, {0x3b, 0x68, 0x66, 0x36, 0x35, 0x2f}},
        {0xf3, 6, {0x37, 0x6a, 0x66, 0x37, 0x35, 0x35}},
        {0xfa, 2, {0x80, 0x0f}}, {0xbe, 1, {0x11}}, {0xcb, 1, {0x02}},
        {0xcd, 1, {0x22}}, {0x9b, 1, {0xff}}, {0x35, 1, {0x00}}, {0x44, 2, {0, 0x0a}},
    };
    ats_gc9307 = id == 0x009307;
    if (!ats_gc9307) {
        ats_command(0x11, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    const ats_lcd_command_t *sequence = ats_gc9307 ? gc9307 : st7789;
    size_t count = ats_gc9307 ? sizeof(gc9307) / sizeof(gc9307[0]) : sizeof(st7789) / sizeof(st7789[0]);
    for (size_t i = 0; i < count; i++) ats_command(sequence[i].cmd, sequence[i].data, sequence[i].length);
    ats_command(0x36, (const uint8_t[]){ats_gc9307 ? 0xe8 : LCD_MADCTL}, 1);
    ats_command(0x3a, (const uint8_t[]){ats_gc9307 ? 0x05 : 0x55}, 1);
    ats_command(ats_gc9307 ? 0x20 : 0x21, NULL, 0);
    if (ats_gc9307) {
        ats_command(0x11, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    } else if (id == 0x858552) {
        ats_command(0x26, (const uint8_t[]){0x08}, 1);
        ats_command(0x55, (const uint8_t[]){0xb1}, 1);
    }
    ats_command(0x29, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
}

void ats_mini_lcd_init(void) {
    uint64_t pins = (UINT64_C(1) << LCD_CS) | (UINT64_C(1) << LCD_DC) | (UINT64_C(1) << LCD_RST) |
                    (UINT64_C(1) << LCD_BL) | (UINT64_C(1) << LCD_WR) | (UINT64_C(1) << LCD_RD);
    for (unsigned i = 0; i < 8; i++) pins |= UINT64_C(1) << ats_data_pins[i];
    const gpio_config_t outputs = {.pin_bit_mask = pins, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&outputs));
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 0));
    ESP_ERROR_CHECK(gpio_set_level(LCD_CS, 1));
    ESP_ERROR_CHECK(gpio_set_level(LCD_WR, 1));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RD, 1));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(120));
    uint32_t id = ats_lcd_read_id();

    const esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = LCD_DC, .wr_gpio_num = LCD_WR, .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {39, 40, 41, 42, 45, 46, 47, 48}, .bus_width = 8,
        .max_transfer_bytes = SCREEN_WIDTH * sizeof(uint16_t), .dma_burst_size = 4,
    };
    esp_lcd_i80_bus_handle_t bus;
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &bus));
    ats_row_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(ats_row_done ? ESP_OK : ESP_ERR_NO_MEM);
    const esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_CS, .pclk_hz = 20000000, .trans_queue_depth = 1,
        .on_color_trans_done = ats_color_done, .user_ctx = ats_row_done,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .dc_levels = {.dc_idle_level = 0, .dc_cmd_level = 0, .dc_dummy_level = 0, .dc_data_level = 1},
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &io_config, &ats_lcd));
    ats_panel_init(id);
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL, 1));
}

void ats_mini_lcd_row(unsigned y, const uint16_t *pixels) {
    if (!y) {
        if (ats_gc9307) ats_command(0x2b, (const uint8_t[]){0xff, 0xff, 0xff, 0xff}, 4);
        unsigned last = SCREEN_WIDTH - 1;
        ats_command(0x2a, (const uint8_t[]){0, 0, last >> 8, last & 255}, 4);
    }
    unsigned address = y + LCD_Y_OFFSET;
    ats_command(0x2b, (const uint8_t[]){address >> 8, address & 255, address >> 8, address & 255}, 4);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(ats_lcd, 0x2c, pixels, SCREEN_WIDTH * sizeof(*pixels)));
    /* The shared renderer reuses its row immediately; wait until DMA releases it. */
    ESP_ERROR_CHECK(xSemaphoreTake(ats_row_done, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
}
