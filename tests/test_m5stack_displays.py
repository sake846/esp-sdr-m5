"""Check shared per-device display geometry; STUB supports host render harnesses."""
import unittest

from host_test import COMPILER, ROOT, compile_run

STUB = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
static int battery_percent = -1;
int board_battery_percent(void) { return battery_percent; }
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
      GPIO_NUM_25=25, GPIO_NUM_35=35, GPIO_NUM_37=37, GPIO_NUM_38=38, GPIO_NUM_39=39};
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
static int64_t display_now = 1000000;
static int64_t esp_timer_get_time(void) { return display_now; }
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

@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class BoardDisplayProfiles(unittest.TestCase):
    def test_shared_geometry_orientation_and_scaling(self):
        profiles = [
            ('BASIC', 320, 240, 1, 1, 0),
            ('CORE2', 320, 240, 1, 1, 0),
            ('CORES3', 320, 240, 1, 1, 0),
            ('CARDPUTER', 240, 135, 1, 0, 0),
            ('CARDPUTER_ADV', 240, 135, 1, 0, 0),
            ('STICKC_PLUS2', 135, 240, 1, 0, 1),
            ('STICKC_PLUS', 135, 240, 1, 0, 1),
            ('STICKS3', 135, 240, 1, 0, 1),
            ('STICKC', 80, 160, 1, 0, 1),
            ('DIN_METER', 240, 135, 1, 0, 0),
            ('PAPER_COLOR', 300, 200, 2, 0, 0),
            ('PAPERS3', 480, 270, 2, 0, 0),
        ]
        source = r'''
#include <assert.h>
#include <string.h>
#include "standalone_device_m5stack_display.h"
int main(void) {
    assert(SCREEN_WIDTH == TEST_WIDTH && SCREEN_HEIGHT == TEST_HEIGHT);
    assert(M5_DISPLAY_SCALE == TEST_SCALE);
    assert(M5_DISPLAY_LAYOUT_BASIC == TEST_BASIC);
    assert(M5_DISPLAY_LAYOUT_STICK == TEST_STICK);
    assert(strcmp(M5_BOARD_NAME, TEST_NAME) == 0);
    return 0;
}
'''
        names = {
            'BASIC': 'BASIC', 'CORE2': 'CORE2', 'CORES3': 'CORES3',
            'CARDPUTER': 'CARDPUTER', 'CARDPUTER_ADV': 'CARDPUTER ADV',
            'STICKC_PLUS2': 'STICKC PLUS2', 'STICKC_PLUS': 'STICKC PLUS',
            'STICKS3': 'STICKS3', 'STICKC': 'STICKC', 'DIN_METER': 'DIN METER',
            'PAPER_COLOR': 'PAPER COLOR', 'PAPERS3': 'PAPERS3',
        }
        for board, width, height, scale, basic, stick in profiles:
            with self.subTest(board=board):
                compile_run(source, [f'CONFIG_ESP_SDR_M5STACK_{board}=1',
                            f'TEST_WIDTH={width}', f'TEST_HEIGHT={height}',
                            f'TEST_SCALE={scale}', f'TEST_BASIC={basic}', f'TEST_STICK={stick}',
                            f'TEST_NAME="{names[board]}"'])
