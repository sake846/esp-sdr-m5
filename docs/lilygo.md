# LILYGO standalone devices

The profiles below use the ESP32-S3's built-in radio with the same receive
frequency range, USB protocol and 256–4096-point FFT as Cardputer. The onboard
SI4732, CC1101, nRF24 and LoRa radios are not receive backends for this firmware.
Their antennas do not feed the ESP32's RF input. AM/FM/SW audio reception is not
provided by these profiles.

以下のプロファイルは、ESP32-S3の内蔵無線回路で受信し、本体画面に表示します。
受信範囲・USBプロトコル・FFTサイズ（256〜4096点）はCardputerと共通です。
SI4732・CC1101・nRF24・LoRaモジュールでの受信や、AM/FM/SWの音声復調には対応しません。
これらのモジュール用アンテナはESP32の受信入力には接続されていません。

| Device / 機種 | Display / 画面 | Profile / プロファイル |
| --- | --- | --- |
| T-Display S3 Pro | ST7796U, 480 × 222 | `lilygo-t-display-s3-pro` |
| T-LoRa Pager (T-Pager) | ST7796U, 480 × 222 | `lilygo-t-pager` |
| T-Embed SI4732 | ST7789, 320 × 170 | `lilygo-t-embed-si4732` |
| T-Embed CC1101 | ST7789, 320 × 170 | `lilygo-t-embed-cc1101` |
| T-Embed CC1101 Plus | ST7789, 320 × 170 | `lilygo-t-embed-cc1101-plus` |
| T-Deck / T-Deck Plus | ST7789, 320 × 240 | `lilygo-t-deck` |
| T-Deck Pro V1.0 | UC8253 / GDEQ031T10, 240 × 320 | `lilygo-t-deck-pro` |
| T-Deck Pro V1.1 | UC8253 / GDEQ031T10, 240 × 320 | `lilygo-t-deck-pro-v11` |

All profiles select 16 MB flash and native USB Serial/JTAG, with UART disabled
to avoid driving onboard GPS/audio/radio connections. Pager and Pro use their
QSPI PSRAM for display history/images; T-Display S3 Pro uses OPI PSRAM; RF and FFT buffers stay in internal SRAM.
The other profiles do not require PSRAM. These devices have **not been tested on physical hardware**.
T-Deck Pro MAX is a separate board and is not covered by the Pro profiles.

全プロファイルで16 MBフラッシュとネイティブUSB Serial/JTAGを使用します。
基板上のGPS・音声・無線モジュールとの競合を避けるためUARTは無効です。
PagerとProは画面履歴・画像をQSPI PSRAMに置き、受信・FFT用の内蔵SRAMを確保します。
T-Display S3 ProはOPI PSRAMを使用し、その他のプロファイルではPSRAMを使用しません。**いずれも実機での動作は未確認です。**
T-Deck Pro MAXは別基板のため、上記のPro用プロファイルには含みません。

## Build and flash / ビルド・書き込み

Activate the pinned ESP-IDF revision described in the main README. Replace the
profile below with the appropriate entry from the table; replace `PORT` with
the USB serial port. The commands work in PowerShell and sh.

READMEの手順で指定リビジョンのESP-IDFを有効化してください。
以下のプロファイル名を上表から選び、`PORT`を本体のUSBシリアルポート名に置き換えます。
PowerShellとshで同じコマンドを使用できます。

```sh
idf.py -B build-lilygo-t-pager -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-lilygo-t-pager/sdkconfig -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.lilygo-t-pager build
idf.py -B build-lilygo-t-pager -p PORT flash
```

Use a separate build directory for each profile. The firmware catalog also adds
these profiles to the existing CI build/export matrix automatically.

プロファイルごとにビルドディレクトリを分けてください。
配布用ファームウェアのCIビルド・エクスポート対象にも自動的に追加されます。

## Controls / 操作

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
