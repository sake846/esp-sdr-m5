# LILYGO standalone devices

The profiles below use the ESP32 or ESP32-S3 built-in radio and provide the
ESP-SDR serial protocol and 256–4096-point FFT. The onboard
SI4732, CC1101, nRF24 and LoRa radios are not receive backends for this firmware.
Their antennas do not feed the ESP32's RF input. AM/FM/SW audio reception is not
provided by these profiles.

以下のプロファイルは、ESP32またはESP32-S3の内蔵無線回路で受信し、本体画面に表示します。
ESP-SDRのシリアルプロトコルと256〜4096点のFFTを利用できます。
SI4732・CC1101・nRF24・LoRaモジュールでの受信や、AM/FM/SWの音声復調には対応しません。
これらのモジュール用アンテナはESP32の受信入力には接続されていません。

| Device / 機種 | Display / 画面 | Profile / プロファイル |
| --- | --- | --- |
| T-Display S3 Pro | ST7796U, 480 × 222 | `lilygo-t-display-s3-pro` |
| T-Watch S3 | ST7789V3, 240 × 240, touch | `lilygo-t-watch-s3` |
| T-Watch 2020 V1 | ST7789, 240 × 240, touch | `lilygo-t-watch-2020-v1` |
| T-Watch 2020 V2 | ST7789, 240 × 240, touch | `lilygo-t-watch-2020-v2` |
| T-Watch 2020 V3 | ST7789, 240 × 240, touch | `lilygo-t-watch-2020-v3` |
| T-LoRa Pager (T-Pager) | ST7796U, 480 × 222 | `lilygo-t-pager` |
| T-Embed SI4732 | ST7789, 320 × 170 | `lilygo-t-embed-si4732` |
| T-Embed CC1101 | ST7789, 320 × 170 | `lilygo-t-embed-cc1101` |
| T-Embed CC1101 Plus | ST7789, 320 × 170 | `lilygo-t-embed-cc1101-plus` |
| T-Deck / T-Deck Plus | ST7789, 320 × 240 | `lilygo-t-deck` |
| T-Deck Pro V1.0 | UC8253 / GDEQ031T10, 240 × 320 | `lilygo-t-deck-pro` |
| T-Deck Pro V1.1 | UC8253 / GDEQ031T10, 240 × 320 | `lilygo-t-deck-pro-v11` |

All profiles select 16 MB flash. T-Watch 2020 V1/V2/V3 use the onboard USB-to-UART
bridge at 2 Mbps. The ESP32-S3 profiles use native USB Serial/JTAG with UART disabled.
T-Watch, Pager and T-Deck Pro use QSPI PSRAM for display history/images.
T-Display S3 Pro uses OPI PSRAM. RF and FFT buffers stay in internal SRAM.
The other profiles do not require PSRAM. T-Embed SI4732 and CC1101 are verified on real hardware; the other profiles have **not been tested on physical hardware**.
T-Deck Pro MAX is a separate board and is not covered by the Pro profiles.

全プロファイルで16 MBフラッシュを使用します。T-Watch 2020 V1・V2・V3のPC接続には
本体のUSB–UART変換回路を使い、通信速度は2 Mbpsです。ESP32-S3搭載機はネイティブUSB Serial/JTAGを使います。
T-Watchシリーズ・Pager・T-Deck Proは画面履歴・画像をQSPI PSRAMに置き、受信・FFT用の内蔵SRAMを確保します。
T-Display S3 ProはOPI PSRAMを使用し、その他のプロファイルではPSRAMを使用しません。T-Embed SI4732・CC1101は実機で動作確認済みですが、**それ以外のプロファイルは実機での動作は未確認です。**
T-Deck Pro MAXは別基板のため、上記のPro用プロファイルには含みません。

## Build and flash / ビルド・書き込み

Activate the pinned ESP-IDF revision using the
[build setup guide](build-setup.md). Replace the
profile below with the appropriate entry from the table; replace `PORT` with
the USB serial port. The commands work in PowerShell and sh.

まず[ビルド環境ガイド](build-setup.ja.md)で指定リビジョンのESP-IDFを有効化してください。
以下のプロファイル名を上表から選び、`PORT`を本体のUSBシリアルポート名に置き換えます。
PowerShellとshで同じコマンドを使用できます。

```sh
idf.py -B build-lilygo-t-pager -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-lilygo-t-pager/sdkconfig -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.lilygo-t-pager build
idf.py -B build-lilygo-t-pager -p PORT flash
```

For T-Watch 2020, use `IDF_TARGET=esp32` and select the profile for the revision
printed on your watch. The backlight GPIO and touch reset wiring differ across revisions.

T-Watch 2020では `IDF_TARGET=esp32` を指定し、機種のリビジョンに合うプロファイルを選びます。
リビジョンごとにバックライトのGPIOやタッチのリセット配線が異なります。V2の場合は次のコマンドを使います。

```sh
idf.py -B build-lilygo-t-watch-2020-v2 -DIDF_TARGET=esp32 -DSDKCONFIG=build-lilygo-t-watch-2020-v2/sdkconfig -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.lilygo-t-watch-2020-v2 build
idf.py -B build-lilygo-t-watch-2020-v2 -p PORT flash
```

Use a separate build directory for each profile. The firmware catalog also adds
these profiles to the existing CI build/export matrix automatically.

プロファイルごとにビルドディレクトリを分けてください。
配布用ファームウェアのCIビルド・エクスポート対象にも自動的に追加されます。

## Controls / 操作

- On T-Watch 2020 and S3, tap `TUNE-` or `TUNE+` at the bottom of the screen to change the
  frequency or selected setting. Tap `MENU` to advance and hold it to return to tuning.
  Use the side power button to turn the watch back on after `POWER OFF`.
- T-Display S3 Pro: BOOT (GPIO0) decreases, GPIO12 selects the next setting,
  GPIO16 increases. Hold GPIO12 to return to tuning. SY6970 charging and its
  watchdog are disabled for battery-free USB operation; SD is deselected and
  the optional camera is held powered down. Touch is not used.
- Pager and T-Embed: turn the encoder to tune or change the selected setting;
  click it to move through the settings, hold it to return to tuning.
- Pager, T-Deck and Pro keyboards: `A`/`D` (also `H`/`L`) decrease/increase,
  `Space` or `Enter` selects the next setting, `Q` closes the menu.
- The BOOT button also acts as the menu button on T-Deck and Pro.
- The menu closes after five seconds without input. PC control takes priority
  and discards local key/encoder events during its serial lease.

T-Watch 2020とS3では、画面下部の `TUNE-` と `TUNE+` で周波数や選択中の設定値を変更します。
`MENU` のタップで次の設定項目へ進み、長押しで周波数操作に戻ります。
`POWER OFF` の後は、側面の電源ボタンで起動します。

T-Display S3 ProはBOOT（GPIO0）で値を減らし、GPIO12で次の設定、GPIO16で値を増やします。
GPIO12の長押しで周波数操作へ戻ります。USB給電で電池がなくても使えるよう、SY6970の充電と
ウォッチドッグを無効にします。SDを非選択にし、カメラは停止します。タッチ操作は使用しません。

PagerとT-Embedは、ダイヤルを回して周波数や選択中の設定値を変更します。
ダイヤルの短押しで設定項目を送り、長押しで周波数操作へ戻ります。
Pager・T-Deck・Proのキーボードでは、`A`/`D`（または`H`/`L`）で値を変更し、
`Space`/`Enter`で次の設定項目、`Q`でメニューを閉じます。
T-DeckとProではBOOTボタンもメニュー操作に使えます。
5秒間操作しないとメニューが閉じます。PC操作中は本体操作よりPC側を優先します。

T-Deck touch/trackball and Plus side buttons are not needed for this control
scheme. Pager power management enables its keyboard through the XL9555 expander;
unused radios, audio amplifier and SD power remain off.

T-Deckのタッチ・トラックボール、CC1101 Plusの追加ボタンは使用しません。
PagerはXL9555経由でキーボードを有効化し、使用しない無線モジュール・アンプ・SDの
電源を無効にします。

## Battery display / バッテリー表示

The status display cycles through CPU, BAT and GAIN every five seconds.
T-Watch 2020 uses the AXP202 fuel gauge and T-Watch S3 uses the AXP2101. Pager, CC1101/Plus and
T-Deck Pro use the BQ27220 fuel gauge.
T-Deck, T-Embed SI4732 and T-Display S3 Pro estimate charge from voltage. SI4732
uses the board example's 1.702 mV/count calibration. SY6970 voltage measurements
on T-Display S3 Pro are unavailable with USB input while charging is disabled.
Missing, failed or out-of-range readings are shown as `BAT --%`.
E-paper shows the selected value when the next panel refresh completes.

ステータス表示はCPU・BAT・GAINを5秒ごとに切り替えます。
T-Watch 2020はAXP202、T-Watch S3はAXP2101、Pager・CC1101/Plus・T-Deck ProはBQ27220の残量計を使います。
その他の機種では電圧から残量を推定します。
T-Display S3 Proは充電を無効にしているため、USB給電中はバッテリー電圧を取得できません。
残量を取得できない場合は `BAT --%` と表示します。電子ペーパーへの反映は画面更新時になります。

## T-Deck Pro e-paper / 電子ペーパー

Pro uses a monochrome spectrum and dithered waterfall. Refresh runs in a separate
task, with at least one second between refresh cycles. After five partial updates,
the next update is full to clear accumulated ghosting. A full update visibly
flashes the panel. A stuck BUSY signal times out without stopping USB or tuning.
Use the V1.1 profile only on V1.1: its display reset is GPIO16; V1.0 has no reset pin.

Proではスペクトラムとウォーターフォールを白黒で表示します。
画面更新は別タスクで行い、更新サイクル間に1秒以上の待ち時間を設けます。
部分更新を5回行うと、残像を消すため次は全面更新になります。その際は画面が明滅します。
BUSY信号の異常で画面更新が止まっても、USB通信と周波数操作は継続します。
V1.1は画面リセットにGPIO16を使用します。リセット端子がないV1.0とはプロファイルを分けてください。

## Hardware references / ハードウェア資料

- [T-LoRa Pager pin definitions](https://github.com/espressif/arduino-esp32/blob/master/variants/lilygo_tlora_pager/pins_arduino.h),
  [LILYGO Pager display and power setup](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/src/LilyGo_LoRa_Pager.cpp)
- [T-Embed SI4732 example](https://github.com/Xinyuan-LilyGO/T-Embed/tree/main/examples/SI473x_Shield),
  [T-Embed LCD setup](https://github.com/Xinyuan-LilyGO/T-Embed/blob/main/examples/esp-idf-v5.0/components/tembed/src/lcd_st7789.c)
- [T-Embed CC1101 and Plus wiring](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/examples/utilities.h)
- [T-Deck wiring](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/utilities.h)
- [T-Deck Pro V1.0 wiring](https://github.com/Xinyuan-LilyGO/T-Deck-Pro/blob/master/examples/factory/utilities.h),
  [V1.1 wiring](https://github.com/Xinyuan-LilyGO/T-Deck-Pro/blob/HD-V2-250915/examples/factory/utilities.h),
  [keyboard map](https://github.com/Xinyuan-LilyGO/T-Deck-Pro/blob/master/examples/factory/peri_keypad.cpp)
- [Pro QSPI PSRAM configuration](https://github.com/Xinyuan-LilyGO/T-Deck-Pro/blob/master/boards/T-Deck-Pro.json)
- [GxEPD2 UC8253 initialization and update sequence, Jean-Marc Zingg, GPLv3](https://github.com/ZinggJM/GxEPD2/blob/master/src/gdeq/GxEPD2_310_GDEQ031T10.cpp)

T-Display S3 Pro wiring and panel initialization follow the [official repository](https://github.com/Xinyuan-LilyGO/T-Display-S3-Pro).

T-Watch S3 follows the [LILYGO hardware reference](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/hardware/lilygo-t-watch-s3.md) and [ST7789V3 initialization](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/src/display/LilyGoDispInterface.cpp).

T-Watch 2020 pin and power maps are documented by LILYGO for [V1](https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library/blob/master/docs/watch_2020_v1.md), [V2](https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library/blob/master/docs/watch_2020_v2.md) and [V3](https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library/blob/master/docs/watch_2020_v3.md).
