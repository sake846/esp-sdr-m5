"""Run the DPX input, history, scanline mapping and allocation fallback on all boards."""
import unittest

from host_test import COMPILER, ROOT, compile_run
from test_m5stack_displays import STUB


CHECK = r'''
#define CELL(level,x) density[(level)*density_width+(x)*density_width/PLOT_WIDTH]
static void sample(unsigned mhz, uint8_t *bins, unsigned gain, int filter, unsigned dc) {
    m5stack_basic_spectrum(mhz, bins, m5stack_basic_fft_points(), gain, filter, dc);
}
static void verify_projection(void) {
    uint8_t reference[64][PLOT_WIDTH], bins[8192];
    unsigned seed = 1;
    for (unsigned f = 0; f < sizeof(m5stack_fft_sizes) / sizeof(m5stack_fft_sizes[0]); f++) {
        for (unsigned r = 0; r < 6; r++) {
            controls.fft = f; controls.rate = r; controls.dpx = true;
            density_mode(2412, false); density_clear();
            unsigned n = m5stack_basic_fft_points(), span = m5stack_basic_span();
            unsigned visible = n * span / (span < 16 ? 16 : span);
            unsigned first_bin = (n - visible) / 2 + n / 2;
            assert(density_width == (visible < PLOT_WIDTH ? visible : PLOT_WIDTH));
            assert(last_allocation == 64 * density_width);
            memset(reference, 0, sizeof(reference));
            for (unsigned pass = 0; pass < 4; pass++) {
                for (unsigned j = 0; j < n; j++) {
                    seed = seed * 1664525u + 1013904223u;
                    bins[j] = (uint8_t)(seed >> 24);
                }
                /* Independent original projection, including duplicated screen columns. */
                for (unsigned x = 0; x < PLOT_WIDTH; x++) {
                    unsigned first = x * visible / PLOT_WIDTH, limit = (x + 1) * visible / PLOT_WIDTH;
                    if (limit <= first) limit = first + 1;
                    if (limit > visible) limit = visible;
                    unsigned peak = 0;
                    for (unsigned j = first; j < limit; j++) {
                        unsigned bin = first_bin + j;
                        if (bin >= n) bin -= n;
                        unsigned code = bins[n - 1 - bin];
                        if (code > peak) peak = code;
                    }
                    reference[peak >> 2][x] += 8;
                }
                sample(2412, bins, 30, -1, 1);
            }
            for (unsigned level = 0; level < 64; level++)
                for (unsigned x = 0; x < PLOT_WIDTH; x++) assert(CELL(level, x) == reference[level][x]);
        }
    }
    m5stack_controls_defaults(&controls);
    density_mode(2412, false);
}
static void verify_palette(void) {
    const unsigned stops[] = {0, 0x001f, 0x07ff, 0x07e0, 0xffe0, 0xf800, 0xffff};
    for (int contrast = 25; contrast <= 200; contrast += 25) {
        for (int bright = -30; bright <= 30; bright += 5) {
            controls.wf_contrast = contrast; controls.wf_bright = bright;
            for (int delta = -127; delta <= 127; delta++) {
                int level = delta * contrast / 100 + bright;
                unsigned expected = 0;
                if (level >= 60) expected = 0xffff;
                else if (level > 0) {
                    unsigned band = (unsigned)level / 10, step = (unsigned)level % 10;
                    unsigned from = stops[band], to = stops[band + 1];
                    unsigned red = (((from >> 11) & 31) * (10 - step) + ((to >> 11) & 31) * step + 5) / 10;
                    unsigned green = (((from >> 5) & 63) * (10 - step) + ((to >> 5) & 63) * step + 5) / 10;
                    unsigned blue = ((from & 31) * (10 - step) + (to & 31) * step + 5) / 10;
                    expected = (red << 11) | (green << 5) | blue;
                }
                assert(waterfall_color(delta) == expected);
            }
        }
    }
    m5stack_controls_defaults(&controls);
}
int main(void) {
    pmu[3] = TEST_PMU;
    m5stack_basic_init();
    assert(!controls.dpx && !density);
    uint8_t bins[8192] = {0};
    unsigned initial_allocations = allocations;
    sample(2412, bins, 30, -1, 1);
    assert(allocations == initial_allocations); /* Normal mode needs no density RAM. */
    controls.dpx = true;
    m5stack_basic_draw(2412, bins, 1024, false, true, 0);
    assert(density && allocations == initial_allocations + 1 && last_allocation == 64 * PLOT_WIDTH);
    for (unsigned i = 0; i < 15; i++) sample(2412, bins, 30, -1, 1);
    bins[510] = 120;
    sample(2412, bins, 30, -1, 1);
    assert(CELL(30,0) == 8 && CELL(0,0) == 120);
    bins[510] = 0; /* The averaged/current trace lacks the captured transient. */
    unsigned before_rows = rows, before_windows = column_windows;
    m5stack_basic_draw(2412, bins, 1024, false, true, 0);
    assert(rows == before_rows + TEST_HEIGHT && column_windows == before_windows + 1);
    assert(CELL(30,0) == 8);
    bool transient_visible = false;
    for (unsigned y = SPECTRUM_TOP; y < SPECTRUM_BOTTOM - 10; y++)
        if (pixels[y * TEST_WIDTH + PLOT_LEFT] == 0x001f) transient_visible = true;
    assert(transient_visible);
    controls.y_pos = 5;
    m5stack_basic_draw(2412, bins, 1024, false, true, 0);
    assert(CELL(30,0) == 8); /* Moving the view must not move/change the stored levels. */
    controls.y_pos = 0;

    /* Every bucket intersecting the viewport must appear, even on a 35-row graph. */
    const int floors[] = {-124, -84, -44, 0, 43, 80};
    for (unsigned fi = 0; fi < sizeof(floors) / sizeof(floors[0]); fi++) {
        for (unsigned level = 0; level < 64; level++) {
            memset(density, 0, 64 * density_width);
            CELL(level,PLOT_WIDTH / 2) = 8;
            bool seen = false;
            for (unsigned y = SPECTRUM_TOP; y < SPECTRUM_BOTTOM; y++) {
                memset(row, 0, sizeof(row));
                density_row(y, floors[fi]);
                if (row[PLOT_LEFT + PLOT_WIDTH / 2]) seen = true;
                assert(!row[0] && !row[SCREEN_WIDTH - 1]);
            }
            int bottom = (int)level * 20 - 843;
            bool intersects = bottom + 20 > floors[fi] * 10 && bottom < (floors[fi] + 80) * 10;
            assert(seen == intersects);
        }
    }
    density_clear();
    bins[510] = 120;
    for (unsigned i = 0; i < 100; i++) sample(2412, bins, 30, -1, 1);
    assert(CELL(30,0) == 255);
    test_now += 100000;
    density_decay();
    assert(CELL(30,0) == 239);
    test_now += 20000000;
    density_decay();
    assert(CELL(30,0) == 0);

    sample(2412, bins, 30, -1, 1);
    m5stack_basic_draw(2412, bins, 1024, true, true, 0);
    test_now += 20000000;
    sample(2412, bins, 30, -1, 1);
    m5stack_basic_draw(2412, bins, 1024, true, true, 0);
    assert(CELL(30,0) == 8); /* PC lease freezes both ingestion and fading. */
    m5stack_basic_draw(2412, bins, 1024, false, false, 0);
    assert(!density_keyed && CELL(30,0) == 0);

    sample(2412, bins, 30, -1, 1);
    bins[510] = 0;
    sample(2412, bins, 31, -1, 1);
    assert(!CELL(30,0)); /* Actual capture gain changed. */
    bins[510] = 120; sample(2412, bins, 31, -1, 1); bins[510] = 0;
    sample(2412, bins, 31, 0, 1); assert(!CELL(30,0));
    bins[510] = 120; sample(2412, bins, 31, 0, 1); bins[510] = 0;
    sample(2412, bins, 31, 0, 0); assert(!CELL(30,0));
    bins[510] = 120; sample(2412, bins, 31, 0, 0);
    density_mode(2413, false); assert(!CELL(30,0));
    sample(2413, bins, 31, 0, 0);
    controls.average = 3;
    density_mode(2413, false); assert(!CELL(30,0));
    sample(2413, bins, 31, 0, 0);
    controls.rate = 0;
    density_mode(2413, false); assert(!CELL(30,0));
    memset(bins, 0, sizeof(bins));
    bins[1023] = 128;
    sample(2413, bins, 31, 0, 0);
    assert(CELL(32,(PLOT_WIDTH + 1) / 2) == 8); /* Zoom preserves the RF direction. */
    controls.fft = 3;
    density_mode(2413, false); assert(!density_keyed);

    controls.dpx = false;
    unsigned before_frees = frees;
    density_mode(2413, false);
    assert(!density && frees == before_frees + 1);
    controls.dpx = true; fail_allocation = true;
    density_mode(2413, false);
    assert(!density && !controls.dpx && controls.dpx_failed);
    m5stack_basic_draw(2413, bins, 2048, false, true, 0);
    assert(!density && controls.dpx_failed); /* Failure stays visible; no retry loop. */
    fail_allocation = false; controls.dpx = true;
    density_mode(2413, false);
    assert(density && !controls.dpx_failed);
    m5stack_controls_defaults(&controls);
    density_mode(2412, false); assert(!density);
    verify_projection();
    verify_palette();
    return 0;
}
'''


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class SpectrumDensity(unittest.TestCase):
    def test_ingestion_mapping_fading_context_changes_and_allocation_failure(self):
        profiles = [
            ('BASIC', 320, 240, 0, 0, 27, 14, 23, 18, 3, 0, 0, 0),
            ('CORE2', 320, 240, 0, 0, 15, 5, 23, 18, 3, 21, 22, 0x03),
            ('CARDPUTER', 240, 135, 40, 53, 34, 37, 35, 36, 3, 8, 9, 0),
            ('CARDPUTER_ADV', 240, 135, 40, 53, 34, 37, 35, 36, 3, 8, 9, 0),
            ('STICKC_PLUS2', 135, 240, 52, 40, 14, 5, 15, 13, 2, 0, 0, 0),
        ]
        stub = STUB.replace(
            'static int64_t esp_timer_get_time(void) { return 1000000; }',
            'static int64_t test_now = 1000000;\nstatic int64_t esp_timer_get_time(void) { return test_now; }')
        stub = stub.replace(
            'static void *heap_caps_calloc(size_t n, size_t size, unsigned caps) { assert(caps==3); return calloc(n,size); }',
            'static unsigned allocations, frees; static size_t last_allocation; static bool fail_allocation;\n'
            'static void *heap_caps_calloc(size_t n, size_t size, unsigned caps) { '
            'assert(caps==3); allocations++; last_allocation=n*size; return fail_allocation ? NULL : calloc(n,size); }\n'
            'static void counted_free(void *p) { if(p)frees++; free(p); }\n#define free counted_free')
        for profile, *values in profiles:
            with self.subTest(profile=profile):
                filename = 'm5stack_basic.c' if profile in ('BASIC', 'CORE2') else 'm5stack_cardputer.c'
                source = (ROOT / 'main/boards' / filename).read_text()
                for header in ['driver/gpio.h', 'driver/spi_master.h', 'driver/i2c_master.h',
                               'esp_check.h', 'esp_heap_caps.h', 'esp_timer.h',
                               'freertos/FreeRTOS.h', 'freertos/task.h']:
                    source = source.replace(f'#include "{header}"', '')
                names = ['WIDTH', 'HEIGHT', 'X', 'Y', 'DC', 'CS', 'MOSI', 'SCLK', 'HOST', 'SDA', 'SCL', 'PMU']
                compile_run(stub + source + CHECK, [f'CONFIG_ESP_SDR_M5STACK_{profile}=1',
                            *(f'TEST_{key}={value}' for key, value in zip(names, values))])
