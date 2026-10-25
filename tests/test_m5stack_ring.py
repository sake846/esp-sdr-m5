"""The S3 ring allocates its own queue/twiddles and recovers from failures."""
import unittest

from host_test import COMPILER, ROOT, compile_run


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class RingWorkspace(unittest.TestCase):
    def test_allocation_and_fft_failures_are_retriable_and_queue_wraps(self):
        source = (ROOT / 'main/common/ring_capture.c').read_text()
        functions = []
        for signature in ['bool ring_capture_init(', 'RING_HOT static bool txq_push(']:
            start = source.index(signature)
            functions.append(source[start:source.index('\n}', start) + 2])
        stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define RING_HOT
#define MEMW() ((void)0)
#define ESP_OK 0
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define RING_SPEC_NFFT_MAX 2048
#define TXQ_SIZE 16384u
static uint8_t *txq;
static uint32_t txq_head, txq_tail;
static int16_t *ring_twiddles;
static bool dsp_ready;
static unsigned allocations, starts, tables, failure = FAIL_STAGE;
static void *heap_caps_malloc(size_t n, unsigned caps) {
    assert(caps==3); allocations++; return failure==1 ? NULL : malloc(n);
}
static void *heap_caps_aligned_alloc(size_t alignment, size_t n, unsigned caps) {
    assert(alignment==16 && n==2048*sizeof(int16_t) && caps==3);
    allocations++; return failure==2 ? NULL : malloc(n);
}
static int dsps_fft2r_init_sc16(int16_t *w, unsigned n) {
    assert(w==ring_twiddles && n==2048); return failure==3 ? -1 : 0;
}
static void log_tables_init(void) { tables++; }
static void c1_start(void) { starts++; }
'''
        check = r'''
int main(void) {
    uint8_t data[TXQ_SIZE];for(unsigned j=0;j<TXQ_SIZE;j++)data[j]=(uint8_t)j;
    assert(!txq_push(data,1) && txq_head==0);
    assert(!ring_capture_init() && !dsp_ready && starts==0);
    failure=0;assert(ring_capture_init() && dsp_ready && starts==1);
    assert(allocations==(FAIL_STAGE==3 ? 2u : 3u));
    unsigned before=allocations;
    assert(ring_capture_init() && allocations==before && starts==1 && tables==2);
    assert(txq_push(data,TXQ_SIZE));
    assert(!memcmp(txq,data,TXQ_SIZE) && !txq_push(data,1));
    /* Both the circular buffer boundary and the free-running counters wrap. */
    txq_head=txq_tail=UINT32_MAX-3;
    assert(txq_push(data,16) && txq_head==12);
    for(unsigned j=0;j<16;j++)assert(txq[(UINT32_MAX-3+j)&(TXQ_SIZE-1)]==data[j]);
    free(txq);free(ring_twiddles);
    return 0;
}
'''
        for failure in [1, 2, 3]:
            with self.subTest(failure=failure):
                compile_run(stub + '\n'.join(functions) + check,
                            ['CONFIG_IDF_TARGET_ESP32S3=1', f'FAIL_STAGE={failure}'])
