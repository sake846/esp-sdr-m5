#pragma once
#include "standalone_display_controls.h"

#if CONFIG_ESP_SDR_ATS_MINI
#include "standalone_device_ats_mini_lcd.h"
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
#define BOARD_TOUCH_RST 13
#define BOARD_TOUCH_INT 21
#define MENU_BUTTON 12
#define LILYGO_NAME "T DISPLAY S3 PRO"
#elif CONFIG_ESP_SDR_LILYGO_T_WATCH_S3 || CONFIG_ESP_SDR_LILYGO_T_WATCH_2020
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define LCD_SCLK 18
#define LCD_RST -1
/* Match the LILYGO display orientation and native FT6336 coordinates. */
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 80
#if CONFIG_ESP_SDR_LILYGO_T_WATCH_2020
#define LCD_MADCTL 0xc8
#else
#define LCD_MADCTL 0xc0
#endif
#define LILYGO_FOOTER_HEIGHT 32
#if CONFIG_ESP_SDR_LILYGO_T_WATCH_S3
#define LCD_MOSI 13
#define LCD_CS 12
#define LCD_DC 38
#define LCD_BL 45
#define BOARD_SDA 10
#define BOARD_SCL 11
#define TOUCH_SDA 39
#define TOUCH_SCL 40
#define WATCH_PMU_ADDR 0x34
#define LILYGO_NAME "T WATCH S3"
#else
#define LCD_MOSI 19
#define LCD_CS 5
#define LCD_DC 27
#define BOARD_SDA 21
#define BOARD_SCL 22
#define TOUCH_SDA 23
#define TOUCH_SCL 32
#define WATCH_PMU_ADDR 0x35
#if CONFIG_ESP_SDR_LILYGO_T_WATCH_2020_V1
#define LCD_BL 12
#define LILYGO_NAME "T WATCH 2020 V1"
#elif CONFIG_ESP_SDR_LILYGO_T_WATCH_2020_V2
#define LCD_BL 25
#define LILYGO_NAME "T WATCH 2020 V2"
#else
#define LCD_BL 15
#define TOUCH_RST 14
#define LILYGO_NAME "T WATCH 2020 V3"
#endif
#endif
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
#define LCD_MADCTL 0xa0
#define BOARD_SDA 18
#define BOARD_SCL 8
#define ENCODER_A 1
#define ENCODER_B 2
#define ENCODER_DIVIDER 2
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
#define LCD_MADCTL 0xa0
#define BOARD_SDA 8
#define BOARD_SCL 18
#define ENCODER_A 5
#define ENCODER_B 4
#define ENCODER_DIVIDER 2
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
#define TDECK_TRACKBALL_LEFT 1
#define TDECK_TRACKBALL_RIGHT 2
#define TDECK_TOUCH_INT 16
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
#ifndef LILYGO_FOOTER_HEIGHT
#define LILYGO_FOOTER_HEIGHT 17
#endif
#define WATERFALL_HEIGHT (SCREEN_HEIGHT - WATERFALL_TOP - LILYGO_FOOTER_HEIGHT)

#ifndef LILYGO_EPAPER
#define LILYGO_EPAPER 0
#endif

void standalone_device_lilygo_ats_mini_init(void);
unsigned standalone_device_lilygo_ats_mini_poll(m5stack_controls_t *controls, unsigned mhz, bool pc_control);
void standalone_device_lilygo_ats_mini_display_row(unsigned y, const uint16_t *pixels);
void standalone_device_lilygo_ats_mini_display_flush(void);
void standalone_device_lilygo_ats_mini_power_off(void);
#if CONFIG_ESP_SDR_LILYGO_T_WATCH_S3 || CONFIG_ESP_SDR_LILYGO_T_WATCH_2020
enum {
    LILYGO_WATCH_TOUCH = 1,
    LILYGO_WATCH_TOUCH_LEFT = 4, LILYGO_WATCH_TOUCH_CENTER = 8,
    LILYGO_WATCH_TOUCH_RIGHT = 16, LILYGO_WATCH_TOUCH_TOP_LEFT = 32,
    LILYGO_WATCH_TOUCH_TOP_RIGHT = 64, LILYGO_WATCH_SWIPE_LEFT = 128,
    LILYGO_WATCH_SWIPE_RIGHT = 256,
};
unsigned standalone_device_lilygo_ats_mini_watch_events(void);
bool standalone_device_lilygo_ats_mini_watch_read_datetime(unsigned *year, unsigned *month, unsigned *day,
                                                             unsigned *hour, unsigned *minute, unsigned *second,
                                                             unsigned *weekday);
bool standalone_device_lilygo_ats_mini_watch_write_datetime(unsigned year, unsigned month, unsigned day,
                                                              unsigned hour, unsigned minute, unsigned second);
void standalone_device_lilygo_ats_mini_watch_display(bool awake);
void standalone_device_lilygo_ats_mini_watch_sleep(void);
#endif
