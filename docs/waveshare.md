# Waveshare ESP32-S3 Touch AMOLED 2.06

The `waveshare-amoled-206` profile runs the standalone ESP-SDR spectrum and waterfall display on the Waveshare ESP32-S3-Touch-AMOLED-2.06 watch development board. It uses the ESP32-S3 radio; its 410 × 502 CO5300 AMOLED is driven over QSPI, and FT3168 touch input controls the display.

The board has 32 MB flash and 8 MB octal PSRAM. The profile keeps the display history in PSRAM and the RF capture buffers in internal SRAM. The onboard audio codec, SD card, IMU and RTC are unused.

## Build and flash

Use the pinned ESP-IDF setup in the [build setup guide](build-setup.md). From the repository root:

```sh
idf.py -B build-waveshare-amoled-206 -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG=build-waveshare-amoled-206/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.waveshare-amoled-206 \
  -DESP_SDR_WAVESHARE_AMOLED_206=ON build
idf.py -B build-waveshare-amoled-206 -p PORT flash
```

Replace `PORT` with the board's USB serial port. The ready-to-flash release profile is also available through `tools/build_firmware.py --profile waveshare-amoled-206`.

## Controls

Touch the left, center or right third of the screen to decrease, advance or increase the selected value. GPIO18 also advances the menu. A long center press returns to tuning. The menu closes after five seconds without input. Connect over native USB Serial/JTAG for the ESP-SDR serial protocol.

## WaveShare ESP32-S3 Touch AMOLED 2.06

`waveshare-amoled-206`プロファイルは、WaveShare ESP32-S3-Touch-AMOLED-2.06でスペクトラムとウォーターフォールを表示します。ESP32-S3内蔵の無線回路で受信し、410 × 502のCO5300 AMOLEDをQSPI、FT3168タッチパネルを操作に使います。

フラッシュは32 MB、PSRAMは8 MBのOctalです。画面履歴をPSRAMに置き、受信バッファは内蔵SRAMに確保します。内蔵オーディオ、SDカード、IMU、RTCは使用しません。

## ビルド・書き込み

[ビルド環境ガイド](build-setup.ja.md)で指定リビジョンのESP-IDFを有効化し、リポジトリのルートで実行します。

```sh
idf.py -B build-waveshare-amoled-206 -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG=build-waveshare-amoled-206/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.waveshare-amoled-206 \
  -DESP_SDR_WAVESHARE_AMOLED_206=ON build
idf.py -B build-waveshare-amoled-206 -p PORT flash
```

`PORT`はUSBシリアルポート名に置き換えてください。`tools/build_firmware.py --profile waveshare-amoled-206`でも配布用プロファイルをビルドできます。

## 操作

画面の左・中央・右をタッチすると、選択値を下げる・メニューを進める・上げる操作になります。GPIO18もメニュー送りに使えます。中央を長押しすると周波数操作に戻り、5秒間操作しないとメニューを閉じます。ESP-SDRのシリアル通信にはネイティブUSB Serial/JTAGを使います。
