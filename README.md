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

| Screen size / orientation | Devices (profiles) | Hardware status |
| --- | --- | --- |
| 320 × 240, landscape | [M5Stack BASIC](docs/m5stack.md#m5stack-basic) (`m5stack-basic`), [Core2](docs/m5stack.md#core2) (`m5stack-core2`), [CoreS3](docs/m5stack.md#additional-m5stack-devices) (`m5stack-cores3`), [LILYGO T-Deck / Plus](docs/lilygo.md) (`lilygo-t-deck`) | BASIC and Core2 verified; CoreS3 and T-Deck unverified |
| 240 × 135, landscape | [Cardputer K132 / ADV](docs/m5stack.md#cardputer-k132) (`m5stack-cardputer`, `m5stack-cardputer-adv`), [DinMeter](docs/m5stack.md#additional-m5stack-devices) (`m5stack-din-meter`) | Cardputers verified; DinMeter unverified |
| 135 × 240, portrait | [M5StickC Plus](docs/m5stack.md#additional-m5stack-devices) (`m5stickc-plus`), [Plus2](docs/m5stack.md#m5stickc-plus2) (`m5stickc-plus2`), [StickS3](docs/m5stack.md#additional-m5stack-devices) (`m5sticks3`) | Plus2 verified; Plus and StickS3 unverified |
| 80 × 160, portrait | [M5StickC](docs/m5stack.md#additional-m5stack-devices) (`m5stickc`) | Unverified |
| 320 × 170, landscape | [LILYGO T-Embed SI4732 / CC1101 / CC1101 Plus](docs/lilygo.md) (`lilygo-t-embed-si4732`, `lilygo-t-embed-cc1101`, `lilygo-t-embed-cc1101-plus`), [ATS-mini](docs/ats-mini.md) (`ats-mini`) | Unverified |
| 480 × 222, landscape | [LILYGO T-Display S3 Pro](docs/lilygo.md) (`lilygo-t-display-s3-pro`), [T-LoRa Pager](docs/lilygo.md) (`lilygo-t-pager`) | Unverified |
| 240 × 320, portrait e-paper | [LILYGO T-Deck Pro V1.0 / V1.1](docs/lilygo.md) (`lilygo-t-deck-pro`, `lilygo-t-deck-pro-v11`) | Unverified |
| 600 × 400, landscape color e-paper | [M5Stack PaperColor](docs/m5stack.md#additional-m5stack-devices) (`m5stack-paper-color`); renders at 300 × 200 | Unverified |
| 960 × 540, landscape grayscale e-paper | [M5Stack PaperS3](docs/m5stack.md#additional-m5stack-devices) (`m5stack-papers3`); renders at 480 × 270 | Unverified |

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
