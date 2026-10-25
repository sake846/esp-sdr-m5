"""Regression checks for terminal newlines and the standalone idle hook."""
import unittest

from host_test import COMPILER, ROOT, compile_run

SOURCE = (ROOT / 'main/common/burst_serial.c').read_text()


@unittest.skipUnless(COMPILER, 'Host C compiler unavailable')
class StandaloneSerial(unittest.TestCase):
    def test_cr_lf_crlf_overflow_recovery_and_peer_fairness(self):
        start = SOURCE.index('int burst_serial_poll_line(')
        function = SOURCE[start:SOURCE.index('\n}', start) + 2]
        stub = r'''
#include <assert.h>
#include <string.h>
#include "burst_serial.h"
#define COMMAND_SIZE 128
static burst_serial_port_t active_port;
static unsigned next_port;
static int64_t now;
static struct {char line[COMMAND_SIZE]; size_t used; bool overflow; int64_t last_byte;} input[2];
static const char *incoming[2] = {"", ""};
static int64_t esp_timer_get_time(void) { return now; }
static int read_port(unsigned port, void *p, unsigned n) {
    assert(n == 1); if (!*incoming[port]) return 0;
    *(char *)p = *incoming[port]++; return 1;
}
static bool baud_command(const char *line) { return false; }
'''
        check = r'''
int main(void) {
    char line[128];
    const char *endings[] = {"INFO\r", "INFO\n", "INFO\r\n", "\r\nINFO\n\r"};
    for (unsigned port = 0; port < 2; port++) for (unsigned j = 0; j < 4; j++) {
        incoming[port] = endings[j];
        assert(burst_serial_poll_line(line, sizeof(line)) == 1);
        assert(!strcmp(line, "INFO") && active_port == port);
        assert(burst_serial_poll_line(line, sizeof(line)) == 0);
    }
    incoming[0] = "FREQ 24"; assert(!burst_serial_poll_line(line, sizeof(line)));
    incoming[0] = "12\r"; assert(burst_serial_poll_line(line, sizeof(line)) == 1);
    assert(!strcmp(line, "FREQ 2412"));
    char overflow[131]; memset(overflow, 'x', 129); overflow[129] = '\r'; overflow[130] = 0;
    incoming[0] = overflow; incoming[1] = "CAPS\n"; next_port = 0;
    assert(burst_serial_poll_line(line, sizeof(line)) == 1);
    assert(active_port == 1 && !strcmp(line, "CAPS")); /* Long input cannot starve peer. */
    assert(burst_serial_poll_line(line, sizeof(line)) == -1);
    incoming[0] = "INFO\r";
    assert(burst_serial_poll_line(line, sizeof(line)) == 1 && !strcmp(line, "INFO"));
    incoming[0] = "TOOLONG\r"; assert(burst_serial_poll_line(line, 4) == -1);
    incoming[0] = "stale"; assert(!burst_serial_poll_line(line, sizeof(line)));
    now += 3000001;
    incoming[0] = "INFO\r";
    assert(burst_serial_poll_line(line, sizeof(line)) == 1 && !strcmp(line, "INFO"));
    return 0;
}
'''
        compile_run(stub + function + check)

    def test_idle_hook_tracks_lease_release_expiry_and_busy_peer(self):
        function = SOURCE[SOURCE.index('void burst_serial_serve('):]
        stub = r'''
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include "burst_serial.h"
static jmp_buf done;
static int64_t now;
static unsigned event, handled, idles, replies, port;
static bool leased[16];
static char responses[128];
static int64_t esp_timer_get_time(void) { return now; }
static void vTaskDelay(unsigned ticks) { now += 1000*ticks; }
burst_serial_port_t burst_serial_port(void) { return (burst_serial_port_t)port; }
void burst_serial_idle(bool pc) {
    leased[idles++] = pc;
    if (idles == 5) now += 5000001; /* Expire the active PC lease. */
}
static void serve_reply(const char *text) { strcat(responses, text); replies++; }
int burst_serial_poll_line(char *line, size_t capacity) {
    (void)capacity;
    switch (event++) {
    case 0: return 0;                         /* Local operation. */
    case 1: port = 0; strcpy(line, "INFO"); return 1;
    case 2: return 0;                         /* PC owns USB. */
    case 3: port = 1; strcpy(line, "FREQ 100"); return 1;
    case 4: return 0;                         /* Busy peer cannot take ownership. */
    case 5: port = 0; strcpy(line, "RELEASE"); return 1;
    case 6: return 0;                         /* Explicit return to local. */
    case 7: port = 1; strcpy(line, "INFO"); return 1;
    case 8: return 0;                         /* UART lease, then clock jump. */
    case 9: return 0;                         /* Expired lease. */
    case 10: port = 0; return -1;             /* Malformed input does not acquire a lease. */
    case 11: return 0;
    default: longjmp(done, 1);
    }
    return 0;
}
static void handle(char *line) { assert(!strcmp(line, "INFO")); handled++; }
'''
        check = r'''
int main(void) {
    if (!setjmp(done)) burst_serial_serve(handle);
    assert(handled == 2 && replies == 3 && idles == 7);
    const bool expected[] = {false, true, true, false, true, false, false};
    for (unsigned j = 0; j < idles; j++) assert(leased[j] == expected[j]);
    assert(!strcmp(responses, "ERR busy\nOK\nERR command_length\n"));
    return 0;
}
'''
        compile_run(stub + function + check)
