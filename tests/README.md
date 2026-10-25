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
python -m unittest discover -s tests -p "test_gpio.py" -v
python -m unittest discover -s tests -p "test_firmware_export.py" -v
```

| Test | Regression coverage |
| --- | --- |
| `test_m5stack_basic.py` | Debounce, all short/long button actions, bounds, timer wrap, PC handoff, Basic SPI rendering and waterfall history |
| `test_cardputer_controls.py` | Original keyboard matrix, ADV event press/release, per-key debounce, Shift reversal, four-digit frequency validation and PC handoff |
| `test_m5stack_displays.py` | Core2 AXP192/AXP2101 power setup, touch-zone boundaries and I2C failure; Core2/Cardputer/ADV/Plus2 LCD geometry, cropped spans, peak reduction, waterfall reset and one column-window update per frame |
| `test_m5stack_spectrum.py` | Actual IQ unpacking, Hann window, FFT and quantization for all five boards; positive/negative tones at every supported FFT size, in-place capture, DC resets, workspace reuse, 4 KiB default table budget, 2048-point protocol workspace and allocation recovery |
| `test_m5stack_idle.py` | ESP32/S3 scheduling, 1/2/4/8/16-sample linear-power averaging against a numeric reference, failed captures, missed slots, setting changes and PC handoff |
| `test_m5stack_serial.py` | CR/LF/CRLF, partial/overlong/stale lines, peer fairness, idle notification, lease expiry and explicit release |
| `test_m5stack_ring.py` | S3 queue/twiddle allocation and FFT-init failures, retries, idempotent init, queue capacity and counter wrap |
| `test_gpio.py` | Peripheral pin exclusions on all five boards with UART enabled/disabled; excluded pins cannot cause GPIO writes |
| `test_firmware_export.py` | All five board identities, target chips and flash sizes survive artifact export |

These tests execute production C or bounded excerpts of it, with mocked SDK I/O.
The numerical snapshot tests use the production FFT; the older snapshot protocol
test separately supplies deterministic FFT output to check frame encoding.
`benchmark_m5stack_display.py` measures host rendering work with the same SDK
mocks; see [the performance measurements](../docs/performance.md).
Native C tests report a skip if their compiler is unavailable. Some upstream
tests require GNU C even on Windows, so use Linux for the complete suite.

RF reception, physical LCD colors/orientation, keyboard wiring and power timing
still require hardware validation. No firmware is flashed by these tests.
