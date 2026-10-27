# Host regression tests

Run from the repository root. Python's standard library and a native C compiler
are sufficient; ESP-IDF and connected boards are not required.

```sh
python3 -m unittest discover -s tests -v
```

The GitHub Actions firmware workflow runs this suite before building firmware.
On Linux, install GCC (`cc`) to run the complete suite, including the upstream
receiver/calibration tests. C harnesses use UBSan where configured.

The tests for code added after the fork from ESPARGOS/esp-sdr also support MSVC.
From an **x64 Native Tools Command Prompt for Visual Studio**, run:

```bat
python -m unittest discover -s tests -p "test_m5stack*.py" -v
python -m unittest discover -s tests -p "test_cardputer_controls.py" -v
python -m unittest discover -s tests -p "test_lilygo.py" -v
python -m unittest discover -s tests -p "test_ats_mini.py" -v
python -m unittest discover -s tests -p "test_m5unified.py" -v
python -m unittest discover -s tests -p "test_gpio.py" -v
python -m unittest discover -s tests -p "test_firmware_export.py" -v
```

| Test | Regression coverage |
| --- | --- |
| `test_m5stack_basic.py` | Menu order/reversal, setting bounds, five-second timeout, reset, receiver synchronization, debounce, timer wrap, PC handoff, Basic SPI overlay and waterfall controls |
| `test_m5stack_menu.py` | Native menu and status fonts, long values on 135–480 pixel displays, proportional widths, compact reset hints, empty/unsupported text and scanline clipping |
| `test_cardputer_controls.py` | Original keyboard matrix, ADV event press/release, per-key debounce, Shift reversal, four-digit frequency validation and PC handoff |
| `test_ats_mini.py` | ATS-mini parallel pinout, ST7789/high-gamma/GC9307 IDs and fallback, delayed DMA row lifetime, radio/audio power, encoder and PC handoff |
| `test_lilygo.py` | All eight profiles: SPI/I2C wiring, SY6970 charge/watchdog control, power/CS setup, scanlines, encoder/keyboard controls, PC handoff, menu hold, monochrome conversion, full/partial e-paper refresh and BUSY timeout |
| `test_m5unified.py` | Seven new M5Stack profile identities, panel geometry/scaling, production rendering and every menu overlay, touch-zone boundaries, two-button chords, encoder/PC handoff, slow PaperColor menu expiry, coherent e-paper snapshots during refresh, mutex-busy nonblocking flushes and pixel scaling/inversion |
| `test_m5stack_displays.py` | Core2 AXP192/AXP2101 power setup, touch-zone boundaries and I2C failure; Core2/Cardputer/ADV/Plus2 LCD geometry, cropped spans, peak reduction, waterfall reset and one column-window update per frame |
| `test_m5stack_spectrum.py` | Actual IQ unpacking, Hann window, FFT and quantization for M5Stack, LILYGO and ATS-mini; positive/negative tones at every supported FFT size, in-place capture, DC resets, workspace reuse, 4 KiB default table budget, 2048-point protocol workspace and allocation recovery |
| `test_m5stack_idle.py` | ESP32/S3 scheduling, 1/2/4/8/16-sample linear-power averaging against a numeric reference, failed captures, missed slots, setting changes and PC handoff |
| `test_m5stack_density.py` | All five boards: individual FFT ingestion, transient retention, level-to-row coverage, zoom direction, timed decay, context resets, PC freeze/resume, memory release and allocation failure/retry |
| `test_m5stack_serial.py` | CR/LF/CRLF, partial/overlong/stale lines, peer fairness, idle notification, lease expiry and explicit release |
| `test_m5stack_ring.py` | S3 queue/twiddle/FFT-buffer allocation and FFT-init failures, retries, idempotent init, queue capacity and counter wrap |
| `test_gpio.py` | Peripheral pin exclusions on M5Stack, LILYGO and ATS-mini with UART enabled/disabled; excluded pins cannot cause GPIO writes |
| `test_firmware_export.py` | All M5Stack, LILYGO and ATS-mini profile identities, target chips and flash sizes survive artifact export |

These tests execute production C or bounded excerpts of it, with mocked SDK I/O.
The numerical snapshot tests use the production FFT; the older snapshot protocol
test separately supplies deterministic FFT output to check frame encoding.
`benchmark_m5stack_display.py` measures host rendering work with the same SDK
mocks; see [the performance measurements](../docs/performance.md).
`benchmark_m5stack_density.py` measures DPX ingestion and rendering for three
screen geometries, default/maximum FFT sizes and 2/16 MHz spans. It saves timings,
heap payload and pixel/history hashes in `artifacts/dpx-benchmark/current.json`.
Native C tests report a skip if their compiler is unavailable. Some upstream
tests require GNU C even on Windows, so use Linux for the complete suite.

RF reception, physical LCD colors/orientation, keyboard wiring and power timing
still require hardware validation. No firmware is flashed by these tests.
