"""Exercise five-second CPU/BAT switching and real board telemetry with mocked SDK I/O."""
import unittest

from host_test import COMPILER, ROOT, compile_run


def production():
    source = (ROOT / 'main/boards/board_battery.c').read_text()
    for header in ['sdkconfig.h', 'driver/i2c_master.h', 'esp_adc/adc_oneshot.h',
                   'esp_adc/adc_cali_scheme.h']:
        source = source.replace(f'#include "{header}"', '')
    return source


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class BoardBattery(unittest.TestCase):
    def test_five_second_switch_cache_invalid_readings_and_long_uptime(self):
        compile_run(r'''
#include <assert.h>
#include <string.h>
#include "board_battery.h"
static int percent = 73, reads;
int board_battery_percent(void) { reads++; return percent; }
static void check(int64_t us, unsigned cpu, const char *expected, int expected_reads) {
    char text[12]; board_status_text(text, sizeof(text), cpu, us);
    assert(!strcmp(text, expected) && reads == expected_reads);
}
int main(void) {
    check(0, 12, "CPU 12%", 0);
    check(4999999, 100, "CPU 100%", 0);
    check(5000000, 12, "BAT 73%", 1);
    percent = 0;
    check(9999999, 91, "BAT 73%", 1);
    check(10000000, 91, "CPU 91%", 1);
    check(15000000, 12, "BAT 0%", 2);
    percent = 100;
    check(25000000, 12, "BAT 100%", 3);
    percent = -1;
    check(35000000, 12, "CPU 12%", 4);
    check(35000001, 99, "CPU 99%", 4);
    percent = 101;
    check(45000000, 12, "CPU 12%", 5);
    percent = 22;
    check(INT64_C(5000000000000), 14, "CPU 14%", 5);
    check(INT64_C(5000005000000), 14, "BAT 22%", 6);
    return 0;
}
''')

    def test_calibrated_adc_pins_read_failures_and_voltage_bounds(self):
        stub = r'''
#include <assert.h>
#include <stddef.h>
#define ESP_OK 0
#define ADC_UNIT_1 1
#define ADC_ATTEN_DB_12 12
#define ADC_BITWIDTH_12 12
typedef int adc_unit_t, adc_channel_t;
typedef void *adc_oneshot_unit_handle_t, *adc_cali_handle_t;
typedef struct { int unit_id; } adc_oneshot_unit_init_cfg_t;
typedef struct { int atten, bitwidth; } adc_oneshot_chan_cfg_t;
typedef struct { int unit_id, chan, atten, bitwidth; } adc_cali_curve_fitting_config_t;
typedef struct { int unit_id, atten, bitwidth; } adc_cali_line_fitting_config_t;
static int fail, millivolts = 1850, deleted;
static int adc_oneshot_io_to_channel(int pin, adc_unit_t *unit, adc_channel_t *channel) {
    assert(pin == TEST_PIN); *unit = 1; *channel = 3; return 0;
}
static int adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *c, void **h) {
    assert(c->unit_id == 1); if (fail == 1) return -1; *h = (void *)1; return 0;
}
static int adc_oneshot_config_channel(void *h, int ch, const adc_oneshot_chan_cfg_t *c) {
    assert(h && ch == 3 && c->atten == 12 && c->bitwidth == 12); return fail == 2 ? -1 : 0;
}
static int adc_oneshot_del_unit(void *h) { deleted++; return 0; }
static int adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *c, void **h) {
    assert(c->unit_id == 1 && c->chan == 3 && c->atten == 12 && c->bitwidth == 12);
    if (fail == 3) return -1; *h = (void *)2; return 0;
}
static int adc_cali_create_scheme_line_fitting(const adc_cali_line_fitting_config_t *c, void **h) {
    assert(c->unit_id == 1 && c->atten == 12 && c->bitwidth == 12);
    if (fail == 3) return -1; *h = (void *)2; return 0;
}
static int adc_oneshot_read(void *h, int ch, int *raw) {
    assert(h && ch == 3); *raw = millivolts; return fail == 4 ? -1 : 0;
}
static int adc_cali_raw_to_voltage(void *h, int raw, int *mv) {
    assert(h); *mv = raw; return fail == 5 ? -1 : 0;
}
'''
        check = r'''
int main(void) {
#if TEST_RAW_FACTOR
    const int failures[] = {1, 2, 4};
    for (unsigned i = 0; i < 3; i++) { fail = failures[i]; assert(board_battery_percent() == -1); }
    assert(deleted == 1);
    fail = 0; millivolts = 2174; assert(board_battery_percent() == 50);
    millivolts = 1939; assert(board_battery_percent() == 0);
    millivolts = 2410; assert(board_battery_percent() == 100);
    millivolts = 0; assert(board_battery_percent() == -1);
    millivolts = 2821; assert(board_battery_percent() == -1);
    millivolts = 2057; assert(board_battery_percent() == 25);
#else
    for (fail = 1; fail <= 5; fail++) assert(board_battery_percent() == -1);
    assert(deleted == 1);
    fail = 0; assert(board_battery_percent() == 50);
    millivolts = 1650; assert(board_battery_percent() == 0);
    millivolts = 2050; assert(board_battery_percent() == 100);
    millivolts = 0; assert(board_battery_percent() == -1);
    millivolts = 2400; assert(board_battery_percent() == -1); /* USB rail is not VBAT. */
    millivolts = 1750; assert(board_battery_percent() == 25);
#endif
    return 0;
}
'''
        for profile, pin, raw_factor in [('M5STACK_CARDPUTER', 10, 0), ('M5STACK_CARDPUTER_ADV', 10, 0),
                                         ('M5STACK_STICKC_PLUS2', 38, 0), ('LILYGO_T_DECK', 4, 0),
                                         ('LILYGO_T_EMBED_SI4732', 4, 1), ('ATS_MINI', 4, 1)]:
            with self.subTest(profile=profile):
                compile_run(stub + production() + check, [f'CONFIG_ESP_SDR_{profile}=1',
                            f'TEST_PIN={pin}', f'TEST_RAW_FACTOR={raw_factor}',
                            f'ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED={int(pin != 38)}',
                            f'ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED={int(pin == 38)}'])

    def test_ip5306_axp192_axp2101_bq27220_and_sy6970(self):
        stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define ESP_OK 0
#define I2C_NUM_0 0
#define I2C_NUM_1 1
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 0
typedef void *i2c_master_dev_handle_t, *i2c_master_bus_handle_t;
typedef struct {int i2c_port, sda_io_num, scl_io_num, clk_source, glitch_ignore_cnt;
    struct {bool enable_internal_pullup;} flags;} i2c_master_bus_config_t;
typedef struct {int dev_addr_length, scl_speed_hz, device_address;} i2c_device_config_t;
static uint8_t registers[256];
static int read_failure, probe_failure = 1, missing_bus = 1;
static int i2c_master_get_bus_handle(int port, void **bus) {
    assert(port == TEST_PORT); *bus = (void *)1; return missing_bus ? -1 : 0;
}
static int i2c_new_master_bus(const i2c_master_bus_config_t *c, void **bus) {
    assert(c->i2c_port == 1 && c->sda_io_num == 21 && c->scl_io_num == 22);
    missing_bus = 0; *bus = (void *)1; return 0;
}
static int i2c_master_probe(void *bus, unsigned addr, int ms) {
    assert(bus && addr == TEST_ADDR && ms == 20); return probe_failure ? -1 : 0;
}
static int i2c_master_bus_add_device(void *bus, const i2c_device_config_t *c, void **dev) {
    assert(bus && c->device_address == TEST_ADDR); *dev = (void *)2; return 0;
}
static int i2c_master_transmit_receive(void *dev, const uint8_t *reg, size_t n, uint8_t *out, size_t size, int ms) {
    assert(dev && n == 1 && ms == 20); if (read_failure) return -1;
    memcpy(out, registers + *reg, size); return 0;
}
static int i2c_master_transmit(void *dev, const uint8_t *data, size_t size, int ms) {
    assert(dev && size == 2 && ms == 20); registers[data[0]] = data[1]; return 0;
}
'''
        check = r'''
int main(void) {
    assert(board_battery_percent() == -1);
    missing_bus = 0; assert(board_battery_percent() == -1);
    probe_failure = 0;
#if CONFIG_ESP_SDR_M5STACK_BASIC
    const unsigned patterns[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0x10};
    const int levels[] = {100, 75, 50, 25, 0, -1};
    for (unsigned i = 0; i < 6; i++) {
        registers[0x78] = patterns[i]; assert(board_battery_percent() == levels[i]);
    }
#elif CONFIG_ESP_SDR_M5STACK_CORE2
    registers[3] = 0x03; assert(board_battery_percent() == -1); /* No battery. */
    registers[1] = 0x20; registers[0x78] = 0xd2; registers[0x79] = 4;
    assert(board_battery_percent() == 50 && (registers[0x82] & 0x80)); /* 3700 mV. */
    registers[3] = 0x4a; registers[0] = 8; registers[0xa4] = 81;
    assert(board_battery_percent() == 81);
    registers[0xa4] = 255; assert(board_battery_percent() == -1);
    registers[0xa4] = 0; assert(board_battery_percent() == 0);
    registers[0] = 0; assert(board_battery_percent() == -1);
    registers[3] = 0xff; assert(board_battery_percent() == -1);
#elif CONFIG_ESP_SDR_LILYGO_T_DISPLAY_S3_PRO
    registers[0x0b] = 0x20; assert(board_battery_percent() == -1); /* VBUS present. */
    registers[0x0b] = 0; registers[0x0e] = 70;
    assert(board_battery_percent() == 50);
    registers[0x0e] = 0; assert(board_battery_percent() == -1);
#else
    registers[0x0a] = 8; registers[0x2c] = 67;
    assert(board_battery_percent() == 67);
    registers[0x2d] = 1; assert(board_battery_percent() == -1);
    registers[0x2d] = 0; registers[0x2c] = 101; assert(board_battery_percent() == -1);
    registers[0x2c] = 0; assert(board_battery_percent() == 0);
    registers[0x0a] = 0; assert(board_battery_percent() == -1);
#endif
    read_failure = 1; assert(board_battery_percent() == -1);
    return 0;
}
'''
        for profile, addr, port in [('M5STACK_BASIC', 0x75, 1), ('M5STACK_CORE2', 0x34, 1),
                                    ('LILYGO_T_DISPLAY_S3_PRO', 0x6a, 0), ('LILYGO_T_PAGER', 0x55, 0),
                                    ('LILYGO_T_EMBED_CC1101', 0x55, 0), ('LILYGO_T_EMBED_CC1101_PLUS', 0x55, 0),
                                    ('LILYGO_T_DECK_PRO', 0x55, 0), ('LILYGO_T_DECK_PRO_V11', 0x55, 0)]:
            with self.subTest(profile=profile):
                compile_run(stub + production() + check, [f'CONFIG_ESP_SDR_{profile}=1',
                            f'TEST_ADDR={addr}', f'TEST_PORT={port}'])

    def test_m5unified_voltage_validation_and_level_passthrough(self):
        source = (ROOT / 'main/boards/m5unified_board.cpp').read_text()
        start = source.index('int m5unified_board_battery_percent(')
        wrapper = source[start:source.index('\n}', start) + 2]
        stub = r'''
#include <assert.h>
static int mv = 3700, percent = 62;
static int voltage(void) { return mv; }
static int level(void) { return percent; }
static struct { struct { int (*getBatteryVoltage)(void); int (*getBatteryLevel)(void); } Power; }
    M5 = {{voltage, level}};
'''
        check = r'''
int main(void) {
    assert(board_battery_percent() == 62);
    mv = 0; assert(board_battery_percent() == -1);
    mv = 5000; assert(board_battery_percent() == -1);
    mv = 3400; percent = 0; assert(board_battery_percent() == 0);
    percent = -1; assert(board_battery_percent() == -1);
    percent = 100; assert(board_battery_percent() == 100);
    return 0;
}
'''
        compile_run(stub + wrapper + production() + check,
                    ['CONFIG_ESP_SDR_M5UNIFIED=1', 'CONFIG_ESP_SDR_M5STACK_CORES3=1'])
