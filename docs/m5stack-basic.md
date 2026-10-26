# M5Stack Basic

M5Stack Basic (the original ESP32 model) runs a standalone spectrum display
and the existing USB-to-UART ESP-SDR protocol. No external RF module or PSRAM
is required. For the related Core2, Cardputer ADV and M5StickC Plus2 builds,
see [M5Stack display builds](m5stack-core2-cardputer.md).

## Build and flash

Activate the ESP-IDF commit pinned by the `m5stack-basic` profile in
[firmware-targets.json](../firmware-targets.json), then run from the repository root:

```sh
idf.py -B build-m5stack-basic -DIDF_TARGET=esp32 \
  -DSDKCONFIG=build-m5stack-basic/sdkconfig \
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-basic' build
idf.py -B build-m5stack-basic -p PORT flash
```

Replace `PORT` with the board's port, such as `COM5` or `/dev/ttyUSB0`.
In PowerShell, put the first command on one line (or use backticks instead of
backslashes for continuation). The build uses 4 MB of flash, DIO at 40 MHz,
and also fits newer 16 MB Basics.

The default LCD driver is ILI9342C. For an early Basic fitted with ILI9341,
enable **ESP-SDR board > Early Basic with ILI9341 LCD** in
`idf.py -B build-m5stack-basic menuconfig`, then rebuild and flash.
LCD inversion is selected using the reset pin's external pull-up, following
the [M5Stack display driver](https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp).

For release artifacts, use:

```sh
python tools/build_firmware.py --profile m5stack-basic --version YOUR_VERSION
```

## Standalone operation

Power on to start at 2412 MHz, with a 16 MHz span, a 1024-point FFT, 16-spectrum
averaging and software gain control targeting a raw IQ peak of -12 dBFS. It
starts from PHY gain index 30. Each display update averages FFT results captured
during a nominal 100 ms interval; LCD drawing adds a little to refresh time.

The normal button hints are `TUNE-`, `MENU`, `TUNE+`. Left/right tune on
release. Press the center button to open the menu or advance one item; hold it
for 0.6 seconds to move back one item (once per hold). Left/right decrease or
increase the selected value. Five seconds without input returns to tuning.
The selected item and its value appear in a panel at the center of the screen,
with the value in larger text. Menu navigation wraps between the first and last
items; numeric settings stop at their limits.

| Menu item, in order | Values / action |
| --- | --- |
| FREQ STEP | 1, 5, 20, 100 MHz |
| FREQ SPAN | 2, 4, 8, 16, 40, 80 MHz |
| Y POS | Trace offset −40 to +40 dB, in 5 dB steps; positive raises the trace |
| WF BRIGHT | Waterfall offset −30 to +30 dB, in 5 dB steps |
| WF CONTRAST | 25–200%, in 25% steps; default 100% |
| WF SPEED | 1/10×, 1/5×, 1/2×, 1×; default one waterfall row per valid display update |
| FFT NUM | 256, 512, 1024, 2048, 4096, 8192; default 1024 |
| FFT AVERAGE | 1, 2, 4, 8, 16; default 16 |
| SPEC MODE | NORMAL (default) or DPX-style density display |
| AGC TYPE | MANUAL, SOFTWARE (default), HARDWARE; cycles in either direction |
| RF GAIN | Calibrated PHY gain index; changing it selects MANUAL mode |
| FILTER | AUTO (follow span, default), WIDE (DAC 0), DAC 1–127; larger codes narrow the filter |
| ABOUT | Left/right switch between the firmware name and board name |
| RESET | Right restores startup frequency and settings; left exits |

RESET returns to 2412 MHz, STEP 1 MHz, SPAN 16 MHz, FFT 1024, AVE 16,
zero Y POS/brightness offsets, 100% contrast, 1× waterfall speed, NORMAL spectrum, SOFTWARE AGC
starting at gain index 30 (limited to the calibrated range), and AUTO filter.
Settings are kept in RAM and return to defaults on reboot.

M5Stack profiles start with software AGC in standalone operation; serial gain
commands can explicitly override it. Gain changes stay within the calibrated
PHY range. The CPU
percentage estimates time spent capturing, calculating FFTs and drawing the
LCD during each integration/update interval; it is a local display workload
estimate, not a system-wide idle-task measurement.

The center frequency and plot span appear above the plots; frequencies below
them are in MHz. The CPU estimate is at the upper right. The Basic and Core2
omit the settings line below the title and extend the spectrum graph upward
into that space. The selected number
of snapshots is scheduled at evenly spaced positions within each 100 ms
interval and averaged in the power domain. If a capture runs late, missed
positions are skipped rather than bunched together, so the actual number may be
lower than selected. Spans of 2, 4 and 8 MHz keep the receiver at 16 MS/s and
zoom the display into the center of its 16 MHz capture; 16, 40 and 80 MHz spans
use their matching sample rates. Averaging smooths the displayed level without
changing the FFT frequency resolution. `LIVE` means at least one local capture and FFT
succeeded in the current interval; it does not mean a transmitter was detected. `WAIT`
means the latest local capture failed, and `PC` means a serial client owns the
receiver. At FFT sizes above 256, the strongest
bin in each displayed frequency group is shown so narrow peaks remain visible.
Both plots use nearly the full LCD width. In NORMAL mode the upper plot is a
thin blue trace with a translucent blue fill beneath it, over a dark grid.
The colors follow SRdeck: a `#2896FF` stroke and `#0050D2` fill at 120/255 opacity.
The RGB565 renderer preblends the fill over black and grid pixels so the grid
stays visible without an extra buffer. Negative frequency offsets are on the left.
Its vertical grid divides the selected span into eight intervals (2 MHz each
at a 16 MHz span). Frequency marks sit between the plots. The lower waterfall
puts each averaged spectrum at the top and scrolls older spectra down, showing
about twelve seconds of history at the nominal 100 ms interval, plus LCD drawing
time. The display converts the upstream 0–255 power code to signed integer
dBFS, rounded to 1 dB (`code / 2 - 84.3 dBFS`). It estimates the minimum FFT-bin
level in the displayed frequency band and smooths it with a 1/32 exponential
average per frame. The trace spans 80 dB, from that floor to 80 dB above it.
The waterfall uses each row's smoothed floor as its reference: the floor is
black, +10 dB blue, +20 dB cyan, +30 dB green, +40 dB yellow, +50 dB red, and
+60 dB or more white, with gradients between those points. The floor reference resets
when tuning, span or FFT size changes. dBFS is referenced to a full-scale,
bin-centered tone with the current IQ and Hann-window normalization; antenna
input power is not calibrated in dBm, and software-controlled PHY gain changes
the corresponding RF input level. Captures are
snapshots with gaps, as on the original ESP32 backend. DC correction estimates
the I/Q mean over each captured waveform before windowing and FFT. A stationary
signal exactly at the LO cannot be distinguished from a DC offset. The three
button hints sit directly above their corresponding physical buttons along the
bottom edge.

The software accepts center frequencies from 100 to 6000 MHz. This does not
mean the radio works over that entire range: reception is primarily around
2.4 GHz, with experimental tuning down to roughly 1.85 GHz. See
[RX controls](rx-controls.md). The speaker is muted; this firmware displays
spectra and captures I/Q, without audio demodulation.

`SPEC MODE` switches the upper graph to a DPX-style density display. Each
successful FFT contributes before averaging; the waterfall still uses averaged
power. The display stores 64 fixed level buckets (about 2 dB each), so moving
Y POS or tracking the floor does not change the saved levels. Blue marks sparse
hits and warmer colors mark more repeated hits; brightness fades with elapsed
time. These weighted, saturating values are qualitative, not probabilities.

DPX needs up to an additional 19,968 bytes on Basic/Core2, 14,848 bytes on Cardputer/ADV
and 8,128 bytes on Plus2. Pixels sharing a zoomed FFT bin share one history column;
for example 1024 points at 2 MHz use 8,192 bytes on Basic/Core2 and Cardputer/ADV.
It allocates internal RAM when enabled and releases it
when disabled. Allocation failure keeps the normal trace and shows `RAM` in the
status area (`NO RAM` in the menu). Tuning, span, FFT, averaging, capture gain,
filter or DC changes clear density history. A PC lease freezes it; local resume
clears it. Gain changes during AGC can therefore shorten visible persistence;
manual gain is preferable when comparing signals. Snapshot gaps still exist:
signals outside captured windows cannot appear in either display.

## PC connection

Connect the Basic's USB port and select its serial port in the
[browser SDR viewer](https://espargos.net/espsdr/app/) at **2,000,000 baud**.
Install the USB-UART driver for the bridge on your board if the port is missing.

PC commands have priority. While the PC owns the receiver, local buttons and
local captures pause. The last LCD spectrum and waterfall history stay visible
during a streaming command; idle periods show `PC CONTROL`. Standalone control resumes after
`RELEASE` or five seconds without commands. Settings selected by the PC,
including frequency and gain, remain in effect.

## Board pins

The [official Basic pin map](https://docs.m5stack.com/en/core/Basic) assigns
LCD/SD SPI to GPIO18/19/23, LCD control to GPIO14/27/32/33, SD select to GPIO4,
the speaker to GPIO25, buttons to GPIO39/38/37 and the power-management I2C
bus to GPIO21/22. The Basic profile excludes all these pins from `GPIO?`
and rejects attempts to drive them through the serial GPIO command.
UART0 uses GPIO1/3 through the onboard USB bridge.
