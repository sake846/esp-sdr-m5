"""Run the ATS-mini LCD/control code with delayed i80 DMA and panel ID reads."""
import json
import unittest

from host_test import COMPILER, ROOT, compile_run
from test_lilygo import STUB, CHECK

ATS_STUB = r'''
#define ESP_ERR_TIMEOUT 3
#define LCD_CLK_SRC_DEFAULT 0
#define pdFALSE 0
typedef int BaseType_t;
typedef unsigned gpio_num_t;
typedef void *esp_lcd_panel_io_handle_t;
typedef void *esp_lcd_i80_bus_handle_t;
typedef struct {unsigned unused;} esp_lcd_panel_io_event_data_t;
typedef struct {int dc_gpio_num,wr_gpio_num,clk_src,data_gpio_nums[16];
 size_t bus_width,max_transfer_bytes,dma_burst_size;} esp_lcd_i80_bus_config_t;
typedef struct {
 int cs_gpio_num;unsigned pclk_hz;size_t trans_queue_depth;
 bool (*on_color_trans_done)(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void*);
 void *user_ctx;int lcd_cmd_bits,lcd_param_bits;
 struct {unsigned dc_idle_level,dc_cmd_level,dc_dummy_level,dc_data_level;} dc_levels;
} esp_lcd_panel_io_i80_config_t;
static unsigned ats_modes[64], ats_read_count, ats_i2c_started, ats_pending_size;
static unsigned ats_ready, ats_completions, ats_window_resets, ats_queued_hash;
static const uint8_t *ats_pending;
static uint8_t ats_registers[256], ats_register_seen[256];
static bool (*ats_callback)(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void*);
static void *ats_callback_context;
static unsigned ats_hash(const uint8_t *data,unsigned size){
 unsigned hash=0;for(unsigned i=0;i<size;i++)hash=hash*33+data[i];return hash;
}
static void esp_rom_delay_us(unsigned us){}
static SemaphoreHandle_t xSemaphoreCreateBinary(void){return (void*)3;}
static int xSemaphoreGiveFromISR(SemaphoreHandle_t s,BaseType_t *woken){
 assert(s==(void*)3 && !ats_ready);ats_ready=1;*woken=1;return 1;
}
static int esp_lcd_new_i80_bus(const esp_lcd_i80_bus_config_t *c,esp_lcd_i80_bus_handle_t *out){
 static const int pins[]={39,40,41,42,45,46,47,48};
 assert(c->dc_gpio_num==7 && c->wr_gpio_num==8 && c->bus_width==8);
 assert(c->max_transfer_bytes==640 && c->dma_burst_size==4);
 for(unsigned i=0;i<8;i++)assert(c->data_gpio_nums[i]==pins[i] && ats_modes[pins[i]]==GPIO_MODE_OUTPUT);
 assert(ats_read_count==4 && levels[9] && levels[6]);*out=(void*)1;return 0;
}
static int esp_lcd_new_panel_io_i80(esp_lcd_i80_bus_handle_t bus,const esp_lcd_panel_io_i80_config_t *c,esp_lcd_panel_io_handle_t *out){
 assert(bus && c->cs_gpio_num==6 && c->pclk_hz==20000000 && c->trans_queue_depth==1);
 assert(c->lcd_cmd_bits==8 && c->lcd_param_bits==8 && !c->dc_levels.dc_cmd_level && c->dc_levels.dc_data_level);
 ats_callback=c->on_color_trans_done;ats_callback_context=c->user_ctx;*out=(void*)1;return 0;
}
static int esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io,int command,const void *data,size_t size){
 assert(io && !ats_pending);const uint8_t *p=data;
 if(size==1){ats_registers[command]=*p;ats_register_seen[command]=1;}
 if(command==0x36)madctl=*p;
 if(command==0x2a){assert(size==4 && ((p[0]<<8)|p[1])==0 && ((p[2]<<8)|p[3])==319);}
 if(command==0x2b){
  assert(size==4);
  if(p[0]==255){assert(TEST_LCD_ID==0x9307);ats_window_resets++;}
  else {assert(p[0]==p[2] && p[1]==p[3]);row_number=((p[0]<<8)|p[1])-35;assert(row_number<170);}
 }return 0;
}
static int esp_lcd_panel_io_tx_color(esp_lcd_panel_io_handle_t io,int command,const void *data,size_t size){
 assert(io && !ats_pending && command==0x2c && size==640 && !((uintptr_t)data&3));
 ats_pending=data;ats_pending_size=(unsigned)size;ats_queued_hash=ats_hash(data,(unsigned)size);return 0;
}
static int ats_dma_wait(SemaphoreHandle_t s,unsigned timeout){
 assert(s==(void*)3 && ats_pending && timeout==portMAX_DELAY);
 assert(ats_queued_hash==ats_hash(ats_pending,ats_pending_size)); /* Row lives until ISR completion. */
 for(unsigned x=0;x<320;x++)pixels[row_number*320+x]=(ats_pending[x*2]<<8)|ats_pending[x*2+1];
 rows++;ats_pending=NULL;ats_pending_size=0;
 assert(ats_callback((void*)1,NULL,ats_callback_context));assert(ats_ready);ats_ready=0;ats_completions++;return 1;
}
'''


class AtsMini(unittest.TestCase):
    def test_profile_uses_eight_mb_without_psram(self):
        profiles = json.loads((ROOT/'firmware-targets.json').read_text())['profiles']
        profile = next(p for p in profiles if p['id'] == 'ats-mini')
        self.assertEqual(profile['target'], 'esp32s3')
        self.assertTrue(profile['allow_larger_flash'])
        defaults = (ROOT/profile['sdkconfig']).read_text()
        self.assertIn('CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y', defaults)
        self.assertIn('CONFIG_ESP_SDR_ATS_MINI=y', defaults)
        self.assertNotIn('CONFIG_SPIRAM=y', defaults)
        self.assertIn('# CONFIG_ESP_SDR_UART_ENABLED is not set', defaults)
        self.assertIn('ats-mini', (ROOT/'docs/ats-mini.md').read_text(encoding='utf-8'))

    @unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
    def test_parallel_lcd_ids_dma_power_encoder_and_pc_handoff(self):
        stub = STUB.replace('static int64_t now;', 'static int64_t now;\n'+ATS_STUB)
        stub = stub.replace('levels[p]=v;', 'levels[p]=v;if(p==9 && !v)ats_read_count++;')
        stub = stub.replace('assert(p<64);return 0;', 'assert(p<64);ats_modes[p]=m;return 0;')
        old = 'return p==37 ? !busy_stuck : !(pressed_inputs & (UINT64_C(1)<<p));'
        new = '''if(ats_read_count && (p>=39 && p<=42 || p>=45 && p<=48)){
 unsigned shift=p<=42?p-39:p-41;unsigned byte=ats_read_count==1?0:(TEST_LCD_ID>>((4-ats_read_count)*8))&255;
 return (byte>>shift)&1;}return !(pressed_inputs & (UINT64_C(1)<<p));'''
        self.assertIn(old, stub)
        stub = stub.replace(old, new)
        stub = stub.replace('assert(c->sda_io_num==TEST_SDA', 'ats_i2c_started++;assert(c->sda_io_num==TEST_SDA')
        stub = stub.replace('if(mutex_taken)return 0;', 'if(s==(void*)3)return ats_dma_wait(s,timeout);if(mutex_taken)return 0;')
        sources = '\n'.join((ROOT/'main/boards'/f).read_text() for f in ['standalone_device_ats_mini_lcd.c', 'standalone_device_lilygo_ats_mini.c', 'standalone_display.c'])
        for header in ['driver/gpio.h', 'driver/spi_master.h', 'driver/i2c_master.h',
                       'esp_check.h', 'esp_heap_caps.h', 'esp_timer.h', 'esp_lcd_io_i80.h',
                       'esp_lcd_panel_io.h', 'esp_rom_sys.h', 'freertos/FreeRTOS.h',
                       'freertos/task.h', 'freertos/semphr.h']:
            sources = sources.replace(f'#include "{header}"', '')
        check = CHECK.replace('assert(standalone_display_fft_points()==1024);', '''
 assert(standalone_display_fft_points()==1024);
 assert(!levels[15] && !levels[16] && !levels[10] && levels[3] && levels[38]);
 assert(!ats_i2c_started && !ats_modes[17] && !ats_modes[18]);
 assert(madctl==(TEST_LCD_ID==0x9307?0xe8:0xa0));
 assert(ats_registers[0x3a]==(TEST_LCD_ID==0x9307?5:0x55));
 assert(!!ats_register_seen[0x26]==(TEST_LCD_ID==0x858552));
 assert(!!ats_window_resets==(TEST_LCD_ID==0x9307));
 assert(ats_completions==170 && !ats_pending);
''')
        for panel_id in [0x8181b3, 0x858552, 0x009307, 0, 0xffffff]:
            with self.subTest(panel_id=hex(panel_id)):
                compile_run(stub+sources+check, [
                    'CONFIG_ESP_SDR_ATS_MINI=1', 'CONFIG_SPIRAM=0', f'TEST_LCD_ID={panel_id}',
                    'TEST_WIDTH=320', 'TEST_HEIGHT=170', 'TEST_DC=7', 'TEST_X=0', 'TEST_Y=35',
                    'TEST_MOSI=0', 'TEST_SCLK=0', 'TEST_CS=6', 'TEST_SDA=18', 'TEST_SCL=17', 'TEST_EPAPER=0'])
