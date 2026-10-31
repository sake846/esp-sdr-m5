#pragma once
#include <stdint.h>

void standalone_device_m5stack_basic_ili9341_init(void);
unsigned standalone_device_m5stack_basic_ili9341_buttons(void);
void standalone_device_m5stack_basic_ili9341_row(unsigned y, const uint16_t *pixels);
void standalone_device_m5stack_basic_ili9341_flush(void);
