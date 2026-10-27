#pragma once
#include "m5stack_basic_controls.h"

#if CONFIG_ESP_SDR_ATS_MINI
#include "ats_mini.h"
#elif CONFIG_ESP_SDR_LILYGO_T_DISPLAY_S3_PRO
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 222
#define LCD_MOSI 17
#define LCD_SCLK 18
#define LCD_CS 39
#define LCD_DC 9
#define LCD_RST 47
#define LCD_BL 48
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 49
#define LCD_MADCTL 0x68
#define BOARD_SDA 5
#define BOARD_SCL 6
#define MENU_BUTTON 12
#define LILYGO_NAME "T DISPLAY S3 PRO"
#elif CONFIG_ESP_SDR_LILYGO_T_PAGER
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 222
#define LCD_MOSI 34
#define LCD_SCLK 35
#define LCD_CS 38
#define LCD_DC 37
#define LCD_RST -1
#define LCD_BL 42
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 49
#define LCD_MADCTL 0xe8
#define BOARD_SDA 3
#define BOARD_SCL 2
#define ENCODER_A 40
#define ENCODER_B 41
#define MENU_BUTTON 7
#define LILYGO_NAME "T PAGER"
#elif CONFIG_ESP_SDR_LILYGO_T_EMBED_SI4732
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 170
#define LCD_MOSI 11
#define LCD_SCLK 12
#define LCD_CS 10
#define LCD_DC 13
#define LCD_RST 9
#define LCD_BL 15
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 35
#define LCD_MADCTL 0x60
#define BOARD_SDA 18
#define BOARD_SCL 8
#define ENCODER_A 2
#define ENCODER_B 1
#define MENU_BUTTON 0
#define LILYGO_NAME "T EMBED SI4732"
#elif CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101 || CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101_PLUS
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 170
#define LCD_MOSI 9
#define LCD_SCLK 11
#define LCD_CS 41
#define LCD_DC 16
#define LCD_RST -1
#define LCD_BL 21
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 35
#define LCD_MADCTL 0x60
#define BOARD_SDA 8
#define BOARD_SCL 18
#define ENCODER_A 4
#define ENCODER_B 5
#define MENU_BUTTON 0
#if CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101_PLUS
#define LILYGO_NAME "T EMBED CC PLUS"
#else
#define LILYGO_NAME "T EMBED CC1101"
#endif
#elif CONFIG_ESP_SDR_LILYGO_T_DECK
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define LCD_MOSI 41
#define LCD_SCLK 40
#define LCD_CS 12
#define LCD_DC 11
#define LCD_RST -1
#define LCD_BL 42
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 0
#define LCD_MADCTL 0x60
#define BOARD_SDA 18
#define BOARD_SCL 8
#define MENU_BUTTON 0
#define LILYGO_NAME "T DECK"
#else /* T-Deck Pro: GDEQ031T10 / UC8253 e-paper, native portrait orientation. */
#define LILYGO_EPAPER 1
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define LCD_MOSI 33
#define LCD_SCLK 36
#define LCD_CS 34
#define LCD_DC 35
#define LCD_BL -1
#if CONFIG_ESP_SDR_LILYGO_T_DECK_PRO_V11
#define LCD_RST 16
#else
#define LCD_RST -1
#endif
#define BOARD_SDA 13
#define BOARD_SCL 14
#define MENU_BUTTON 0
#define LILYGO_NAME "T DECK PRO"
#endif

#define PLOT_LEFT 4
#define PLOT_WIDTH (SCREEN_WIDTH - 8)
#define SPECTRUM_TOP 14
#if CONFIG_SPIRAM
#define SPECTRUM_BOTTOM (SCREEN_HEIGHT / 3)
#else
/* Keep 64 waterfall rows in internal RAM and use the rest for the trace. */
#define SPECTRUM_BOTTOM (SCREEN_HEIGHT - 96)
#endif
#define FREQUENCY_TOP (SPECTRUM_BOTTOM + 3)
#define WATERFALL_TOP (FREQUENCY_TOP + 13)
#define WATERFALL_HEIGHT (SCREEN_HEIGHT - WATERFALL_TOP - 16)

#ifndef LILYGO_EPAPER
#define LILYGO_EPAPER 0
#endif

void lilygo_init(void);
unsigned lilygo_poll(m5stack_controls_t *controls, unsigned mhz, bool pc_control);
void lilygo_display_row(unsigned y, const uint16_t *pixels);
void lilygo_display_flush(void);
