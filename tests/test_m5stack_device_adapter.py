"""Exercise M5Stack device geometry, input mapping and the common renderer."""
import json
import unittest

from host_test import COMPILER, ROOT, compile_run

BOARDS = [
    ('m5stack-cores3', 'CORES3', 'ESP32S3', 16, 320, 240, 1),
    ('m5stack-din-meter', 'DIN_METER', 'ESP32S3', 8, 240, 135, 1),
    ('m5sticks3', 'STICKS3', 'ESP32S3', 8, 135, 240, 1),
    ('m5stickc-plus', 'STICKC_PLUS', 'ESP32', 4, 135, 240, 1),
    ('m5stickc', 'STICKC', 'ESP32', 4, 80, 160, 1),
    ('m5stack-paper-color', 'PAPER_COLOR', 'ESP32S3', 16, 300, 200, 2),
    ('m5stack-papers3', 'PAPERS3', 'ESP32S3', 16, 480, 270, 2),
]

STUB = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
static int battery_percent = -1;
int board_battery_percent(void) { return battery_percent; }
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define ESP_ERROR_CHECK(x) assert((x)==0)
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
static unsigned buttons, rows, flushes;
static int encoder_steps;
static int64_t now;
static unsigned completed_frames;
static int64_t esp_timer_get_time(void) { return now*1000; }
static void *heap_caps_calloc(size_t n, size_t s, unsigned caps) {
 assert(caps==(CONFIG_SPIRAM ? 6 : 3)); return calloc(n,s);
}
'''

CHECK = r'''
#include "standalone_device_m5stack_input.h"
static uint16_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
void m5stack_device_adapter_init(void) {}
unsigned m5stack_device_adapter_refresh_count(void) { return completed_frames; }
unsigned m5stack_device_adapter_buttons(void) { return buttons; }
int m5stack_device_adapter_encoder(void) { int steps=encoder_steps; encoder_steps=0; return steps; }
void m5stack_device_adapter_row(unsigned y, const uint16_t *p) {
 assert(y<SCREEN_HEIGHT); rows++; memcpy(pixels+y*SCREEN_WIDTH,p,SCREEN_WIDTH*sizeof(*p));
}
void m5stack_device_adapter_flush(void) { flushes++; }
static unsigned input(unsigned mhz, unsigned pressed) {
 buttons=pressed; now+=40; mhz=standalone_display_poll(mhz,false);
 now+=31; return standalone_display_poll(mhz,false);
}
int main(void) {
 assert(SCREEN_WIDTH==TEST_WIDTH && SCREEN_HEIGHT==TEST_HEIGHT && M5_DISPLAY_SCALE==TEST_SCALE);
 assert(SPECTRUM_TOP<SPECTRUM_BOTTOM && WATERFALL_HEIGHT>0);
 standalone_display_init(); assert(rows==SCREEN_HEIGHT && flushes==1);
 uint8_t bins[1024]={0}; bins[510]=bins[511]=255;
 standalone_display_draw(2412,bins,1024,false,true,0); assert(waterfall_count==1);
 for(unsigned menu=1;menu<=M5_MENU_RESET;menu++) {
   controls.menu=(m5stack_menu_t)menu;
   standalone_display_draw(2412,bins,1024,false,false,0);
   assert(pixels[(SCREEN_HEIGHT/2-34)*SCREEN_WIDTH+(SCREEN_WIDTH>200 ? 32 : 4)]==
          __builtin_bswap16(COLOR_SRDECK_YELLOW_GREEN));
 }
 controls.menu=M5_MENU_NONE;
 unsigned mhz=input(2412,1); mhz=input(mhz,0); assert(mhz==2402);
 mhz=input(mhz,4); mhz=input(mhz,0); assert(mhz==2412);
 mhz=input(mhz,2); mhz=input(mhz,0); assert(controls.menu==M5_MENU_STEP);
 controls.menu=M5_MENU_NONE;
 encoder_steps=2; now+=1; assert(standalone_display_poll(2412,false)==2432);
 encoder_steps=-8; buttons=5; assert(standalone_display_poll(2412,true)==2412 && !encoder_steps);
 buttons=0; now+=40; standalone_display_poll(2412,false);
 now+=31; assert(standalone_display_poll(2412,false)==2412);
 controls.menu=M5_MENU_GAIN; controls.last_input=(uint32_t)now;
 now+=6000; standalone_display_poll(2412,false);
#if CONFIG_ESP_SDR_M5STACK_PAPER_COLOR
 assert(controls.menu==M5_MENU_GAIN); now+=120000; standalone_display_poll(2412,false);
#endif
 assert(controls.menu==M5_MENU_NONE);
 bool chord=false;
 assert(m5stack_device_input_two_buttons(1,&chord)==1);
 assert(m5stack_device_input_two_buttons(3,&chord)==2 && chord);
 assert(m5stack_device_input_two_buttons(2,&chord)==2);
 assert(m5stack_device_input_two_buttons(0,&chord)==0 && !chord);
 assert(m5stack_device_input_two_buttons(2,&chord)==4);
 assert(m5stack_device_input_touch_button(0,75,300,100)==1);
 assert(m5stack_device_input_touch_button(100,75,300,100)==2);
 assert(m5stack_device_input_touch_button(299,99,300,100)==4);
 assert(!m5stack_device_input_touch_button(300,99,300,100));
 assert(!m5stack_device_input_touch_button(-1,75,300,100));
 assert(!m5stack_device_input_touch_button(0,74,300,100));
 controls.menu=M5_MENU_NONE; battery_percent=100;
 const char *telemetry[]={"BAT 100%", "CPU 49%"};
 for(unsigned frame=0;frame<2;frame++) {
   now=(frame+1)*5000; completed_frames=frame+1;
   standalone_display_draw(2412,bins,1024,false,true,49);
#if M5_DISPLAY_LAYOUT_STICK
   const GFXfont *font=&DejaVu9;
   unsigned left=SCREEN_WIDTH-m5stack_text_width(telemetry[frame],font)-4;
   for(unsigned y=25;y<36;y++) {
     memset(row,0,sizeof(row));
     m5stack_text_row(y,left,34,telemetry[frame],font,0xffff,0);
     for(unsigned x=0;x<SCREEN_WIDTH;x++)assert(pixels[y*SCREEN_WIDTH+x]==row[x]);
   }
#else
   const GFXfont *font=SCREEN_WIDTH<200 ? &DejaVu9 : &DejaVu12;
   unsigned left=SCREEN_WIDTH-m5stack_text_width(telemetry[frame],font)-4;
   const char *title="ESP SDR M5";
   assert(12+m5stack_text_width(title,&DejaVu9)+3 <=
          (SCREEN_WIDTH-m5stack_text_width("LIVE",font))/2);
   assert(SCREEN_WIDTH/2+m5stack_text_width("LIVE",font)/2+3 <= left);
   for(unsigned y=1;y<12;y++) {
     memset(row,0,sizeof(row));
     m5stack_text_row(y-1,left,9,telemetry[frame],font,0xffff,0);
     for(unsigned x=left;x<SCREEN_WIDTH;x++)assert(pixels[y*SCREEN_WIDTH+x]==row[x]);
   }
#endif
 }
#if CONFIG_ESP_SDR_M5STACK_PAPER_COLOR
 now+=30000; standalone_display_draw(2412,bins,1024,false,true,49);
 char expected[12]; board_status_text(expected,sizeof(expected),49,(int64_t)completed_frames*5000000);
 assert(!strcmp(expected,"CPU 49%")); /* Wall-clock updates do not skip the BAT refresh. */
#endif
 free(waterfall); return 0;
}
'''


class M5StackDeviceAdapter(unittest.TestCase):
    @unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
    def test_epaper_frame_lifetime_scaling_and_nonblocking_flush(self):
        source = (ROOT/'main/boards/standalone_device_m5stack.cpp').read_text()
        functions = []
        for signature in ['static void paper_worker(', 'void m5stack_device_adapter_flush(']:
            start = source.index(signature)
            functions.append(source[start:source.index('\n}', start)+2])
        # The worker only uses the C-compatible display methods mocked below.
        worker = '\n'.join(functions).replace('paper_worker(void *)', 'paper_worker(void *unused)')
        stub = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "standalone_device_m5stack.h"
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
static const size_t frame_pixels=SCREEN_WIDTH*SCREEN_HEIGHT;
static uint16_t frame[SCREEN_WIDTH*SCREEN_HEIGHT], pending[SCREEN_WIDTH*SCREEN_HEIGHT];
static uint16_t tx[SCREEN_WIDTH*SCREEN_HEIGHT];
static uint16_t *paper_frame=frame, *paper_pending=pending, *paper_tx=tx;
static unsigned paper_lock, paper_task, locked, notices, calls, rows, refreshes, waited, delayed;
static unsigned paper_refresh_count;
static jmp_buf worker_exit;
static unsigned ulTaskNotifyTake(unsigned clear, unsigned timeout) {
 assert(clear==pdTRUE && timeout==portMAX_DELAY);
 if(calls++)longjmp(worker_exit,1); return 1;
}
static unsigned xSemaphoreTake(unsigned lock, unsigned timeout) {
 if(locked)return 0; locked=1; return pdTRUE;
}
static void xSemaphoreGive(unsigned lock) { assert(locked);locked=0; }
static void xTaskNotifyGive(unsigned task) { notices++; }
static void vTaskDelay(unsigned ms) { delayed=ms; }
static void push_image(int x, int y, int w, int h, const uint16_t *pixels) {
 assert(!locked && !x && y==(int)rows && w==SCREEN_WIDTH*2 && h==1);
 uint16_t expected=TEST_INVERT ? (uint16_t)~0x1234 : 0x1234;
 for(int col=0;col<w;col++)assert(pixels[col]==expected);
 if(!rows) {
   paper_frame[0]=0x5678; m5stack_device_adapter_flush();
   assert(paper_pending[0]==0x5678 && paper_tx[0]==0x1234);
 }
 rows++;
}
static void display(void) { assert(rows==SCREEN_HEIGHT*2); refreshes++; }
static void wait_display(void) { waited++; }
static struct {
 struct {
   void (*pushImage)(int,int,int,int,const uint16_t*);
   void (*display)(void);
   void (*waitDisplay)(void);
 } Display;
} M5={{push_image,display,wait_display}};
'''
        check = r'''
int main(void) {
 for(size_t i=0;i<frame_pixels;i++)paper_frame[i]=0x1234;
 m5stack_device_adapter_flush(); assert(notices==1);
 locked=1; paper_frame[0]=0x9999; m5stack_device_adapter_flush();
 assert(notices==1 && paper_pending[0]==0x1234); locked=0;
 if(!setjmp(worker_exit))paper_worker(NULL);
 assert(!locked && notices==2 && refreshes==1 && waited==1 && delayed==TEST_DELAY);
#if CONFIG_ESP_SDR_M5STACK_PAPER_COLOR
 assert(paper_refresh_count==1);
#endif
 return 0;
}
'''
        for board, inverted, delay in [('PAPER_COLOR', 0, 30000), ('PAPERS3', 1, 1000)]:
            with self.subTest(board=board):
                compile_run(stub+worker+check, [f'CONFIG_ESP_SDR_M5STACK_{board}=1',
                            f'TEST_INVERT={inverted}', f'TEST_DELAY={delay}'])

    def test_catalog_and_defaults(self):
        catalog = json.loads((ROOT/'firmware-targets.json').read_text())
        profiles = {p['id']: p for p in catalog['profiles']}
        self.assertNotIn('m5stack-tab5', profiles)
        self.assertIn('ESP32-C6', catalog['unavailable']['m5stack-tab5'])
        for name, board, chip, flash, *_ in BOARDS:
            with self.subTest(profile=name):
                p = profiles[name]
                self.assertTrue(p['m5stack'])
                self.assertEqual(p['target'], chip.lower())
                defaults = (ROOT/p['sdkconfig']).read_text()
                self.assertIn(f'CONFIG_ESP_SDR_M5STACK_{board}=y', defaults)
                self.assertIn(f'CONFIG_ESPTOOLPY_FLASHSIZE_{flash}MB=y', defaults)
                self.assertIn(name, (ROOT/'docs/m5stack-additional.md').read_text(encoding='utf-8'))

    @unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
    def test_renderer_geometry_menu_and_pc_handoff(self):
        source = (ROOT/'main/boards/standalone_display.c').read_text()
        for header in ['driver/gpio.h', 'driver/spi_master.h', 'esp_check.h', 'esp_heap_caps.h',
                       'esp_timer.h', 'freertos/FreeRTOS.h', 'freertos/task.h']:
            source = source.replace(f'#include "{header}"', '')
        for _, board, chip, _, width, height, scale in BOARDS:
            with self.subTest(board=board):
                compile_run(STUB+source+CHECK, ['CONFIG_ESP_SDR_M5STACK=1',
                            f'CONFIG_ESP_SDR_M5STACK_{board}=1', f'CONFIG_IDF_TARGET_{chip}=1',
                            f'CONFIG_SPIRAM={int(board not in ("DIN_METER", "STICKC", "STICKC_PLUS"))}',
                            f'TEST_WIDTH={width}', f'TEST_HEIGHT={height}', f'TEST_SCALE={scale}'])
