"""Exercise production LILYGO wiring, controls, scanlines and e-paper refreshes."""
import json
import unittest

from host_test import COMPILER, ROOT, compile_run

BOARDS = [
    ('T_DISPLAY_S3_PRO', 480, 222, 17, 18, 39, 9, 0, 49, 5, 6),
    ('T_PAGER', 480, 222, 34, 35, 38, 37, 0, 49, 3, 2),
    ('T_EMBED_SI4732', 320, 170, 11, 12, 10, 13, 0, 35, 18, 8),
    ('T_EMBED_CC1101', 320, 170, 9, 11, 41, 16, 0, 35, 8, 18),
    ('T_EMBED_CC1101_PLUS', 320, 170, 9, 11, 41, 16, 0, 35, 8, 18),
    ('T_DECK', 320, 240, 41, 40, 12, 11, 0, 0, 18, 8),
    ('T_DECK_PRO', 240, 320, 33, 36, 34, 35, 0, 0, 13, 14),
    ('T_DECK_PRO_V11', 240, 320, 33, 36, 34, 35, 0, 0, 13, 14),
]

STUB = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>
static int battery_percent = -1;
int board_battery_percent(void) { return battery_percent; }
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERROR_CHECK(x) assert((x)==0)
#define GPIO_MODE_OUTPUT 2
#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_ENABLE 1
#define GPIO_INTR_ANYEDGE 3
#define SPI2_HOST 2
#define SPI_DMA_CH_AUTO 1
#define I2C_NUM_0 0
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 0
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xffffffffu
#define pdTRUE 1
#define pdPASS 1
#define portMUX_TYPE int
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define portENTER_CRITICAL_ISR(x) ((void)0)
#define portEXIT_CRITICAL_ISR(x) ((void)0)
typedef int esp_err_t;
typedef void *spi_device_handle_t;
typedef uintptr_t i2c_master_dev_handle_t;
typedef void *i2c_master_bus_handle_t;
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef struct {uint64_t pin_bit_mask; int mode,pull_up_en,intr_type;} gpio_config_t;
typedef struct {int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num;
               size_t max_transfer_sz;} spi_bus_config_t;
typedef struct {int clock_speed_hz,mode,spics_io_num,queue_size;} spi_device_interface_config_t;
typedef struct {size_t length; const void *tx_buffer;} spi_transaction_t;
typedef struct {int i2c_port,sda_io_num,scl_io_num,clk_source,glitch_ignore_cnt;
               struct {bool enable_internal_pullup;} flags;} i2c_master_bus_config_t;
typedef struct {int dev_addr_length,device_address,scl_speed_hz;} i2c_device_config_t;
static unsigned levels[64], dc, cmd, row_number, rows, flushes, madctl;
static uint8_t registers[128][256], event_fifo[16], event_count, deck_key;
static uint16_t pixels[480*320];
static unsigned busy_stuck, mutex_taken, task_calls, refreshes, full_refreshes, partial_refreshes;
static uint64_t pressed_inputs;
static unsigned new_planes, old_planes, image_bytes, last_caps;
static int64_t now;
static jmp_buf worker_exit;
static int gpio_config(const gpio_config_t *c){return 0;}
static int gpio_set_level(unsigned p,unsigned v){assert(p<64);levels[p]=v;if(p==TEST_DC)dc=v;return 0;}
static int gpio_set_direction(unsigned p,unsigned m){assert(p<64);return 0;}
static int gpio_get_level(unsigned p){return p==37 ? !busy_stuck : !(pressed_inputs & (UINT64_C(1)<<p));}
static int gpio_install_isr_service(unsigned f){return ESP_ERR_INVALID_STATE;}
static int gpio_isr_handler_add(unsigned p,void (*cb)(void*),void *arg){return 0;}
static int64_t esp_timer_get_time(void){return now*1000;}
static void vTaskDelay(unsigned ms){now+=ms;}
static void *heap_caps_calloc(size_t n,size_t s,unsigned caps){assert(caps==3 || caps==6);last_caps=caps;return calloc(n,s);}
static int i2c_new_master_bus(const i2c_master_bus_config_t *c,i2c_master_bus_handle_t *b){
 assert(c->sda_io_num==TEST_SDA && c->scl_io_num==TEST_SCL);*b=(void*)1;return 0;
}
static int i2c_master_bus_add_device(i2c_master_bus_handle_t b,const i2c_device_config_t *c,i2c_master_dev_handle_t *d){
 *d=c->device_address;return 0;
}
static int i2c_master_transmit(i2c_master_dev_handle_t d,const uint8_t *p,size_t n,int t){
 assert(n==2);registers[d][p[0]]=p[1];return 0;
}
static int i2c_master_transmit_receive(i2c_master_dev_handle_t d,const uint8_t *p,size_t n,uint8_t *out,size_t size,int t){
 if(d==0x6a){assert(n==1 && size==1);*out=registers[d][*p];return 0;}
 assert(d==0x34 && n==1 && size==1);
 if(*p==3)*out=event_count;
 else {assert(*p==4 && event_count);*out=event_fifo[--event_count];}return 0;
}
static int i2c_master_receive(i2c_master_dev_handle_t d,uint8_t *out,size_t n,int t){
 assert(d==0x55 && n==1);*out=deck_key;deck_key=0;return 0;
}
static int spi_bus_initialize(unsigned h,const spi_bus_config_t *c,unsigned dma){
 assert(h==2 && c->mosi_io_num==TEST_MOSI && c->sclk_io_num==TEST_SCLK);return 0;
}
static int spi_bus_add_device(unsigned h,const spi_device_interface_config_t *c,spi_device_handle_t *d){
 assert(h==2 && c->spics_io_num==TEST_CS);*d=(void*)1;return 0;
}
static int spi_device_polling_transmit(spi_device_handle_t d,const spi_transaction_t *t){
 const uint8_t *p=t->tx_buffer;
 if(!dc){
   assert(t->length==8);cmd=*p;
   if(TEST_EPAPER && cmd==0x12)refreshes++;
   if(TEST_EPAPER && cmd==0x10)old_planes++;
   if(TEST_EPAPER && cmd==0x13)new_planes++;
 }else if(TEST_EPAPER){
   if(cmd==0xe5){if(*p==0x5a)full_refreshes++;else {assert(*p==0x79);partial_refreshes++;}}
   if(cmd==0x10 || cmd==0x13){assert(t->length==1920);image_bytes+=240;}
 }else if(cmd==0x36)madctl=*p;
 else if(cmd==0x2a){assert(((p[0]<<8)|p[1])==TEST_X && ((p[2]<<8)|p[3])==TEST_X+TEST_WIDTH-1);}
 else if(cmd==0x2b){row_number=((p[0]<<8)|p[1])-TEST_Y;assert(row_number<TEST_HEIGHT);}
 else if(cmd==0x2c){
   assert(t->length==TEST_WIDTH*16);rows++;
   for(unsigned x=0;x<TEST_WIDTH;x++)pixels[row_number*TEST_WIDTH+x]=(p[x*2]<<8)|p[x*2+1];
 }return 0;
}
static SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void*)1;}
static int xSemaphoreTake(SemaphoreHandle_t s,unsigned timeout){if(mutex_taken)return 0;mutex_taken=1;return 1;}
static int xSemaphoreGive(SemaphoreHandle_t s){assert(mutex_taken);mutex_taken=0;return 1;}
static int xTaskCreate(void (*fn)(void*),const char *n,unsigned stack,void *arg,unsigned prio,TaskHandle_t *out){*out=(void*)2;return 1;}
static void xTaskNotifyGive(TaskHandle_t t){flushes++;}
static unsigned ulTaskNotifyTake(unsigned clear,unsigned timeout){if(++task_calls>7)longjmp(worker_exit,1);return 1;}
static void vTaskSuspend(void *t){longjmp(worker_exit,2);}
'''

CHECK = r'''
int main(void){
 registers[0x6a][3]=registers[0x6a][7]=0xff;
 standalone_display_init();
 assert(standalone_display_fft_points()==1024);
#if CONFIG_ESP_SDR_LILYGO_T_DISPLAY_S3_PRO
 assert((registers[0x6a][3]&0x30)==0 && (registers[0x6a][7]&0x30)==0);
 assert((registers[0x6a][2]&0xc0)==0xc0);
 assert(levels[14] && levels[46] && levels[13] && madctl==0x68);
#elif CONFIG_ESP_SDR_LILYGO_T_PAGER
 assert(registers[0x20][2]==0x84 && registers[0x20][3]==1);
 assert(registers[0x34][0x1d]==0x0f && registers[0x34][0x1f]==3);
 assert(levels[21] && levels[36] && levels[39] && madctl==0xe8);
#elif CONFIG_ESP_SDR_LILYGO_T_EMBED_SI4732
 assert(levels[46] && !levels[16] && levels[17] && levels[39]);
#elif CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101 || CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101_PLUS
 assert(levels[15] && levels[12] && levels[13]);
#if CONFIG_ESP_SDR_LILYGO_T_EMBED_CC1101_PLUS
 assert(levels[44] && !levels[43]);
#endif
#elif CONFIG_ESP_SDR_LILYGO_T_DECK
 assert(levels[10] && levels[9] && levels[39]);
#endif
 uint8_t bins[4096]={0};bins[127]=255;
 standalone_display_draw(2412,bins,256,false,true,0);
 assert(waterfall_count==1);
 controls.density_mode=true;density_mode(2412,false);assert(density && density_width>0);
 assert(last_caps==(CONFIG_SPIRAM ? 6 : 3));
 controls.density_mode=false;density_mode(2412,false);assert(!density);
#if LILYGO_EPAPER
 assert(flushes==2);
 assert((epaper_frame[0]&0xf0)==0xf0); /* Black UI background becomes white paper. */
 assert(!(epaper_frame[(1*SCREEN_WIDTH+4)/8] & (0x80>>4))); /* Ink in the title banner. */
 epaper_tx[0]=123;epaper_frame[0]=0;
 standalone_device_lilygo_ats_mini_display_flush();assert(!epaper_pending[0] && epaper_tx[0]==123 && flushes==3);
 int result=setjmp(worker_exit);
 if(!result)epaper_worker(NULL);
 assert(result==1 && refreshes==7 && full_refreshes==2 && partial_refreshes==5);
 assert(new_planes==7 && old_planes==9 && image_bytes==16*9600);
 busy_stuck=1;task_calls=0;now=0;result=setjmp(worker_exit);
 if(!result)epaper_worker(NULL);
 assert(result==2 && now>=10000 && !mutex_taken); /* BUSY timeout releases the image. */
#else
 assert(rows==TEST_HEIGHT*2);
 assert(pixels[WATERFALL_TOP*TEST_WIDTH+PLOT_LEFT]==0xffff);
#endif
 unsigned mhz=2412;controls.menu=M5_MENU_NONE;
#ifdef ENCODER_A
 encoder_count=8;mhz=standalone_device_lilygo_ats_mini_poll(&controls,mhz,false);assert(mhz==2432);
 encoder_count=-4;mhz=standalone_device_lilygo_ats_mini_poll(&controls,mhz,true);assert(mhz==2432 && !encoder_count);
#endif
 if(keyboard){
#if CONFIG_ESP_SDR_LILYGO_T_DECK
   deck_key='a';
#else
   event_fifo[0]=0x80|20;event_count=1; /* A: row 1, physical column 9. */
#endif
   now+=20;unsigned previous=mhz;mhz=standalone_device_lilygo_ats_mini_poll(&controls,mhz,false);assert(mhz==previous-10);
#if CONFIG_ESP_SDR_LILYGO_T_DECK
   deck_key='d';
#else
   event_fifo[0]=0x80|18;event_count=1;
#endif
   now+=20;assert(standalone_device_lilygo_ats_mini_poll(&controls,mhz,true)==mhz); /* PC lease drains input. */
 }
 controls.menu=M5_MENU_FFT;controls.b_long=false;
 pressed_inputs=UINT64_C(1)<<MENU_BUTTON;now+=50;
 standalone_device_lilygo_ats_mini_poll(&controls,mhz,false);now+=31;standalone_device_lilygo_ats_mini_poll(&controls,mhz,false);
 now+=601;standalone_device_lilygo_ats_mini_poll(&controls,mhz,false);assert(controls.menu==M5_MENU_NONE);
 pressed_inputs=0;now+=1;standalone_device_lilygo_ats_mini_poll(&controls,mhz,false);
 now+=31;standalone_device_lilygo_ats_mini_poll(&controls,mhz,false);assert(controls.menu==M5_MENU_NONE);
 controls.menu=M5_MENU_NONE; battery_percent=100;
 const char *telemetry[]={"BAT 100%", "CPU 49%"};
 for(unsigned frame=0;frame<2;frame++) {
   now=(frame+1)*5000; standalone_display_draw(2412,bins,256,false,true,49);
   unsigned left=SCREEN_WIDTH-text_width(telemetry[frame])-4;
   for(unsigned y=1;y<12;y++) {
     memset(row,0,sizeof(row)); text_row(y-1,left,telemetry[frame],0xffff);
     for(unsigned x=left;x<SCREEN_WIDTH;x++) {
#if LILYGO_EPAPER
       assert(!!(epaper_frame[y*(SCREEN_WIDTH/8)+x/8] & (0x80u>>(x&7))) == !row[x]);
#else
       assert(pixels[y*SCREEN_WIDTH+x] == __builtin_bswap16(row[x]));
#endif
     }
   }
 }
#if LILYGO_EPAPER
 free(epaper_frame);
#endif
 free(waterfall);return 0;
}
'''


class LilygoProfiles(unittest.TestCase):
    def test_catalog_defaults_and_docs(self):
        profiles = {p['id']: p for p in json.loads((ROOT/'firmware-targets.json').read_text())['profiles']}
        for board, *_ in BOARDS:
            name = 'lilygo-' + board.lower().replace('_', '-')
            profile = profiles[name]
            self.assertEqual(profile['target'], 'esp32s3')
            defaults = (ROOT/profile['sdkconfig']).read_text()
            self.assertIn(f'CONFIG_ESP_SDR_LILYGO_{board}=y', defaults)
            self.assertIn('CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y', defaults)
            self.assertIn('# CONFIG_ESP_SDR_UART_ENABLED is not set', defaults)
            self.assertIn(name, (ROOT/'docs/lilygo.md').read_text(encoding='utf-8'))

    @unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
    def test_board_io_rendering_and_receiver_handoff(self):
        sources = '\n'.join((ROOT/'main/boards'/f).read_text() for f in ['standalone_device_lilygo_ats_mini.c', 'standalone_display.c'])
        headers = ['driver/gpio.h', 'driver/spi_master.h', 'driver/i2c_master.h',
                   'esp_check.h', 'esp_heap_caps.h', 'esp_timer.h',
                   'freertos/FreeRTOS.h', 'freertos/task.h', 'freertos/semphr.h']
        for h in headers:
            sources = sources.replace(f'#include "{h}"', '')
        for board, *values in BOARDS:
            with self.subTest(board=board):
                names = ['WIDTH', 'HEIGHT', 'MOSI', 'SCLK', 'CS', 'DC', 'X', 'Y', 'SDA', 'SCL']
                compile_run(STUB+sources+CHECK, ['CONFIG_ESP_SDR_LILYGO=1',
                    f'CONFIG_ESP_SDR_LILYGO_{board}=1', f'TEST_EPAPER={int(board.startswith("T_DECK_PRO"))}',
                    f'CONFIG_SPIRAM={int("PRO" in board or board == "T_PAGER")}',
                    *(f'TEST_{k}={v}' for k,v in zip(names,values))])

    @unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
    def test_encoder_invalid_edges_key_bounds_and_monochrome(self):
        compile_run(r'''
#include <assert.h>
#include "standalone_device_lilygo_ats_mini_controls.h"
int main(void){
 assert(standalone_device_lilygo_ats_mini_encoder_delta(0,3)==0 && standalone_device_lilygo_ats_mini_encoder_delta(3,0)==0);
 assert(standalone_device_lilygo_ats_mini_encoder_delta(0,1)+standalone_device_lilygo_ats_mini_encoder_delta(1,3)+
        standalone_device_lilygo_ats_mini_encoder_delta(3,2)+standalone_device_lilygo_ats_mini_encoder_delta(2,0)==-4);
 m5stack_controls_t s={0};m5stack_controls_defaults(&s);
 assert(lilygo_key(&s,RX_FREQ_MIN,'a',false)==RX_FREQ_MIN);
 assert(lilygo_key(&s,RX_FREQ_MAX,'d',false)==RX_FREQ_MAX);
 lilygo_key(&s,2412,' ',false);assert(s.menu==M5_MENU_STEP);
 lilygo_key(&s,2412,'D',false);assert(s.step==3);
 lilygo_key(&s,2412,'q',true);assert(s.menu==M5_MENU_STEP);
 lilygo_key(&s,2412,'q',false);assert(s.menu==M5_MENU_NONE);
 assert(lilygo_tca_key(0x80|20)=='a' && lilygo_tca_key(0x80|18)=='d');
 assert(lilygo_tca_key(0x80|40)==' ' && !lilygo_tca_key(20));
 assert(!lilygo_tca_key(0x80) && !lilygo_tca_key(0xff));
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){
   assert(!lilygo_epaper_ink(0,x,y));assert(lilygo_epaper_ink(0xffff,x,y));
 }
 return 0;
}
''', ['CONFIG_ESP_SDR_LILYGO=1'])
