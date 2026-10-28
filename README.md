# ESP-SDR for M5Stack, LILYGO and ATS-mini

English | [日本語](README.ja.md)

Turn an M5Stack, LILYGO or ATS-mini device into a standalone spectrum and waterfall display using its built-in ESP32 radio. No external SDR hardware or computer is needed during operation.

This fork adapts [ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr) for the screens and controls on these devices. A computer is needed to build and flash the firmware; after that, the device runs on its own.

## Features

- Live spectrum trace and scrolling waterfall on the device display.
- On-device tuning, display span, FFT size, averaging and gain controls.
- Profiles for M5Stack, LILYGO and ATS-mini devices.
- Optional PC connection through the ESP-SDR USB/UART protocol for I/Q capture and spectrum viewing.
- FFT sizes from 256 to 4096 points on ESP32 and ESP32-S3 profiles.

## Get started

1. Find your device and hardware test status in the table below.
2. Prepare Git, Python and the pinned ESP-IDF environment using the [build setup guide](docs/build-setup.md).
3. Follow the matching device guide for its build command, controls and hardware-specific notes.

The required ESP-IDF revision for each profile is listed in [firmware-targets.json](firmware-targets.json). The device guides contain the commands and operational details for that profile.

Standalone profiles start at 2440 MHz with a 10 MHz tuning step, 80 MHz span,
1024-point FFT and 16-spectrum averaging. M5Stack profiles also start with
software AGC. See the [M5Stack guide](docs/m5stack.md#standalone-operation)
for the shared menu and display settings.

## Supported devices

M5Stack BASIC, Core2, original Cardputer K132, Cardputer ADV and M5StickC Plus2 have been verified on real hardware in this project. Other profiles are not yet hardware-verified.

| Device | Chip | Display | Profile | Hardware status |
| --- | --- | --- | --- | --- |
| [M5Stack BASIC](docs/m5stack.md#m5stack-basic) | ESP32 | 320 × 240 | m5stack-basic | **Verified** |
| [M5Stack Core2](docs/m5stack.md#core2) | ESP32 | 320 × 240 | m5stack-core2 | **Verified** |
| [M5Stack CoreS3](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 320 × 240 | m5stack-cores3 | Unverified |
| [M5Stack DinMeter](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 240 × 135, landscape | m5stack-din-meter | Unverified |
| [Cardputer, original K132](docs/m5stack.md#cardputer-k132) | ESP32-S3 | 240 × 135 | m5stack-cardputer | **Verified** |
| [Cardputer ADV](docs/m5stack.md#cardputer-adv) | ESP32-S3 | 240 × 135 | m5stack-cardputer-adv | **Verified** |
| [M5StickC](docs/m5stack.md#additional-m5stack-devices) | ESP32 | 160 × 80, landscape | m5stickc | Unverified |
| [M5StickC Plus](docs/m5stack.md#additional-m5stack-devices) | ESP32 | 240 × 135, landscape | m5stickc-plus | Unverified |
| [M5StickC Plus2](docs/m5stack.md#m5stickc-plus2) | ESP32 | 135 × 240, portrait | m5stickc-plus2 | **Verified** |
| [M5StickS3](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 240 × 135, landscape | m5sticks3 | Unverified |
| [M5Stack PaperColor](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 600 × 400, color e-paper | m5stack-paper-color | Unverified |
| [M5Stack PaperS3](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 960 × 540, grayscale e-paper | m5stack-papers3 | Unverified |
| [LILYGO T-Display S3 Pro](docs/lilygo.md) | ESP32-S3 | 480 × 222 | lilygo-t-display-s3-pro | Unverified |
| [LILYGO T-LoRa Pager](docs/lilygo.md) | ESP32-S3 | 480 × 222 | lilygo-t-pager | Unverified |
| [LILYGO T-Embed SI4732](docs/lilygo.md) | ESP32-S3 | 320 × 170 | lilygo-t-embed-si4732 | Unverified |
| [LILYGO T-Embed CC1101](docs/lilygo.md) | ESP32-S3 | 320 × 170 | lilygo-t-embed-cc1101 | Unverified |
| [LILYGO T-Embed CC1101 Plus](docs/lilygo.md) | ESP32-S3 | 320 × 170 | lilygo-t-embed-cc1101-plus | Unverified |
| [LILYGO T-Deck / T-Deck Plus](docs/lilygo.md) | ESP32-S3 | 320 × 240 | lilygo-t-deck | Unverified |
| [LILYGO T-Deck Pro V1.0](docs/lilygo.md) | ESP32-S3 | 240 × 320, e-paper | lilygo-t-deck-pro | Unverified |
| [LILYGO T-Deck Pro V1.1](docs/lilygo.md) | ESP32-S3 | 240 × 320, e-paper | lilygo-t-deck-pro-v11 | Unverified |
| [ATS-mini](docs/ats-mini.md) | ESP32-S3 | 320 × 170, parallel LCD | ats-mini | Unverified |

## Device guides

- [M5Stack device guide](docs/m5stack.md)
- [LILYGO devices](docs/lilygo.md)
- [ATS-mini](docs/ats-mini.md)

T-Deck Pro uses a slower monochrome e-paper display. LILYGO profiles receive through the ESP32-S3 radio; their onboard SI4732, CC1101 and LoRa radios are not used. See the [LILYGO guide](docs/lilygo.md) for details. Tab5 is not supported in this release because its ESP32-P4 has no RF backend; the onboard ESP32-C6 would need separate receiver firmware and a data path to the P4.

## Operating photos

### M5Stack BASIC

![M5Stack BASIC running the standalone spectrum and waterfall display](docs/m5stack-basic-screen.png)

### Cardputer ADV

![Cardputer ADV running the standalone spectrum and waterfall display](docs/cardputer-adv-screen.png)

### M5StickC Plus2

![M5StickC Plus2 running the standalone spectrum and waterfall display](docs/m5stickc-plus2-screen.png)

## Radio and display limits

- Reception is primarily around **2.4 GHz**. Experimental tuning can reach roughly 1.85 GHz, but the accepted 100–6000 MHz setting range does not mean reception works across that entire range. See [RX controls](docs/rx-controls.md).
- The standalone display uses snapshot captures with gaps between them.
- Display levels are signed integer dBFS rounded to 1 dB; antenna input power is not calibrated in dBm.
- This is receive-only firmware for spectrum display and I/Q capture. It does not provide audio demodulation.

## Documentation map

The [documentation index](docs/README.md) groups device guides, build setup, receiver behavior, serial protocol and implementation notes.

## Upstream and credits

The radio capture mechanism and original ESP-SDR firmware come from [ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr). This fork focuses on standalone displays and controls for M5Stack, LILYGO and ATS-mini devices. See the [upstream repository](https://github.com/ESPARGOS/esp-sdr) and [project overview](https://espargos.net/espsdr/).

Thanks to the upstream contributors, including [Florian Euchner](https://github.com/Jeija), [Zoltan Doczi](https://github.com/zodoczi) and [14sea](https://github.com/14sea), and to [h0m3us3r](https://github.com/h0m3us3r/eSpDR) for the 5/6 LO investigation that informed lower-frequency tuning.

## License

ESP-SDR is licensed under **GPL-3.0-or-later**; see [LICENSE](LICENSE). If you distribute a modified version, you must make its source available under the GPL.

Third-party code retains its own licenses, including the Apache-2.0 ESP-IDF code in platform/esp32s2/esp_usb_cdc_rom_console/, the [ESP-DSP component](components/esp-dsp/LICENSE), and FFT kernels derived from it in main/targets/esp32s3/s3_fft_rnd.S and main/targets/esp32s31/burst/s31_fft_rnd.S.
