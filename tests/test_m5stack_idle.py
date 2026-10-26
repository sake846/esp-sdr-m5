"""Exercise both production standalone loops with a deterministic clock/capture."""
import unittest

from host_test import COMPILER, ROOT, compile_run


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class StandaloneLoop(unittest.TestCase):
    def test_linear_power_average_failures_setting_changes_and_pc_handoff(self):
        stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "software_agc.h"
#include "m5stack_basic_controls.h"
static unsigned gain_code = 30, gain_applies;
static m5stack_controls_t ui;
static int rx_filter = -1;
static bool rx_filter_span_follow = true;
#if TEST_S3
#define GAIN_SOFTWARE M5_AGC_SOFTWARE
#define GAIN_MIN 0
static unsigned gain_mode = M5_AGC_MANUAL;
static unsigned gain_max(void) { return 72; }
static void gain_reconfigure(void) { gain_applies++; }
#else
static unsigned gain_max = 72;
static bool hardware_agc, software_agc;
static void apply_gain(void) { gain_applies++; }
#endif
static bool m5stack_basic_sync_rx(unsigned *agc, unsigned *gain, unsigned maximum,
                                 int *filter, bool *follow) {
    return m5stack_controls_sync_rx(&ui, agc, gain, maximum, filter, follow);
}
static void m5stack_basic_set_gain_index(unsigned gain) { assert(gain <= 72); }
#define M5_LOCAL_FFT_MAX 8192
#define M5_LOCAL_AVERAGE_MAX 16
static int64_t now = 10000;
static unsigned frequency_mhz = 2412, rate = 6, points = 256, target = 1;
static unsigned captures, draws, resets, prepared, requested_mhz, fail_every;
static unsigned successful, drawn_points;
static double expected_power[4];
static bool rx_ready = true, local_capture, drawn_valid, drawn_pc;
static uint8_t drawn[8192], local_bins[8192];
static uint16_t local_average_power_q6[8192];
static int16_t iq_buffer[16384];
#define IQ_BUFFER iq_buffer
static int64_t esp_timer_get_time(void) { return now; }
static void prepare_rx(void) { assert(!rx_ready); prepared++; rx_ready = true; }
static unsigned m5stack_basic_poll(unsigned mhz, bool pc) { return pc || !requested_mhz ? mhz : requested_mhz; }
static unsigned m5stack_basic_rate(void) { return rate; }
static unsigned m5stack_basic_fft_points(void) { return points; }
static unsigned m5stack_basic_average_count(void) { return target; }
static void spectrum_snapshot_reset_dc(void) { resets++; }
static bool spectrum_acquire(unsigned n, unsigned r, const uint32_t **p, unsigned *us) { return true; }
static bool spectrum_snapshot(unsigned r, unsigned n,
    bool (*acquire)(unsigned,unsigned,const uint32_t**,unsigned*), uint8_t *bins) {
    assert(local_capture && r == rate && n == points && acquire == spectrum_acquire);
    unsigned pass = captures++;
    now += 100;
    if (fail_every && (pass+1)%fail_every == 0) return false;
    const unsigned values[] = {pass%2 ? 120 : 80, 0, 255, pass%2 ? 0 : 60};
    for (unsigned j = 0; j < n; j++) bins[j] = (uint8_t)values[j%4];
    for (unsigned j = 0; j < 4; j++)
        expected_power[j] += values[j] ? pow(10.0, values[j]/20.0) : 0;
    successful++;
    return true;
}
static bool spectrum_snapshot_inplace_peak(unsigned r, unsigned n,
    bool (*acquire)(unsigned,unsigned,const uint32_t**,unsigned*), uint8_t *bins, int16_t *workspace, unsigned *peak) {
    *peak = 0;
    assert(workspace == iq_buffer); return spectrum_snapshot(r, n, acquire, bins);
}
static void m5stack_basic_draw(unsigned mhz, const uint8_t *bins, unsigned n,
                                bool pc, bool valid, unsigned cpu) {
    assert(!local_capture && mhz == frequency_mhz && cpu <= 100);
    draws++; drawn_valid = valid; drawn_pc = pc; drawn_points = n;
    memcpy(drawn, bins, n); now += 500;
}
'''
        check = r'''
static int64_t begin_window(void) {
    frequency_mhz++; now += 200000; /* Retuning starts a fresh averaging window. */
    captures = successful = 0; memset(expected_power, 0, sizeof(expected_power));
    burst_serial_idle(false); return now;
}
static void at(int64_t t, bool pc) { now = t; burst_serial_idle(pc); assert(!local_capture); }
static void verify_average(void) {
    assert(!drawn_pc && drawn_valid == (successful != 0) && drawn_points == points);
    if (!successful) return;
    for (unsigned j = 0; j < points; j++) {
        double p = expected_power[j%4]/successful;
        int expected = p > 0 ? (int)lrint(20*log10(p)) : 0;
        assert(abs((int)drawn[j]-expected) <= 1);
    }
}
int main(void) {
    for (target = 1; target <= 16; target *= 2) {
        for (fail_every = 0; fail_every <= 2; fail_every++) {
            int64_t start = begin_window(); unsigned before = draws;
            for (unsigned k = 0; k < target; k++)
                at(start+(2*k+1)*50000/target, false);
            assert(captures == target && draws == before);
            at(start+100000, false);
            assert(draws == before+1); verify_average();
        }
    }
    target = 16; fail_every = 0;
    int64_t start = begin_window();
    at(start+90000, false); assert(captures == 1); /* Missed slots do not bunch captures. */
    at(start+100000, false); assert(captures == 1); verify_average();
    target = 2;
    start = begin_window(); at(start+25000, false);
    assert(captures == 1);
    unsigned before = draws, reset_before = resets;
    at(start+30000, true); assert(draws == before+1 && drawn_pc);
    at(start+35000, true); assert(draws == before+1 && captures == 1);
    frequency_mhz++; at(start+40000, true); assert(draws == before+2);
    at(start+45000, false); assert(resets > reset_before && captures == 1);
    /* Resume must discard the partial average from before the PC lease. */
    captures = successful = 0; memset(expected_power, 0, sizeof(expected_power));
    at(start+70000, false); at(start+120000, false); at(start+145000, false);
    assert(captures == 2); verify_average();
    /* Each setting change also discards the old partial frame. */
    for (unsigned setting = 0; setting < 4; setting++) {
        target = 2; start = begin_window(); at(start+25000, false);
        reset_before = resets;
        if (setting == 0) rate = rate == 6 ? 1 : 6;
        if (setting == 1) points = 4096;
        if (setting == 2) target = 4;
        if (setting == 3) requested_mhz = frequency_mhz+1;
        at(start+30000, false); assert(resets == reset_before+1);
        requested_mhz = 0;
        captures = successful = 0; memset(expected_power, 0, sizeof(expected_power));
        for (unsigned k = 0; k < target; k++)
            at(start+30000+(2*k+1)*50000/target, false);
        at(start+130000, false); verify_average();
    }
    assert(prepared == 1);
#if !TEST_S3
    target = 2; start = begin_window(); at(start+25000, false);
    reset_before = resets;
    ui.menu = M5_MENU_GAIN;
    m5stack_menu_adjust(&ui, frequency_mhz, 1);
    at(start+30000, false);
    assert(gain_code == 31 && !hardware_agc && !software_agc && gain_applies == 1);
    assert(resets == reset_before + 1);
    captures = successful = 0; memset(expected_power, 0, sizeof(expected_power));
    at(start+55000, false); at(start+105000, false); at(start+130000, false);
    assert(captures == 2); verify_average(); /* Old gain samples were discarded. */
    ui.menu = M5_MENU_FILTER;
    m5stack_menu_adjust(&ui, frequency_mhz, 1);
    at(start+135000, false);
    assert(!rx_filter_span_follow && rx_filter == 0);
#endif
    return 0;
}
'''
        for chip in ['esp32', 'esp32s3']:
            with self.subTest(chip=chip):
                source = (ROOT / f'main/targets/{chip}/receiver.c').read_text()
                start = source.index('void burst_serial_idle(bool leased) {')
                function = source[start:source.index('\n}', start) + 2]
                compile_run(stub + function + check, [f"TEST_S3={int(chip == 'esp32s3')}"])
