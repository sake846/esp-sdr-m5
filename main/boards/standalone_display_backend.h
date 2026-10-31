#pragma once
#include <stdint.h>

#if CONFIG_ESP_SDR_M5STACK_BASIC && CONFIG_ESP_SDR_M5STACK_LCD_ILI9341
#include "standalone_device_m5stack_basic_ili9341.h"
static inline void standalone_display_backend_init(void) {
    standalone_device_m5stack_basic_ili9341_init();
}
static inline unsigned standalone_display_backend_buttons(void) {
    return standalone_device_m5stack_basic_ili9341_buttons();
}
static inline int standalone_display_backend_encoder(void) { return 0; }
static inline void standalone_display_backend_row(unsigned y, const uint16_t *pixels) {
    standalone_device_m5stack_basic_ili9341_row(y, pixels);
}
static inline void standalone_display_backend_flush(void) {
    standalone_device_m5stack_basic_ili9341_flush();
}
#elif CONFIG_ESP_SDR_M5STACK
#include "standalone_device_m5stack.h"
static inline void standalone_display_backend_init(void) {
    m5stack_device_adapter_init();
}
static inline unsigned standalone_display_backend_buttons(void) {
    return m5stack_device_adapter_buttons();
}
static inline int standalone_display_backend_encoder(void) {
    return m5stack_device_adapter_encoder();
}
static inline void standalone_display_backend_row(unsigned y, const uint16_t *pixels) {
    m5stack_device_adapter_row(y, pixels);
}
static inline void standalone_display_backend_flush(void) {
    m5stack_device_adapter_flush();
}
#elif CONFIG_ESP_SDR_LILYGO || CONFIG_ESP_SDR_ATS_MINI
#include "standalone_device_lilygo_ats_mini.h"
static inline void standalone_display_backend_init(void) {
    standalone_device_lilygo_ats_mini_init();
}
static inline unsigned standalone_display_backend_buttons(void) { return 0; }
static inline int standalone_display_backend_encoder(void) { return 0; }
static inline void standalone_display_backend_row(unsigned y, const uint16_t *pixels) {
    standalone_device_lilygo_ats_mini_display_row(y, pixels);
}
static inline void standalone_display_backend_flush(void) {
    standalone_device_lilygo_ats_mini_display_flush();
}
#else
#error Select a standalone display device
#endif
