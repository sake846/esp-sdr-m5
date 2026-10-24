# M5Stack Basic

M5Stack Basic (the original ESP32 model) runs a standalone spectrum display
and the existing USB-to-UART ESP-SDR protocol. No external RF module or PSRAM
is required. Core2 and CoreS3 use different board wiring and are not this profile.

## Build and flash

Activate the ESP-IDF commit pinned by the `m5stack-basic` profile in
[firmware-targets.json](../firmware-targets.json), then run from the repository root:

```sh
idf.py -B build-m5stack-basic -DIDF_TARGET=esp32 \
  -DSDKCONFIG=build-m5stack-basic/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-basic build
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

Power on to start at 2412 MHz, with a 16 MHz span, a 256-point FFT and hardware
automatic gain control. The LCD refreshes approximately ten times per second.

| Button | Action |
| --- | --- |
| A | Decrease the center frequency by the selected step |
| B, short press | Cycle the step: 1, 5, 20 MHz |
| B, hold for 0.6 seconds | Cycle the span: 16, 40, 80 MHz |
| C | Increase the center frequency by the selected step |
| C, hold for 0.6 seconds | Cycle FFT size: 256, 512, 1024, 2048, 4096, 8192, then 256 |

Short A/C presses tune only after the button is released. Holding A still
decreases the frequency when released; holding C cycles the FFT size. The CPU
percentage estimates the capture, FFT and LCD update time against the nominal
100 ms refresh period; it is a local display workload estimate, not a system
wide idle-task measurement.

The center frequency, span, step, FFT size and CPU estimate appear above the
plots; frequencies below them are in MHz. At FFT sizes above 256, the strongest
bin in each displayed frequency group is shown so narrow peaks remain visible.
Both plots use nearly the full LCD width. The upper plot is a
thin blue trace over a dark grid, with negative frequency offsets on the left.
Its vertical grid divides the selected span into eight intervals (2 MHz each
at a 16 MHz span). Frequency marks sit between the plots. The lower waterfall
puts each new capture at the top and scrolls older captures down, showing about
twelve seconds of history. Its colors show relative power: blue is quieter,
cyan/yellow/white is stronger. Neither plot is calibrated in dBm. Captures are
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
