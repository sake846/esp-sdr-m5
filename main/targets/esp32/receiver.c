/* Original ESP32 RX-only backend for the ESP-SDR burst protocol.
 * Register sequence derived from ESP-IDF's ESP32 librftest.a: adctrig/mac_init.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "burst_serial.h"
#include "burst_gpio.h"
#include "burst_version.h"
#include "spectrum.h"
#include "spectrum_dc.h"
#include "software_agc.h"
#include "rx_recalibration.h"
#include "rx_tuning.h"
#include "rx_lo.h"
#include "rx_bandwidth.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_rom_crc.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "soc/soc.h"
#include "soc/rtc.h"
#include "soc/dport_reg.h"
#include "soc/dport_access.h"
#include "heap_memory_layout.h"
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
#include "m5stack_basic.h"
#define M5_LOCAL_FFT_MAX 8192u
#define M5_LOCAL_AVERAGE_MAX 16u
static bool local_capture;
static unsigned local_capture_gain;
#endif

/* Reserve both SRAM banks, including their instruction aliases. Mode 2 only
 * exposes 32 KiB to MAC; mode 3 exposes the entire 64 KiB capture aperture. */
SOC_RESERVE_MEMORY_REGION(0x3ffe8000, 0x3fff8000, esp32_rf_dump);
#define DUMP_CTRL   0x60033d90u
#define DUMP_STATUS 0x60033d94u
#define DUMP_BYTES  0x60033dc4u
#define RX_GAIN     0x3ff5c02cu
#define CAPACITY    16384u
#define MAX_SAMPLES (CAPACITY - 4)
#define SENTINEL    0xa55aa55au
static uint32_t *const samples = (void *)0x3ffe8000;
static unsigned gain_max, gain_code = 40;
static bool hardware_agc = true;
static bool software_agc, software_agc_probing;
static unsigned frequency_mhz = 2412;
/* Original ESP32 BBTOP RX filter: registers 1/2, seven-bit capacitor DAC.
 * Preserve bit 7 and restore PHY calibration before any retune or transfer. */
static int rx_filter = -1;
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
static bool rx_filter_span_follow = true;
#endif
extern unsigned rom_chip_i2c_readReg(unsigned, unsigned, unsigned);
extern void rom_chip_i2c_writeReg(unsigned, unsigned, unsigned, unsigned);
static unsigned filter_saved[2];
static void filter_apply(void) {
    for (unsigned j = 0; j < 2; j++) {
        filter_saved[j] = rom_chip_i2c_readReg(0x67, 1, 1 + j);
        if (rx_filter >= 0)
            rom_chip_i2c_writeReg(0x67, 1, 1 + j, (filter_saved[j] & ~127u) | (unsigned)rx_filter);
    }
    esp_rom_delay_us(10);
}
static void filter_restore(void) {
    if (rx_filter >= 0)
        for (unsigned j = 0; j < 2; j++) rom_chip_i2c_writeReg(0x67, 1, 1 + j, filter_saved[j]);
}
extern void rom_pbus_workmode(void);
extern void rom_pbus_xpd_rx_on(int);
extern void rom_pbus_xpd_tx_off(void);
extern void rom_set_rxclk_en(int);

static void reply(const char *fmt, ...) {
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    if (local_capture || software_agc_probing) return; /* Internal capture failures must not enter the UART protocol. */
#else
    if (software_agc_probing) return;
#endif
    char text[256];
    va_list ap;
    va_start(ap, fmt);
    int length = vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    if (length > 0) burst_serial_send(text, length < sizeof(text) ? length : sizeof(text) - 1);
}

static void tune_rx(unsigned mhz);
static void apply_gain(void);
static bool rx_ready;
static void prepare_rx(void) {
    if (!rx_ready || rx_recalibration_stale()) {
        tune_rx(frequency_mhz);
        rx_ready = true;
    }
    rom_pbus_workmode();
    rom_pbus_xpd_tx_off();
    rom_pbus_xpd_rx_on(1);
    rom_set_rxclk_en(1);
    /* PBUS work-mode setup releases a forced RX gain; apply it afterwards. */
    apply_gain();
    if (rx_lo_select(rx_lo_plan(frequency_mhz).alternate)) esp_rom_delay_us(3000);
}

/* Clock selectors: 0=16 MS/s, 1=80 MS/s, 2=40 MS/s. Hardware sampling,
 * not software decimation. Rates inferred from capture-duration slopes. */
static bool acquire_iq(unsigned n, unsigned source, unsigned clock, unsigned *capture_us) {
    if (n < 256 || n > MAX_SAMPLES || source > 3 || clock > 2) {
        reply("ERR args\n");
        return false;
    }
    if (rx_recalibration_stale()) prepare_rx();
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    if (software_agc && !software_agc_probing && !local_capture) {
#else
    if (software_agc && !software_agc_probing) {
#endif
        unsigned release_streak = 0;
        for (unsigned round = 0; round < SOFTWARE_AGC_MAX_ROUNDS; round++) {
            unsigned probe_us;
            software_agc_probing = true;
            bool probed = acquire_iq(SOFTWARE_AGC_PROBE_SAMPLES, source, clock, &probe_us);
            software_agc_probing = false;
            if (!probed) break;
            unsigned peak = software_agc_peak(samples, SOFTWARE_AGC_PROBE_SAMPLES);
            unsigned next = software_agc_next_gain(peak, gain_code, 0, gain_max, &release_streak);
            if (next == gain_code) {
                if (peak <= SOFTWARE_AGC_RELEASE_PEAK_MAX &&
                    release_streak < SOFTWARE_AGC_RELEASE_CONFIRMATIONS)
                    continue;
                break;
            }
            gain_code = next;
            spectrum_snapshot_reset_dc();
            apply_gain();
        }
    }
    REG_WRITE(DUMP_CTRL, 0);
    for (unsigned j = 0; j < CAPACITY; j++) samples[j] = SENTINEL;
    filter_apply();
    uint32_t owner = DPORT_REG_READ(DPORT_IRAM_DRAM_AHB_SEL_REG);
    uint32_t byte_select = REG_READ(DUMP_BYTES);
    DPORT_REG_WRITE(DPORT_IRAM_DRAM_AHB_SEL_REG,
                    (owner & ~DPORT_MAC_DUMP_MODE_M) | (3 << DPORT_MAC_DUMP_MODE_S));
    REG_WRITE(DUMP_BYTES, 0x03020100); /* Four distinct bytes, as in mac_init. */
    uint32_t ctrl = n | (source << 20) | ((clock == 0) << 16) | ((clock == 2) << 15);
    REG_WRITE(DUMP_CTRL, ctrl | BIT(31));
    int64_t start = esp_timer_get_time();
    REG_SET_BIT(DUMP_CTRL, BIT(19));
    REG_CLR_BIT(DUMP_CTRL, BIT(19));
    while (!(REG_READ(DUMP_CTRL) & BIT(18)) && esp_timer_get_time() - start < 20000) {}
    uint32_t result = REG_READ(DUMP_CTRL), status = REG_READ(DUMP_STATUS);
    unsigned elapsed = esp_timer_get_time() - start;
    REG_WRITE(DUMP_CTRL, 0);
    esp_rom_delay_us(10);
    DPORT_REG_WRITE(DPORT_IRAM_DRAM_AHB_SEL_REG, owner);
    REG_WRITE(DUMP_BYTES, byte_select);
    filter_restore();
    if (!(result & BIT(18)) || (status & 0x7fff) != n) {
        reply("ERR capture_timeout\n");
        return false;
    }
    for (unsigned j = 0; j < CAPACITY; j++) {
        if ((j < n && samples[j] == SENTINEL) || (j >= n && samples[j] != SENTINEL)) {
            reply("ERR capture_memory %u\n", j);
            return false;
        }
    }
    *capture_us = elapsed;
    return true;
}

static bool spectrum_acquire(unsigned n, unsigned rate, const uint32_t **data, unsigned *elapsed) {
    unsigned clock = rate == 6 ? 0 : rate == 1 ? 2 : 1;
    bool ok = acquire_iq(n, 0, clock, elapsed);
    *data = samples;
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    if (ok && local_capture) local_capture_gain = (uint8_t)(samples[0] >> 20);
#endif
    return ok;
}

#include "ring_probe.h"

static void apply_gain(void) {
    uint32_t gain = REG_READ(RX_GAIN);
    if (hardware_agc)
        REG_CLR_BIT(RX_GAIN, BIT(23));
    else
        REG_WRITE(RX_GAIN, (gain & 0x7fffff) | (gain_code << 24) | BIT(23));
}

extern void set_chanfreq(unsigned mhz, unsigned mode);
extern void rom_set_rf_freq_offset(unsigned crystal, unsigned mhz, int offset);

static void tune_rx(unsigned mhz) {
    static unsigned calibrated_mhz;
    if (calibrated_mhz != mhz || rx_recalibration_stale()) {
        rx_recalibrate(mhz);
        calibrated_mhz = mhz;
    }
    rx_lo_plan_t plan = rx_lo_plan(mhz);
    rx_lo_select(false);
    bool channel = (mhz >= 2412 && mhz <= 2472 && (mhz - 2412) % 5 == 0) || mhz == 2484;
    /* Calibrate on a real channel before bypassing the channel-number mapping.
     * Use the PHY entry point even for repeated requests: the Wi-Fi API can
     * skip a channel it thinks is already selected after a direct PLL retune. */
    set_chanfreq(channel ? mhz : 2412, 0);
    if (!channel) {
        unsigned xtal = rtc_clk_xtal_freq_get();
        /* Channel setup pins its capacitor code. Release it before the ROM
         * retune, otherwise the SDM changes but the PLL can remain unlocked. */
        unsigned cap = ram_chip_i2c_readReg(0x62, 1, 0);
        ram_chip_i2c_writeReg(0x62, 1, 0, cap & ~0x80u);
        /* This ROM uses 1/1024 MHz offsets, unlike the newer kHz API. */
        int offset = (plan.offset_khz * 1024 + 500) / 1000;
        rom_set_rf_freq_offset(xtal == 26 ? 1 : xtal == 24 ? 2 : 0, plan.mhz, offset);
    }
}

#define IQ_BUFFER  samples
#define IQ_WORDS   MAX_SAMPLES
#define send_bytes burst_serial_send
#include "burst_capture.h"

/* Rate codes 0, 1 and 6 (80, 40 and 16 MS/s) are dump clocks 1, 2 and 0. */
static bool capture(unsigned n, unsigned rate, unsigned format) {
    unsigned elapsed;
    if (rate != 0 && rate != 1 && rate != 6) {
        reply("ERR rate\n");
        return false;
    }
    if (!acquire_iq(n, 0, rate == 0 ? 1 : rate == 1 ? 2 : 0, &elapsed)) return false;
    return burst_send_capture(n, format, elapsed);
}

#define frequency_valid rx_frequency_valid
#define BURST_ID        "ESP32SDR"
#define BURST_MAX_RATE  6
#include "burst_commands.h"

static void command(char *line) {
    if (burst_version_command(line)) return;
    if (burst_gpio_command(line)) return;
#ifdef RING_PROBE
    if (ring_probe_command(line)) return;
#endif
    if (spectrum_command(line, frequency_mhz, spectrum_acquire)) return;
    if (burst_common_command(line)) return;
    unsigned n;
    char extra;
    if (!strcmp(line, "CAPS"))
        reply(
            "CAPS VERSION GPIO SPEC SPECN SPECCAPS SPECSTAT DCT UARTBAUD RXLIMITS SERIALLEASE TUNEEXT RX40 RX16 LPFANA GAIN HWAGC "
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
            "SWAGC "
#endif
            "IQ8\n");
    else if (sscanf(line, "BANDWIDTH %u %c", &n, &extra) == 1 &&
             (!n || (n >= RX_BANDWIDTH_MIN && n <= RX_BANDWIDTH_MAX))) {
        rx_filter = rx_bandwidth_dcap(n);
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
        rx_filter_span_follow = false;
#endif
        reply("OK\n");
    } else if (!strcmp(line, "LPF AUTO")) {
        rx_filter = -1;
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
        rx_filter_span_follow = true;
#endif
        reply("OK\n");
    } else if (sscanf(line, "LPF %u %c", &n, &extra) == 1 && n <= 127) {
        rx_filter = n;
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
        rx_filter_span_follow = false;
#endif
        reply("OK\n");
    } else if (!strcmp(line, "LPF?"))
        reply("LPF %d %u %u\n", rx_filter, rom_chip_i2c_readReg(0x67, 1, 1),
              rom_chip_i2c_readReg(0x67, 1, 2));
    else if (!strcmp(line, "LIMITS?")) {
        reply(
            "LIMITS {\"gain\":[0,%u,1],\"bandwidth\":[%u,%u,1,0],\"rates\":[80000000,40000000,16000000],\"bits\":[8,10]}\n",
            gain_max, RX_BANDWIDTH_MIN, RX_BANDWIDTH_MAX);
    } else if (!strcmp(line, "GAIN?"))
        reply("GAIN %s %d 0 %u %u\n", hardware_agc ? "HARDWARE" : software_agc ? "SOFTWARE" : "MANUAL",
              hardware_agc ? -1 : (int)gain_code, gain_max, (unsigned)((REG_READ(RX_GAIN) >> 23) & 1));
    else if (!strcmp(line, "GAIN HARDWARE")) {
        hardware_agc = true;
        software_agc = false;
        apply_gain();
        reply("OK\n");
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    } else if (!strcmp(line, "GAIN SOFTWARE")) {
        hardware_agc = false;
        software_agc = true;
        apply_gain();
        reply("OK\n");
#endif
    } else if (sscanf(line, "GAIN MANUAL %u %c", &n, &extra) == 1 && n <= gain_max) {
        hardware_agc = false;
        software_agc = false;
        gain_code = n;
        apply_gain();
        reply("OK\n");
    } else
        reply("ERR command\n");
}

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_NONE);
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    m5stack_basic_init();
#endif
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    prepare_rx();
    gain_max = (REG_READ(RX_GAIN) >> 8) & 127;
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    hardware_agc = false;
    software_agc = true;
    gain_code = 30;
    apply_gain();
#else
    REG_CLR_BIT(RX_GAIN, BIT(23)); /* Hardware AGC is the default outside M5Stack profiles. */
#endif
    fflush(stdout);
    burst_serial_init();
    burst_serial_serve(command);
}

#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
void burst_serial_idle(bool leased) {
    static int64_t next_poll, next_frame;
    static int64_t average_start;
    static uint64_t frame_work_us;
    static bool showing_pc;
    static unsigned pc_mhz;
    static unsigned snapshot_mhz, snapshot_rate, snapshot_fft, snapshot_average_target;
    static unsigned cpu_percent, average_count, average_slot, average_gain;
    static unsigned frame_iq_peak;
    static unsigned agc_release_streak;
    static uint32_t agc_peak_q8;
    static bool agc_peak_ready;
    static bool agc_was_enabled;
    static uint8_t bins[M5_LOCAL_FFT_MAX];
    /* Log-power sums in Q6 represent linear-power averaging with 16 KiB of RAM. */
    static uint16_t average_power_q6[M5_LOCAL_FFT_MAX];
    /* Q6 correction: 20*log10(1 + 10^(-difference/20)). */
    static const uint16_t log_add_q6[] = {
        385, 354, 325, 298, 272, 248, 226, 205, 186, 169, 153, 138, 125, 112, 101, 91, 82, 73, 66, 59,
        53,  47,  42,  38,  34,  30,  27,  24,  22,  19,  17,  15,  14,  12,  11,  10,  9,  8,  7,  6,
        6,   5,   4,   4,   3,   3,   3,   2,   2,   2,   2,   2,   1,   1,   1,   1,   1,   1,   1,   1,
        1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,
    };
    static const uint16_t log_count_q6[M5_LOCAL_AVERAGE_MAX + 1] = {
        0, 0, 385, 611, 771, 895, 996, 1082, 1156, 1221, 1280, 1333, 1381, 1426, 1467, 1505, 1541,
    };
    int64_t now = esp_timer_get_time();
    if (now < next_poll) return;
    next_poll = now + 2000;
    unsigned mhz = m5stack_basic_poll(frequency_mhz, leased);
    if (mhz != frequency_mhz) {
        frequency_mhz = mhz;
        rx_ready = false;
        prepare_rx();
    }
    unsigned agc = hardware_agc ? M5_AGC_HARDWARE : software_agc ? M5_AGC_SOFTWARE : M5_AGC_MANUAL;
    if (m5stack_basic_sync_rx(&agc, &gain_code, gain_max, &rx_filter, &rx_filter_span_follow)) {
        hardware_agc = agc == M5_AGC_HARDWARE;
        software_agc = agc == M5_AGC_SOFTWARE;
        apply_gain();
        spectrum_snapshot_reset_dc();
        average_count = average_slot = 0;
        average_start = 0;
        frame_work_us = 0;
        frame_iq_peak = 0;
        agc_peak_ready = false;
        agc_release_streak = 0;
    }
    if (software_agc != agc_was_enabled) {
        agc_was_enabled = software_agc;
        agc_peak_ready = false;
        agc_release_streak = 0;
    }
#if CONFIG_ESP_SDR_M5STACK_BASIC || CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    if (rx_filter_span_follow) rx_filter = rx_bandwidth_dcap(m5stack_basic_span());
#endif
    if (leased) {
        if (!showing_pc) {
            spectrum_snapshot_reset_dc();
            average_count = 0;
            average_slot = 0;
            frame_work_us = 0;
            average_start = 0;
        }
        if (!showing_pc || pc_mhz != frequency_mhz) {
            m5stack_basic_set_gain_index(gain_code);
            m5stack_basic_draw(frequency_mhz, bins, m5stack_basic_fft_points(), true, true, cpu_percent);
        }
        showing_pc = true;
        pc_mhz = frequency_mhz;
        return;
    }
    if (showing_pc) {
        showing_pc = false;
        agc_peak_ready = false;
        agc_release_streak = 0;
        frame_iq_peak = 0;
        spectrum_snapshot_reset_dc();
        average_count = 0;
        average_slot = 0;
        frame_work_us = 0;
        average_start = 0;
    }
    unsigned rate = m5stack_basic_rate();
    unsigned fft_points = m5stack_basic_fft_points();
    unsigned average_target = m5stack_basic_average_count();
    if (snapshot_mhz != frequency_mhz || snapshot_rate != rate || snapshot_fft != fft_points ||
        snapshot_average_target != average_target) {
        spectrum_snapshot_reset_dc();
        snapshot_mhz = frequency_mhz;
        snapshot_rate = rate;
        snapshot_fft = fft_points;
        snapshot_average_target = average_target;
        agc_peak_ready = false;
        agc_release_streak = 0;
        frame_iq_peak = 0;
        average_count = 0;
        average_slot = 0;
        frame_work_us = 0;
        average_start = 0;
    }
    now = esp_timer_get_time();
    if (!average_start) {
        average_start = now;
        next_frame = now + 100000;
    }

    now = esp_timer_get_time();
    if (now < next_frame && average_slot < average_target) {
        /* Trigger near the midpoint of each equal interval across the 100 ms window. */
        uint64_t due_slots = (uint64_t)(now - average_start) * average_target * 2 / 100000;
        if (due_slots) {
            unsigned due_slot = (unsigned)((due_slots - 1) / 2);
            if (due_slot >= average_target) due_slot = average_target - 1;
            if (due_slot >= average_slot) {
                average_slot = due_slot + 1; /* Skip missed slots rather than bunching captures. */
                int64_t work_start = esp_timer_get_time();
                local_capture = true;
                unsigned iq_peak = 0;
                bool captured = spectrum_snapshot_inplace_peak(rate, fft_points, spectrum_acquire, bins,
                                                               (int16_t *)IQ_BUFFER, &iq_peak);
                local_capture = false;
                if (captured) m5stack_basic_spectrum(frequency_mhz, bins, fft_points,
                                                     local_capture_gain, rx_filter, spectrum_dc_mode);
                int64_t work_end = esp_timer_get_time();
                frame_work_us += (uint64_t)(work_end - work_start);
                if (captured) {
                    if (iq_peak > frame_iq_peak) frame_iq_peak = iq_peak;
                    /* Do not mix FFT frames captured at different PHY gain indices. */
                    if (average_count && average_gain != gain_code) average_count = 0;
                    if (!average_count) average_gain = gain_code;
                    if (!average_count) {
                        for (unsigned j = 0; j < fft_points; j++)
                            average_power_q6[j] = bins[j] ? (uint16_t)(bins[j] << 6) : 0;
                    } else {
                        for (unsigned j = 0; j < fft_points; j++) {
                            unsigned sample = bins[j];
                            if (!sample) continue;
                            unsigned sum = average_power_q6[j], value = sample << 6;
                            if (!sum) {
                                average_power_q6[j] = (uint16_t)value;
                                continue;
                            }
                            unsigned high = sum > value ? sum : value;
                            unsigned low = sum > value ? value : sum;
                            unsigned difference = (high - low + 32) >> 6;
                            unsigned correction = difference < sizeof(log_add_q6) / sizeof(log_add_q6[0])
                                                      ? log_add_q6[difference]
                                                      : 0;
                            average_power_q6[j] = (uint16_t)(high + correction);
                        }
                    }
                    average_count++;
                }
            }
        }
    }

    now = esp_timer_get_time();
    if (now < next_frame) return;
    bool valid = average_count != 0;
    if (valid) {
        unsigned normalize = log_count_q6[average_count];
        for (unsigned j = 0; j < fft_points; j++) {
            unsigned sum = average_power_q6[j];
            unsigned mean = sum > normalize ? (sum - normalize + 32) >> 6 : 0;
            bins[j] = mean > 255 ? 255 : (uint8_t)mean;
        }
    }
    int64_t draw_start = esp_timer_get_time();
    m5stack_basic_set_gain_index(gain_code);
    m5stack_basic_draw(frequency_mhz, bins, fft_points, false, valid, cpu_percent);
    int64_t draw_end = esp_timer_get_time();
    frame_work_us += (uint64_t)(draw_end - draw_start);
    int64_t frame_end = draw_end;
    if (software_agc && valid && frame_iq_peak) {
        int64_t agc_start = esp_timer_get_time();
        unsigned peak = software_agc_smooth_peak(frame_iq_peak, &agc_peak_q8, &agc_peak_ready);
        unsigned next_gain = software_agc_next_gain(peak, gain_code, 0, gain_max, &agc_release_streak);
        if (next_gain != gain_code) {
            gain_code = next_gain;
            apply_gain();
            spectrum_snapshot_reset_dc();
        }
        frame_end = esp_timer_get_time();
        frame_work_us += (uint64_t)(frame_end - agc_start);
    }
    frame_iq_peak = 0;
    uint64_t elapsed_us = (uint64_t)(frame_end - average_start);
    unsigned frame_load = elapsed_us ? (unsigned)(frame_work_us * 100 / elapsed_us) : 100;
    if (frame_load > 100) frame_load = 100;
    cpu_percent = (cpu_percent * 3 + frame_load + 2) / 4;
    average_count = 0;
    average_slot = 0;
    frame_work_us = 0;
    average_start = frame_end;
    next_frame = frame_end + 100000;
}
#endif
