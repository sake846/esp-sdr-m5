"""Compile board drivers and inspect their SPI frames and Core2 I2C controls."""
import unittest

from host_test import COMPILER, ROOT, compile_run

STUB = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_NOT_FOUND 2
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
#define GPIO_MODE_OUTPUT 2
#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLDOWN_ONLY 2
#define GPIO_FLOATING 0
#define SPI2_HOST 2
#define SPI3_HOST 3
#define SPI_DMA_CH_AUTO 1
#define I2C_NUM_0 0
#define I2C_NUM_1 1
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 0
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define pdMS_TO_TICKS(x) (x)
typedef unsigned gpio_num_t;
enum {GPIO_NUM_3=3, GPIO_NUM_4=4, GPIO_NUM_5=5, GPIO_NUM_6=6, GPIO_NUM_7=7,
      GPIO_NUM_8=8, GPIO_NUM_9=9, GPIO_NUM_11=11, GPIO_NUM_13=13, GPIO_NUM_15=15,
      GPIO_NUM_35=35, GPIO_NUM_37=37, GPIO_NUM_39=39};
typedef void *spi_device_handle_t;
typedef uintptr_t i2c_master_dev_handle_t;
typedef void *i2c_master_bus_handle_t;
typedef struct {uint64_t pin_bit_mask; int mode, pull_up_en;} gpio_config_t;
typedef struct {int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num;
               size_t max_transfer_sz;} spi_bus_config_t;
typedef struct {int clock_speed_hz,mode,spics_io_num,queue_size;} spi_device_interface_config_t;
typedef struct {size_t length; const void *tx_buffer;} spi_transaction_t;
typedef struct {int i2c_port,sda_io_num,scl_io_num,clk_source,glitch_ignore_cnt;} i2c_master_bus_config_t;
typedef struct {int dev_addr_length,device_address,scl_speed_hz;} i2c_device_config_t;
static unsigned levels[64], command, dc, line, rows, madctl, column_windows;
static unsigned touch_x, touch_y, touch_count, touch_failure;
static uint8_t pmu[256];
static uint16_t pixels[320*240];
static int64_t esp_timer_get_time(void) { return 1000000; }
static void vTaskDelay(unsigned ticks) {}
static int gpio_config(const gpio_config_t *c) { return 0; }
static int gpio_set_level(unsigned pin, unsigned value) { levels[pin]=value; if(pin==TEST_DC)dc=value; return 0; }
static int gpio_get_level(unsigned pin) { return 1; }
static int gpio_set_direction(unsigned pin, unsigned mode) { return 0; }
static int gpio_set_pull_mode(unsigned pin, unsigned mode) { return 0; }
static void *heap_caps_calloc(size_t n, size_t size, unsigned caps) { assert(caps==3); return calloc(n,size); }
static int i2c_new_master_bus(const i2c_master_bus_config_t *c, i2c_master_bus_handle_t *b) {
    assert(c->sda_io_num==TEST_SDA && c->scl_io_num==TEST_SCL); *b=(void*)1; return 0;
}
static int i2c_master_bus_add_device(i2c_master_bus_handle_t b, const i2c_device_config_t *c,
                                    i2c_master_dev_handle_t *d) { *d=c->device_address; return 0; }
static int i2c_master_transmit(i2c_master_dev_handle_t d, const uint8_t *p, size_t n, int timeout) {
    assert(n==2); if(d==0x34)pmu[p[0]]=p[1]; return 0;
}
static int i2c_master_transmit_receive(i2c_master_dev_handle_t d, const uint8_t *p, size_t n,
                                      uint8_t *out, size_t size, int timeout) {
    assert(n==1);
    if(d==0x34){assert(size==1); *out=pmu[*p]; return 0;}
    assert(d==0x38);
    if(touch_failure)return -1;
    if(*p==0xa6){*out=0x12;return 0;}
    if(*p==0xa8){*out=0x11;return 0;}
    if(*p==2){*out=(uint8_t)touch_count;return 0;}
    assert(*p==3 && size==4);
    out[0]=(uint8_t)(touch_x>>8)|0x80;out[1]=(uint8_t)touch_x;
    out[2]=(uint8_t)(touch_y>>8)|0x40;out[3]=(uint8_t)touch_y;
    return 0;
}
static int spi_bus_initialize(unsigned host, const spi_bus_config_t *c, unsigned dma) {
    assert(host==TEST_HOST && dma==SPI_DMA_CH_AUTO);
    assert(c->mosi_io_num==TEST_MOSI && c->sclk_io_num==TEST_SCLK);
    assert(c->max_transfer_sz>=TEST_WIDTH*2); return 0;
}
static int spi_bus_add_device(unsigned host, const spi_device_interface_config_t *c, spi_device_handle_t *d) {
    assert(host==TEST_HOST && c->spics_io_num==TEST_CS); *d=(void*)1; return 0;
}
static int spi_device_polling_transmit(spi_device_handle_t d, const spi_transaction_t *t) {
    const uint8_t *p=t->tx_buffer; assert(d);
    if(!dc){assert(t->length==8); command=p[0];}
    else if(command==0x36){assert(t->length==8); madctl=*p;}
    else if(command==0x2a){
        column_windows++;
        assert(t->length==32 && ((p[0]<<8)|p[1])==TEST_X);
        assert(((p[2]<<8)|p[3])==TEST_X+TEST_WIDTH-1);
    }else if(command==0x2b){
        assert(t->length==32 && p[0]==p[2] && p[1]==p[3]);
        line=((p[0]<<8)|p[1])-TEST_Y; assert(line<TEST_HEIGHT);
    }else if(command==0x2c){
        assert(t->length==TEST_WIDTH*16);
        for(unsigned x=0;x<TEST_WIDTH;x++)pixels[line*TEST_WIDTH+x]=(uint16_t)((p[2*x]<<8)|p[2*x+1]);
        rows++;
    }
    return 0;
}
'''

CHECK = r'''
int main(void) {
    pmu[3]=TEST_PMU;
    m5stack_basic_init();
    assert(rows==TEST_HEIGHT && column_windows==1 && m5stack_basic_rate()==0);
    assert(m5stack_basic_span()==80 && m5stack_steps[controls.step]==10);
    assert(m5stack_basic_fft_points()==1024 && m5stack_basic_average_count()==16);
#if CONFIG_ESP_SDR_M5STACK_CORE2
    assert(levels[4]==1);
    if(TEST_PMU==0x4a){assert((pmu[0x90]&0x1a)==0x1a && pmu[0x96]==28);}
    else {assert((pmu[0x12]&6)==6 && (pmu[0x96]&2) && pmu[0x27]==103);}
    const unsigned xs[]={0,106,107,212,213,319};
    for(unsigned j=0;j<6;j++){
        touch_x=xs[j];touch_y=207;touch_count=1;assert(core2_touch_buttons()==0);
        touch_y=208;assert(core2_touch_buttons()==(1u<<(j/2)));
    }
    touch_count=0;assert(core2_touch_buttons()==0);
    touch_count=1;touch_failure=1;assert(core2_touch_buttons()==0);touch_failure=0;
#elif CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    assert(levels[4]==1 && levels[27]==1 && madctl==0x08);
#else
    assert(levels[38]==1 && madctl==0x6c);
#endif
    uint8_t bins[8192]={0};
    bins[127]=255;
    m5stack_basic_draw(2412,bins,256,false,true,0);
    assert(rows==2*TEST_HEIGHT && column_windows==2 && waterfall_count==1);
    assert(pixels[WATERFALL_TOP*TEST_WIDTH+PLOT_LEFT]==0xffff);
    memset(bins,0,sizeof(bins));
    m5stack_basic_draw(2412,bins,256,false,true,100);
    assert(pixels[WATERFALL_TOP*TEST_WIDTH+PLOT_LEFT]==waterfall_color(0));
    assert(pixels[(WATERFALL_TOP+1)*TEST_WIDTH+PLOT_LEFT]==0xffff);
    m5stack_basic_draw(2412,bins,256,true,false,50);
    assert(waterfall_count==2); /* PC and failed captures freeze history. */
    m5stack_basic_draw(2413,bins,256,false,true,0);
    assert(waterfall_count==1 && pixels[(WATERFALL_TOP+1)*TEST_WIDTH+PLOT_LEFT]==0);
    /* A 2 MHz view crops the central 32 bins of the 16 MHz acquisition. */
    controls.rate=0;bins[127]=255;bins[15]=200;
    m5stack_basic_draw(2413,bins,256,false,true,0);
    assert(waterfall_count==1);
    assert(pixels[WATERFALL_TOP*TEST_WIDTH+PLOT_LEFT]==waterfall_color(60));
    /* Peak reduction preserves a narrow signal in a 4096-bin spectrum. */
    memset(bins,0,sizeof(bins));controls.rate=3;
    bins[TEST_FFT/2-2]=255;
    m5stack_basic_draw(2413,bins,TEST_FFT,false,true,0);
    assert(waterfall_count==1 && pixels[WATERFALL_TOP*TEST_WIDTH+PLOT_LEFT]==0xffff);
    controls.fft=0; controls.menu=M5_MENU_NONE;
    memset(bins,0,sizeof(bins));
    for(unsigned j=64;j<96;j++)bins[j]=128;
    m5stack_basic_draw(2413,bins,256,false,true,0);
    unsigned x=(48*PLOT_WIDTH+255)/256;
    unsigned height=spectrum_dbfs_to_display_value(spectrum_code_to_dbfs(128),-84,80)*
                    (SPECTRUM_BOTTOM-SPECTRUM_TOP)/255;
    unsigned trace_y=SPECTRUM_BOTTOM-height;
    unsigned step=TEST_WIDTH==320 ? 13 : 6;
    unsigned grid_y=SPECTRUM_TOP+((trace_y+1-SPECTRUM_TOP+step-1)/step)*step;
    for(unsigned d=0;d<=8;d++)assert(x!=(d*(PLOT_WIDTH-1)+4)/8);
    assert(pixels[trace_y*TEST_WIDTH+PLOT_LEFT+x]==0x2cbf);
    assert(pixels[(trace_y+1)*TEST_WIDTH+PLOT_LEFT+x]==0x012c);
    assert(pixels[grid_y*TEST_WIDTH+PLOT_LEFT+x]==0x11ae);
    assert(pixels[(trace_y-1)*TEST_WIDTH+PLOT_LEFT+x]==
           ((trace_y-1-SPECTRUM_TOP)%step==0 ? 0x2104 : 0));
    assert(pixels[SPECTRUM_BOTTOM*TEST_WIDTH+PLOT_LEFT+x]==0x4208);
    m5stack_basic_draw(2413,bins,256,false,false,0);
    assert(pixels[(trace_y+1)*TEST_WIDTH+PLOT_LEFT+x]==0);
    assert(pixels[grid_y*TEST_WIDTH+PLOT_LEFT+x]==0x2104);
    controls.dpx=true;
    m5stack_basic_draw(2413,bins,256,false,true,0);
    assert(pixels[(trace_y+1)*TEST_WIDTH+PLOT_LEFT+x]==0); /* DPX has no under-curve fill. */
    assert(pixels[grid_y*TEST_WIDTH+PLOT_LEFT+x]==0x2104);
    controls.dpx=false;
    density_mode(2413,false);
#if CONFIG_ESP_SDR_M5STACK_CORE2 || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS2
    controls.menu = M5_MENU_AGC;
    m5stack_basic_draw(2413,bins,TEST_FFT,false,false,0);
    unsigned margin = TEST_WIDTH > 200 ? 32 : 4;
    assert(pixels[(TEST_HEIGHT/2-34)*TEST_WIDTH+margin] == COLOR_SRDECK_YELLOW_GREEN);
    assert(pixels[(TEST_HEIGHT/2)*TEST_WIDTH+margin+1] == 0x0861);
#endif
#if !CONFIG_ESP_SDR_M5STACK_CORE2
    free(waterfall);
#endif
    return 0;
}
'''


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class BoardDisplays(unittest.TestCase):
    def test_core2_power_touch_and_compact_display_geometry_and_history(self):
        profiles = [
            ('CORE2', 320, 240, 0, 0, 15, 5, 23, 18, 3, 21, 22, 0x03, 4096),
            ('CORE2', 320, 240, 0, 0, 15, 5, 23, 18, 3, 21, 22, 0x4a, 4096),
            ('CARDPUTER', 240, 135, 40, 53, 34, 37, 35, 36, 3, 8, 9, 0, 4096),
            ('CARDPUTER_ADV', 240, 135, 40, 53, 34, 37, 35, 36, 3, 8, 9, 0, 4096),
            ('STICKC_PLUS2', 135, 240, 52, 40, 14, 5, 15, 13, 2, 0, 0, 0, 4096),
        ]
        for profile, *values in profiles:
            with self.subTest(profile=profile, pmu=values[-2]):
                file = 'm5stack_basic.c' if profile == 'CORE2' else 'm5stack_cardputer.c'
                source = (ROOT / 'main/boards' / file).read_text()
                for header in ['driver/gpio.h', 'driver/spi_master.h', 'driver/i2c_master.h',
                               'esp_check.h', 'esp_heap_caps.h', 'esp_timer.h',
                               'freertos/FreeRTOS.h', 'freertos/task.h']:
                    source = source.replace(f'#include "{header}"', '')
                names = ['WIDTH', 'HEIGHT', 'X', 'Y', 'DC', 'CS', 'MOSI', 'SCLK', 'HOST',
                         'SDA', 'SCL', 'PMU', 'FFT']
                compile_run(STUB + source + CHECK, [f'CONFIG_ESP_SDR_M5STACK_{profile}=1',
                            *(f'TEST_{k}={v}' for k, v in zip(names, values))])
