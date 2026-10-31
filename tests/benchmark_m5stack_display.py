"""Measure Basic rendering on the host; SDK/SPI calls use the regression mocks."""
from host_test import COMPILER, ROOT, compile_run
from test_m5stack_displays import STUB

source = (ROOT / 'main/boards/standalone_display.c').read_text()
source += (ROOT / 'main/boards/standalone_device_m5stack_basic_ili9341.c').read_text()
for header in ['driver/gpio.h', 'driver/spi_master.h', 'esp_check.h', 'esp_timer.h', 'esp_heap_caps.h',
               'freertos/FreeRTOS.h', 'freertos/task.h']:
    source = source.replace(f'#include "{header}"', '')

stub = 'static unsigned transactions, formats;\n' + STUB.replace(
    'const uint8_t *p=t->tx_buffer;', 'transactions++; const uint8_t *p=t->tx_buffer;')
instrumentation = r'''
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
static int count_snprintf(char *p, size_t n, const char *fmt, ...) {
    va_list args; va_start(args, fmt); formats++;
    int result = vsnprintf(p, n, fmt, args); va_end(args); return result;
}
#define snprintf count_snprintf
'''
benchmark = r'''
int main(void) {
    uint8_t bins[8192]; unsigned seed=1;
    for (unsigned j=0;j<8192;j++) {seed=seed*1664525+1013904223;bins[j]=(uint8_t)(seed>>24);}
    standalone_display_init();
    for (unsigned repeat=0;repeat<3;repeat++) {
        transactions=formats=0;
        clock_t start=clock();
        for(unsigned j=0;j<1000;j++) {
            controls.rate=(j/50)%6;
            standalone_display_draw(2412+(j/100)%3,bins,8192,false,true,j%101);
        }
        double ms=1000.0*(clock()-start)/CLOCKS_PER_SEC;
        uint32_t hash=2166136261u;
        for(unsigned j=0;j<320*240;j++)hash=(hash^pixels[j])*16777619u;
        assert(transactions<=962000 && formats<=5000);
        printf("{\"frames\":1000,\"host_ms\":%.3f,\"spi_transactions\":%u,\"formats\":%u,\"pixel_hash\":%u}\n",
               ms,transactions,formats,hash);
    }
    return 0;
}
'''

if __name__ == '__main__':
    if not COMPILER:
        raise SystemExit('Host C compiler unavailable')
    compile_run(stub + instrumentation + source + benchmark,
                ['CONFIG_ESP_SDR_M5STACK=1', 'CONFIG_ESP_SDR_M5STACK_BASIC=1',
                 'CONFIG_ESP_SDR_M5STACK_LCD_ILI9341=1', 'TEST_DC=27', 'TEST_HOST=3',
                 'TEST_MOSI=23', 'TEST_SCLK=18', 'TEST_WIDTH=320', 'TEST_HEIGHT=240',
                 'TEST_CS=14', 'TEST_X=0', 'TEST_Y=0', 'TEST_SDA=0', 'TEST_SCL=0'])
