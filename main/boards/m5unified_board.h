#pragma once
#include <stdbool.h>
#include <stdint.h>

#if CONFIG_ESP_SDR_M5STACK_CORES3
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define M5_BOARD_NAME "CORES3"
#elif CONFIG_ESP_SDR_M5STACK_DIN_METER
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135
#define M5_BOARD_NAME "DIN METER"
#elif CONFIG_ESP_SDR_M5STACK_STICKS3
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135
#define M5_BOARD_NAME "STICKS3"
#elif CONFIG_ESP_SDR_M5STACK_STICKC_PLUS
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135
#define M5_BOARD_NAME "STICKC PLUS"
#elif CONFIG_ESP_SDR_M5STACK_STICKC
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 80
#define M5_BOARD_NAME "STICKC"
#elif CONFIG_ESP_SDR_M5STACK_PAPER_COLOR
#define SCREEN_WIDTH 300
#define SCREEN_HEIGHT 200
#define M5_BOARD_NAME "PAPER COLOR"
#elif CONFIG_ESP_SDR_M5STACK_PAPERS3
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 270
#define M5_BOARD_NAME "PAPERS3"
#else
#error Select an M5Unified board
#endif

#if CONFIG_ESP_SDR_M5STACK_PAPER_COLOR || CONFIG_ESP_SDR_M5STACK_PAPERS3
#define M5_DISPLAY_SCALE 2
#define M5_EPAPER 1
#else
#define M5_DISPLAY_SCALE 1
#define M5_EPAPER 0
#endif

#if CONFIG_ESP_SDR_M5STACK_DIN_METER
#define M5_CONTROL_HINT "TURN +/- CLICK MENU"
#elif CONFIG_ESP_SDR_M5STACK_STICKC || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS || CONFIG_ESP_SDR_M5STACK_STICKS3
#define M5_CONTROL_HINT "A- B+ A+B MENU"
#else
#define M5_CONTROL_HINT "LEFT- MENU RIGHT+"
#endif

#define PLOT_LEFT 4
#define PLOT_WIDTH (SCREEN_WIDTH - 8)
#define SPECTRUM_TOP 14
#define SPECTRUM_BOTTOM (SCREEN_HEIGHT / 3)
#define FREQUENCY_TOP (SPECTRUM_BOTTOM + 3)
#define WATERFALL_TOP (FREQUENCY_TOP + 13)
#define WATERFALL_HEIGHT (SCREEN_HEIGHT - WATERFALL_TOP - 16)

#ifdef __cplusplus
extern "C" {
#endif
void m5unified_board_init(void);
unsigned m5unified_board_buttons(void);
int m5unified_board_encoder(void);
void m5unified_board_row(unsigned y, const uint16_t *pixels);
void m5unified_board_flush(void);
#ifdef __cplusplus
}
#endif
