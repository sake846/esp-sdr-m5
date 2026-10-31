"""Local snapshots use the real unpack, Hann window, FFT and power encoder."""
import unittest

from host_test import COMPILER, ROOT, compile_run


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class LocalSpectrum(unittest.TestCase):
    def test_tones_inplace_dc_workspace_reuse_and_allocation_recovery(self):
        source = (ROOT / 'main/common/spectrum.c').read_text()
        source = source[:source.index('static void capabilities(')]
        for header in ['esp_cpu.h', 'esp_heap_caps.h', 'dsps_fft2r.h', 'esp_rom_crc.h',
                       'esp_timer.h', 'freertos/FreeRTOS.h', 'freertos/task.h']:
            source = source.replace(f'#include "{header}"', '')
        stub = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "spectrum.h"
#define ESP_OK 0
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
unsigned spectrum_dc_mode;
static unsigned allocations, fail_at, table_error, acquisitions;
static void *heap_caps_realloc(void *p, size_t n, unsigned caps) {
    assert(caps == 3); if (++allocations == fail_at) return NULL; return realloc(p, n);
}
static void *heap_caps_malloc(size_t n, unsigned caps) { return heap_caps_realloc(NULL, n, caps); }
static unsigned reverse(unsigned v, unsigned bits) {
    unsigned r = 0; while (bits--) { r = 2*r+(v&1); v >>= 1; } return r;
}
static int dsps_gen_w_r2_sc16(int16_t *w, unsigned n) {
    if (table_error == 1) { w[0] = 0; return -1; }
    for (unsigned j = 0; j < n/2; j++) {
        w[2*j] = (int16_t)(32767*cos(2*M_PI*j/n));
        w[2*j+1] = (int16_t)(32767*sin(2*M_PI*j/n));
    }
    return ESP_OK;
}
static int dsps_bit_rev_sc16_ansi(int16_t *w, unsigned n) {
    if (table_error == 2) { w[0] = 0; return -1; }
    unsigned bits = 0; while ((1u << bits) < n) bits++;
    for (unsigned j = 0; j < n; j++) {
        unsigned k = reverse(j, bits);
        if (j < k) for (unsigned c = 0; c < 2; c++) {
            int16_t t = w[2*j+c]; w[2*j+c] = w[2*k+c]; w[2*k+c] = t;
        }
    }
    return ESP_OK;
}
static int dsps_fft2r_init_sc16(int16_t *w, unsigned n) {
    if (dsps_gen_w_r2_sc16(w, n)) return -1;
    return dsps_bit_rev_sc16_ansi(w, n/2);
}
bool burst_serial_send(const void *p, size_t n) { assert(0); return false; }
'''
        check = r'''
static union { uint32_t words[8192]; int16_t samples[16384]; } capture;
static uint8_t bins[8192], reference[8192];
static bool acquire(unsigned n, unsigned rate, const uint32_t **p, unsigned *us) {
    assert(n <= MAX_FFT && (rate == 6 || rate == 1));
    acquisitions++; *p = capture.words; *us = 1; return true;
}
static bool fail_acquire(unsigned n, unsigned rate, const uint32_t **p, unsigned *us) { return false; }
static void tone(unsigned n, int frequency) {
    for (unsigned j = 0; j < n; j++) {
        int i = (int)lrint(100+120*cos(2*M_PI*frequency*j/n));
        int q = (int)lrint(-80+120*sin(2*M_PI*frequency*j/n));
        capture.words[j] = ((unsigned)i & 1023) | (((unsigned)q & 1023) << 10);
    }
}
static void constant(unsigned n, unsigned value) {
    for (unsigned j = 0; j < n; j++) capture.words[j] = value;
}
static void quiet(unsigned n) { for (unsigned j = 0; j < n; j++) assert(bins[j] == 0); }
int main(void) {
#if CHECK_MEMORY_BUDGET
    tone(1024, 17);
    assert(spectrum_snapshot_inplace(6, 1024, acquire, bins, capture.samples));
    assert(fft_data == NULL && power_storage == NULL && frame == NULL);
    assert(fft_capacity == 0 && window_capacity == 1024 && twiddle_capacity == 1024);
    assert(allocations == 2); /* Only the two 2 KiB tables are needed locally. */
    fail_at = allocations + 1;
    assert(!spectrum_fft_init());
    fail_at = 0;
    assert(spectrum_fft_init());
    assert(fft_capacity == 2048 && window_capacity == 2048 && twiddle_capacity == 2048);
    assert(power_storage && frame);
    unsigned prepared_allocations = allocations;
    assert(spectrum_fft_init() && allocations == prepared_allocations);
    assert(spectrum_fft_init_n(256) && twiddle_n == 2048);
    return 0;
#endif
    const unsigned invalid[] = {0, 255, 257, MAX_FFT+1, MAX_FFT*2};
    for (unsigned j = 0; j < sizeof(invalid)/sizeof(*invalid); j++)
        assert(!spectrum_snapshot(6, invalid[j], acquire, bins));
    assert(!spectrum_snapshot(2, 256, acquire, bins));
    assert(!spectrum_snapshot(6, 256, NULL, bins));
    assert(!spectrum_snapshot(6, 256, acquire, NULL));
    assert(acquisitions == 0);
    assert(!spectrum_snapshot_inplace(6, 256, acquire, bins, NULL));
#if SPEC_HEAP_WORKSPACE
    /* Each failed growth is retriable; existing buffers remain usable. */
    for (unsigned j = 1; j <= 3; j++) {
        fail_at = allocations + j;
        assert(!spectrum_snapshot(6, 256u << (j-1), acquire, bins));
        fail_at = 0;
        assert(spectrum_snapshot(6, 256u << (j-1), acquire, bins));
    }
    for (unsigned failure = 1; failure <= 2; failure++) {
        table_error = failure;
        assert(!spectrum_fft_init_n(1024u << failure));
        table_error = 0;
        tone(256, 17);
        assert(spectrum_snapshot(6, 256, acquire, bins) && bins[17] > 140);
    }
    assert(spectrum_fft_init_n(2048));
#endif
    assert(!spectrum_snapshot(6, 256, fail_acquire, bins));
    int16_t unrelated[512] = {123};
    assert(!spectrum_snapshot_inplace(6, 256, acquire, bins, unrelated));
    assert(unrelated[0] == 123);
    for (unsigned n = 256; n <= MAX_FFT; n *= 2) {
        for (int f = -23; f <= 17; f += 40) {
            tone(n, f);
            assert(spectrum_snapshot(6, n, acquire, reference));
            tone(n, f);
            assert(spectrum_snapshot_inplace(6, n, acquire, bins, capture.samples));
            assert(!memcmp(bins, reference, n));
            unsigned peak = 0;
            for (unsigned j = 1; j < n; j++) if (bins[j] > bins[peak]) peak = j;
            assert(peak == (unsigned)((f+(int)n)%(int)n));
            int expected = (int)lrint(40*log10(120*32.0));
            assert(abs((int)bins[peak]-expected) <= 2);
            assert(bins[(peak+n/4)%n]+40 < bins[peak]);
        }
    }
    /* Ring workspace users may overwrite the Hann table. */
    spectrum_workspace_t shared = spectrum_workspace();
    assert(shared.fft && shared.window && shared.power && shared.frame);
    memset(shared.window, 0, MAX_FFT*sizeof(*shared.window));
    tone(256, 17); assert(spectrum_snapshot(6, 256, acquire, bins));
    assert(bins[17] > 140);
    spectrum_dc_mode = 1; spectrum_snapshot_reset_dc();
    constant(256, 100); assert(spectrum_snapshot(6, 256, acquire, bins)); quiet(256);
    constant(256, 200); assert(spectrum_snapshot(6, 256, acquire, bins)); assert(bins[0] > 0);
    assert(spectrum_snapshot(1, 256, acquire, bins)); quiet(256); /* New rate resets DC. */
    constant(512, 300); assert(spectrum_snapshot(1, 512, acquire, bins)); quiet(512);
    spectrum_dc_mode = 0;
    constant(512, 400); assert(spectrum_snapshot(1, 512, acquire, bins)); quiet(512);
    spectrum_dc_mode = 1;
    assert(spectrum_snapshot(1, 512, acquire, bins)); quiet(512);
    unsigned before = allocations;
    assert(spectrum_snapshot(1, 512, acquire, bins));
    assert(allocations == before); /* Repeated snapshots reuse memory. */
    return 0;
}
'''
        for profile, chip in [('BASIC', 'ESP32'), ('CORE2', 'ESP32'),
                              ('STICKC_PLUS2', 'ESP32'), ('CARDPUTER', 'ESP32S3'),
                              ('CARDPUTER_ADV', 'ESP32S3'), ('CORES3', 'ESP32S3'),
                              ('STICKC', 'ESP32'), ('LILYGO', 'ESP32S3'), ('ATS_MINI', 'ESP32S3')]:
            for budget in [0, 1]:
                with self.subTest(profile=profile, memory_budget=bool(budget)):
                    board = profile not in ('LILYGO', 'ATS_MINI')
                    board_definitions = ['CONFIG_ESP_SDR_M5STACK=1'] if board else []
                    compile_run(stub + source + check, [f'CONFIG_IDF_TARGET_{chip}=1',
                                *board_definitions,
                                (f'CONFIG_ESP_SDR_{profile}=1' if not board else
                                 f'CONFIG_ESP_SDR_M5STACK_{profile}=1'), f'CHECK_MEMORY_BUDGET={budget}'])
