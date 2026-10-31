# ATS-mini

The `ats-mini` profile uses the ESP32-S3 radio, 320 × 170 LCD and rotary encoder
in the ATS-mini / ESP32-SI4732 pocket receiver. It provides the same standalone
spectrum, waterfall, density display, gain controls and 256–4096-point FFT as
the other S3 standalone profiles. Native USB Serial/JTAG provides the existing burst protocol.

This firmware does not use the SI4732 broadcast receiver or the speaker/headphone
audio path. The telescopic antenna feeds the SI4732, not the ESP32. Receive
frequency limits and practical RF performance are the same as the generic S3
backend; this profile does not add AM/FM/SW reception or audio demodulation.

`ats-mini`プロファイルは、ATS-mini（ESP32-SI4732小型ラジオ）のESP32-S3内蔵無線回路、
320 × 170 LCD、ロータリーエンコーダーを使用します。スペクトラム、ウォーターフォール、
密度表示、ゲイン設定、256〜4096点FFTを本体で操作できます。
PC接続はネイティブUSB Serial/JTAGで、既存のバーストプロトコルを使用します。

SI4732とスピーカー・ヘッドホンの音声経路は使用しません。
伸縮アンテナはSI4732に接続されており、ESP32の受信入力には接続されていません。
受信範囲とRF特性は既存のESP32-S3バックエンドと共通で、AM/FM/SW受信や音声復調の
追加ではありません。

## Hardware variants / 対象ハードウェア

- ESP32-S3, **8 MB or larger** flash. The same image supports N8R2 and N16R8;
  PSRAM stays disabled, so QSPI/OSPI PSRAM types do not require separate images.
- Eight-bit parallel LCD, with ST7789, high-gamma ST7789 and GC9307 IDs detected
  before handing the bus to ESP-IDF's LCD_CAM/i80 driver. Unknown IDs use the
  standard ST7789 sequence.
- The profile follows the pinout below. Other boards sold under similar names
  must have the same wiring to use it. **Physical hardware has not been tested.**

フラッシュは**8 MB以上**を対象とします。PSRAMは無効のため、N8R2とN16R8、QSPIとOSPIで
ファームウェアを分ける必要はありません。LCDは起動時にIDを読み、標準ST7789、
高ガンマ版ST7789、GC9307を判別します。不明なIDは標準ST7789として初期化します。
同名の別基板では、下表と配線が一致していることが前提です。**実機での動作は未確認です。**

| Signal / 信号 | GPIO |
| --- | --- |
| LCD CS / DC / RESET / WR / RD | 6 / 7 / 5 / 8 / 9 |
| LCD D0…D7 | 39, 40, 41, 42, 45, 46, 47, 48 |
| Backlight / バックライト | 38 |
| Encoder A / B / push / エンコーダー | 2 / 1 / 21 |
| SI4732 power / reset / 電源・リセット | 15 / 16 |
| SI4732 SDA / SCL | 18 / 17 |
| Audio mute / amplifier enable / ミュート・アンプ | 3 / 10 |
| Battery ADC / バッテリー電圧 | 4 |

CPU/BAT percentages alternate every five seconds when GPIO4 provides a usable
battery reading. Charge is estimated using the original board firmware's
1.702 mV/count calibration and ten-sample averaging. The monitor reads the USB
supply when connected; voltages above the battery range keep CPU% visible.

GPIO4からバッテリー電圧を取得できる場合は、CPU%とBAT%を5秒ごとに切り替えます。
残量は基板用ファームウェアの補正値と10回の読み取り平均から推定します。
USB給電時など、電圧がバッテリーの範囲を外れる場合はCPU%を表示します。

The SI4732 power rail and amplifier remain off. The SI4732 reset stays low and
its I2C pins are left without pull-ups. GPIO commands cannot drive onboard LCD,
radio, audio, encoder, battery or memory pins. LCD DMA completes before the
renderer reuses its scanline buffer.

SI4732とアンプの電源を無効にし、SI4732のリセットをLowに保ちます。
電源が切れているSI4732のI2C端子はプルアップしません。GPIOコマンドでは、画面・無線・音声・
エンコーダー・バッテリー・メモリの端子を操作できません。
描画用の行バッファは、LCDへのDMA転送完了を待ってから再利用します。

## Build and flash / ビルド・書き込み

Activate the pinned ESP-IDF revision using the
[build setup guide](build-setup.md), then run from the project root. Replace
`PORT` with the USB serial port. These commands work in sh and
PowerShell. Keep a separate build directory for this board.

まず[ビルド環境ガイド](build-setup.ja.md)で指定リビジョンのESP-IDFを有効にしてから、
リポジトリのルートで実行します。
`PORT`はUSBシリアルポート名に置き換えてください。shとPowerShellで同じコマンドを使えます。

```sh
idf.py -B build-ats-mini -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-ats-mini/sdkconfig -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.ats-mini build
idf.py -B build-ats-mini -p PORT flash
```

## Controls / 操作

Turn the encoder to tune or adjust the selected setting. Click to move to the
next menu item; hold for 600 ms to close the menu. Five seconds without input
also closes it. PC control takes priority and discards encoder events during
its serial lease.

ダイヤルを回して周波数や設定値を変更します。短押しで次の設定項目に進み、
600 ms以上の長押しでメニューを閉じます。5秒間操作しない場合もメニューが閉じます。
PC操作中はPC側を優先し、その間の本体操作は破棄します。

## References / 資料

- [ATS-mini hardware notes and original schematic links](https://esp32-si4732.github.io/ats-mini/hardware.html)
- [ATS-mini display IDs, wiring and initialization](https://github.com/esp32-si4732/ats-mini/blob/main/ats-mini/Display.h)
- [ATS-mini radio/audio/encoder wiring](https://github.com/esp32-si4732/ats-mini/blob/main/ats-mini/Common.h)
- [ATS-mini 8 MB QSPI/OSPI build profiles](https://github.com/esp32-si4732/ats-mini/blob/main/.github/workflows/build.yml)
- [LovyanGFX GC9307 register sequence](https://github.com/lovyan03/LovyanGFX/blob/master/src/lgfx/v1/panel/Panel_GC9A01.hpp)

The GC9307 register sequence retains the LovyanGFX FreeBSD license notice in
[`main/boards/standalone_device_ats_mini_lcd.c`](../main/boards/standalone_device_ats_mini_lcd.c).
