#pragma once
#include <stdint.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 170
#define LCD_CS 6
#define LCD_DC 7
#define LCD_RST 5
#define LCD_BL 38
#define LCD_WR 8
#define LCD_RD 9
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 35
#define LCD_MADCTL 0xa0
#define ENCODER_A 2
#define ENCODER_B 1
#define MENU_BUTTON 21
#define LILYGO_NAME "ATS MINI"

void ats_mini_lcd_init(void);
void ats_mini_lcd_row(unsigned y, const uint16_t *pixels);
