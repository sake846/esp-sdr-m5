"""Benchmark DPX ingestion/rendering with mocked SPI; excludes FFT computation and device timing."""
import json
from pathlib import Path
import statistics
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from host_test import compile_run
from test_m5stack_displays import STUB

OUT = ROOT / 'artifacts/dpx-benchmark'
OUT.mkdir(parents=True, exist_ok=True)
RUN = 'current'
PROFILES = [
    ('BASIC', 320, 240, 0, 0, 27, 14, 23, 18, 3, 0, 0, 0, 8192),
    ('CARDPUTER', 240, 135, 40, 53, 34, 37, 35, 36, 3, 8, 9, 0, 4096),
    ('STICKC_PLUS2', 135, 240, 52, 40, 14, 5, 15, 13, 2, 0, 0, 0, 8192),
]
CHECK = r'''
#include <time.h>
static uint8_t snapshots[16][8192];
int main(void) {
    FILE *out = fopen(RESULT_PATH, "w"); assert(out);
    unsigned seed = 1;
    for (unsigned a=0;a<16;a++) for (unsigned j=0;j<8192;j++) {
        seed = seed*1664525u+1013904223u; snapshots[a][j]=(uint8_t)(seed>>24);
    }
    m5stack_basic_init();
    for (unsigned big=0;big<2;big++) for (unsigned zoom=0;zoom<2;zoom++) {
        controls.fft = big ? TEST_FFT_INDEX : 2;
        controls.rate = zoom ? 0 : 3;
        controls.dpx = true;
        density_mode(2412, false);
        unsigned n=m5stack_basic_fft_points(), frames=800;
        for (unsigned repeat=0;repeat<3;repeat++) {
            density_clear();
            clock_t start=clock();
            for (unsigned frame=0;frame<frames;frame++) {
                for (unsigned fft=0;fft<16;fft++)
                    m5stack_basic_spectrum(2412,snapshots[(frame+fft)%16],n,30,-1,1);
                m5stack_basic_draw(2412,snapshots[frame%16],n,false,true,25);
                test_now+=130000;
            }
            double ms=1000.0*(clock()-start)/CLOCKS_PER_SEC;
            uint32_t hash=2166136261u, history=2166136261u;
            for(unsigned i=0;i<SCREEN_WIDTH*SCREEN_HEIGHT;i++)hash=(hash^pixels[i])*16777619u;
            for(unsigned level=0;level<64;level++)for(unsigned x=0;x<PLOT_WIDTH;x++)
                history=(history^BENCH_CELL(level,x))*16777619u;
            fprintf(out,"{\"nfft\":%u,\"span\":%u,\"frames\":%u,\"host_ms\":%.3f,"
                        "\"density_bytes\":%zu,\"pixels\":%u,\"history\":%u}\n",
                        n,m5stack_basic_span(),frames,ms,density_bytes,hash,history);
        }
    }
    assert(!fclose(out));
    return 0;
}
'''

stub = STUB.replace('static int64_t esp_timer_get_time(void) { return 1000000; }',
                    'static int64_t test_now=1000000; static int64_t esp_timer_get_time(void) { return test_now; }')
stub = stub.replace('static void *heap_caps_calloc(size_t n, size_t size, unsigned caps) { assert(caps==3); return calloc(n,size); }',
                    'static size_t density_bytes; static void *heap_caps_calloc(size_t n, size_t size, unsigned caps) { assert(caps==3); density_bytes=n*size; return calloc(n,size); }')
results = []
for board, *values in PROFILES:
    filename = 'm5stack_basic.c' if board == 'BASIC' else 'm5stack_cardputer.c'
    source = (ROOT / 'main/boards' / filename).read_text()
    for header in ['driver/gpio.h', 'driver/spi_master.h', 'driver/i2c_master.h',
                   'esp_check.h', 'esp_heap_caps.h', 'esp_timer.h',
                   'freertos/FreeRTOS.h', 'freertos/task.h']:
        source = source.replace(f'#include "{header}"', '')
    access = '#define BENCH_CELL(level,x) density[(level)*density_width + (x)*density_width/PLOT_WIDTH]\n'
    names=['WIDTH','HEIGHT','X','Y','DC','CS','MOSI','SCLK','HOST','SDA','SCL','PMU','FFT']
    output=OUT/f'{RUN}-{board}.jsonl'
    compile_run(stub+source+access+CHECK,[f'CONFIG_ESP_SDR_M5STACK_{board}=1',
                *(f'TEST_{k}={v}' for k,v in zip(names,values)),
                f'TEST_FFT_INDEX={5 if values[-1]==8192 else 4}',
                'RESULT_PATH='+json.dumps(output.as_posix())])
    rows=[json.loads(line) for line in output.read_text().splitlines()]
    for n,span in sorted({(r['nfft'],r['span']) for r in rows}):
        group=[r for r in rows if r['nfft']==n and r['span']==span]
        assert len(group)==3 and len({(r['pixels'],r['history']) for r in group})==1
        results.append({'board':board,'ffts_per_frame':16,**group[0],'host_ms':statistics.median(r['host_ms'] for r in group)})
    print(board, 'completed', flush=True)
(OUT/f'{RUN}.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
