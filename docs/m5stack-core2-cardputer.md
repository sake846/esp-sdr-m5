# M5Stack Core2, Cardputer and M5StickC Plus2

Each profile runs the standalone spectrum display and ESP-SDR serial protocol.
Build from the repository root with the matching ESP-IDF commit listed in
`firmware-targets.json`.

## Core2

Core2 uses the ESP32 receiver backend, its 320×240 LCD and three touch zones
along the bottom of the display. The firmware detects the AXP192 or AXP2101
power controller and configures the LCD, backlight and shared LCD/touch reset.
The display touch zones map to the Basic A/B/C controls. The USB connection
uses the onboard USB-UART bridge at 2,000,000 baud.

```powershell
idf.py -B build-m5stack-core2 -DIDF_TARGET=esp32 `
  -DSDKCONFIG=build-m5stack-core2/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-core2 build
idf.py -B build-m5stack-core2 -p COMx flash
```

## Cardputer K132

Cardputer K132 uses the ESP32-S3 receiver backend, the 240×135 ST7789 display
and the built-in keyboard matrix. Keys `,` and `/` tune down and up; holding
them changes SPAN and AVE. Space changes STEP on a short press and FFT size on
a long press. The local FFT menu goes up to 4096 points and uses internal RAM.
Its screen uses a compact spectrum trace and waterfall layout. The USB
connection uses native USB Serial/JTAG.

```powershell
idf.py -B build-m5stack-cardputer -DIDF_TARGET=esp32s3 `
  -DSDKCONFIG=build-m5stack-cardputer/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer build
idf.py -B build-m5stack-cardputer -p COMx flash
```

## Cardputer ADV

Cardputer ADV uses the same 240×135 display and ESP32-S3 backend as K132, but
its keyboard is connected through a TCA8418 I²C controller. The firmware
reads the same comma, space and slash keys as K132; the short and long press
actions are unchanged. Its local FFT menu also goes up to 4096 points. Use its
separate profile and defaults file:

```powershell
idf.py -B build-m5stack-cardputer-adv -DIDF_TARGET=esp32s3 `
  -DSDKCONFIG=build-m5stack-cardputer-adv/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer-adv build
idf.py -B build-m5stack-cardputer-adv -p COMx flash
```

## M5StickC Plus2

M5StickC Plus2 uses the ESP32 backend, its portrait 135×240 ST7789 display and
the built-in A/B/C buttons. A controls span and tuning down, B changes STEP
or FFT, and C controls averaging and tuning up. The firmware asserts GPIO4
early to keep the power rail on. USB serial uses its onboard USB-UART bridge
at 2,000,000 baud.

```powershell
idf.py -B build-m5stickc-plus2 -DIDF_TARGET=esp32 `
  -DSDKCONFIG=build-m5stickc-plus2/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc-plus2 build
idf.py -B build-m5stickc-plus2 -p COMx flash
```

The builds retain the frequency, STEP, SPAN, FFT and AVE behavior described
in [M5Stack Basic](m5stack-basic.md). Release artifacts can be built with
`python tools/build_firmware.py --profile m5stack-core2 --version VERSION` or
`python tools/build_firmware.py --profile m5stack-cardputer --version VERSION`,
`--profile m5stack-cardputer-adv` or `--profile m5stickc-plus2`.
