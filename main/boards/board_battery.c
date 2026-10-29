/* Battery telemetry only: leave charging and power-rail settings alone. */
#include "sdkconfig.h"
#include "board_battery.h"

#if CONFIG_ESP_SDR_M5UNIFIED
#include "m5unified_board.h"

int board_battery_percent(void) {
    return m5unified_board_battery_percent();
}

#else
static inline int battery_voltage_percent(int mv) {
    if (mv < 2500 || mv > 4400) return -1;
    /* Same single-cell estimate as M5Unified; voltage is not a fuel gauge. */
    int percent = (mv - 3300) / 8;
    return percent < 0 ? 0 : percent > 100 ? 100 : percent;
}

#if CONFIG_ESP_SDR_M5STACK_CARDPUTER || CONFIG_ESP_SDR_M5STACK_CARDPUTER_ADV
#define BATTERY_ADC_PIN 10
#elif CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
#define BATTERY_ADC_PIN 38
#elif CONFIG_ESP_SDR_LILYGO_T_DECK || CONFIG_ESP_SDR_LILYGO_T_EMBED_SI4732 || CONFIG_ESP_SDR_ATS_MINI
#define BATTERY_ADC_PIN 4
#endif

#ifdef BATTERY_ADC_PIN
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"

int board_battery_percent(void) {
    static adc_oneshot_unit_handle_t adc;
    static adc_channel_t channel;
    if (!adc) {
        adc_unit_t unit;
        if (adc_oneshot_io_to_channel(BATTERY_ADC_PIN, &unit, &channel) != ESP_OK) return -1;
        const adc_oneshot_unit_init_cfg_t init = {.unit_id = unit};
        if (adc_oneshot_new_unit(&init, &adc) != ESP_OK) return -1;
        const adc_oneshot_chan_cfg_t config = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
        if (adc_oneshot_config_channel(adc, channel, &config) != ESP_OK) {
            adc_oneshot_del_unit(adc);
            adc = NULL;
            return -1;
        }
    }
#if CONFIG_ESP_SDR_ATS_MINI || CONFIG_ESP_SDR_LILYGO_T_EMBED_SI4732
    /* The board examples calibrate GPIO4 at 1.702 mV/count with 12 dB attenuation. */
    int total = 0;
    for (unsigned i = 0; i < 10; i++) {
        int raw;
        if (adc_oneshot_read(adc, channel, &raw) != ESP_OK) return -1;
        total += raw;
    }
    return battery_voltage_percent(total * 1702 / 10000);
#else
    static adc_cali_handle_t calibration;
    if (!calibration) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        const adc_cali_curve_fitting_config_t config = {
            .unit_id = ADC_UNIT_1, .chan = channel, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
        };
        if (adc_cali_create_scheme_curve_fitting(&config, &calibration) != ESP_OK) return -1;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        const adc_cali_line_fitting_config_t config = {
            .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
        };
        if (adc_cali_create_scheme_line_fitting(&config, &calibration) != ESP_OK) return -1;
#else
        return -1;
#endif
    }
    int raw, mv;
    if (adc_oneshot_read(adc, channel, &raw) != ESP_OK ||
        adc_cali_raw_to_voltage(calibration, raw, &mv) != ESP_OK) return -1;
    return battery_voltage_percent(mv * 2);
#endif
}

#else
#include "driver/i2c_master.h"

static i2c_master_dev_handle_t battery_device;

static int battery_read(uint8_t reg, uint8_t *data, size_t size) {
    return i2c_master_transmit_receive(battery_device, &reg, 1, data, size, 20);
}

int board_battery_percent(void) {
    if (!battery_device) {
        i2c_master_bus_handle_t bus;
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2
        if (i2c_master_get_bus_handle(I2C_NUM_1, &bus) != ESP_OK) {
#if CONFIG_ESP_SDR_M5STACK_BASIC
            const i2c_master_bus_config_t config = {
                .i2c_port = I2C_NUM_1, .sda_io_num = 21, .scl_io_num = 22,
                .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
                .flags.enable_internal_pullup = true,
            };
            if (i2c_new_master_bus(&config, &bus) != ESP_OK) return -1;
#else
            return -1;
#endif
        }
#else
        if (i2c_master_get_bus_handle(I2C_NUM_0, &bus) != ESP_OK) return -1;
#endif
        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7, .scl_speed_hz = 400000,
#if CONFIG_ESP_SDR_M5STACK_BASIC
            .device_address = 0x75, /* Optional I2C IP5306; older Basics have no telemetry. */
#elif CONFIG_ESP_SDR_M5STACK_CORE2
            .device_address = 0x34,
#elif CONFIG_ESP_SDR_LILYGO_T_DISPLAY_S3_PRO
            .device_address = 0x6a,
#else
            .device_address = 0x55, /* BQ27220: Pager, CC1101/Plus and T-Deck Pro. */
#endif
        };
        if (i2c_master_probe(bus, config.device_address, 20) != ESP_OK ||
            i2c_master_bus_add_device(bus, &config, &battery_device) != ESP_OK) return -1;
    }
    uint8_t data[2];
#if CONFIG_ESP_SDR_M5STACK_BASIC
    if (battery_read(0x78, data, 1) != ESP_OK) return -1;
    switch (data[0] >> 4) {
        case 0x0: return 100;
        case 0x8: return 75;
        case 0xc: return 50;
        case 0xe: return 25;
        case 0xf: return 0;
        default: return -1;
    }
#elif CONFIG_ESP_SDR_M5STACK_CORE2
    if (battery_read(0x03, data, 1) != ESP_OK) return -1;
    if (data[0] == 0x4a) {
        if (battery_read(0x00, data, 1) != ESP_OK || !(data[0] & 0x08) ||
            battery_read(0xa4, data, 1) != ESP_OK || data[0] > 100) return -1;
        return data[0];
    }
    if (data[0] != 0x03 || battery_read(0x01, data, 1) != ESP_OK || !(data[0] & 0x20)) return -1;
    if (battery_read(0x82, data, 1) != ESP_OK) return -1;
    const uint8_t enable_adc[] = {0x82, data[0] | 0x80};
    if (i2c_master_transmit(battery_device, enable_adc, sizeof(enable_adc), 20) != ESP_OK ||
        battery_read(0x78, data, 2) != ESP_OK) return -1;
    return battery_voltage_percent(((data[0] << 4) | (data[1] & 0x0f)) * 11 / 10);
#elif CONFIG_ESP_SDR_LILYGO_T_DISPLAY_S3_PRO
    /* Charging is disabled, so SY6970 cannot measure VBAT while VBUS is present. */
    if (battery_read(0x0b, data, 1) != ESP_OK || (data[0] & 0xe0) ||
        battery_read(0x0e, data, 1) != ESP_OK || !(data[0] & 0x7f)) return -1;
    return battery_voltage_percent(2304 + (data[0] & 0x7f) * 20);
#else
    if (battery_read(0x0a, data, 2) != ESP_OK || !(data[0] & 0x08) ||
        battery_read(0x2c, data, 2) != ESP_OK) return -1;
    unsigned percent = data[0] | (data[1] << 8);
    return percent <= 100 ? (int)percent : -1;
#endif
}
#endif
#endif
