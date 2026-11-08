#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "standalone_device_m5stack_display.h"

/* Shared display, battery and input adapter for M5Stack devices. */
#ifdef __cplusplus
extern "C" {
#endif
void m5stack_device_adapter_init(void);
int m5stack_device_adapter_battery_percent(void);
unsigned m5stack_device_adapter_buttons(void);
int m5stack_device_adapter_encoder(void);
void m5stack_device_adapter_row(unsigned y, const uint16_t *pixels);
void m5stack_device_adapter_flush(void);
void m5stack_device_adapter_power_off(void);
#ifdef __cplusplus
}
#endif
