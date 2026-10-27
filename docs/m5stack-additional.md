# Additional M5Stack devices / 追加M5Stack機種

These profiles use the existing ESP32 / ESP32-S3 RF receivers and the pinned
M5Unified 0.2.27 / M5GFX 0.2.33 submodules for displays, touch and power management.
They have **not been verified on physical hardware**.

既存のESP32／ESP32-S3受信処理と、固定リビジョンのM5Unified・M5GFXを使用します。
**追加機種の実機動作は未確認です。**

| Device / 機種 | Profile / プロファイル | Flash | Display / 表示 |
| --- | --- | --- | --- |
| CoreS3 | `m5stack-cores3` | 16 MB | 320 × 240 LCD / touch |
| DinMeter | `m5stack-din-meter` | 8 MB | 240 × 135 LCD / encoder |
| M5StickS3 | `m5sticks3` | 8 MB | 240 × 135 LCD / buttons |
| M5StickC Plus | `m5stickc-plus` | 4 MB | 240 × 135 LCD / buttons |
| M5StickC | `m5stickc` | 4 MB | 160 × 80 LCD / buttons |
| PaperColor | `m5stack-paper-color` | 16 MB | 600 × 400 color e-paper / buttons |
| PaperS3 | `m5stack-papers3` | 16 MB | 960 × 540 grayscale e-paper / touch |

The Stick and DinMeter displays run in landscape. PaperColor and PaperS3 render
at 300 × 200 and 480 × 270 respectively, then scale by two for readable controls.
ESP32 profiles allow 256–8192-point FFTs; ESP32-S3 profiles allow 256–4096 points.
CoreS3, StickS3 and both Paper profiles use OPI PSRAM for display storage. DinMeter
and the original StickC / Plus use internal memory. RF and FFT buffers stay in internal SRAM.

Stick系とDinMeterは横画面です。電子ペーパーでは操作表示を読みやすくするため2倍に拡大します。
FFTはESP32で256〜8192点、ESP32-S3で256〜4096点です。画面用PSRAMと受信・FFT用の内蔵SRAMを使い分けます。

## Build and flash / ビルド・書き込み

Initialize submodules and activate the pinned ESP-IDF environment from the README.
Select a profile from the table. This helper also enables the required
`ESP_SDR_M5UNIFIED` CMake option. Use a fresh output directory for each export.

READMEの手順でサブモジュールを取得し、指定のESP-IDFを有効化してください。
以下のプロファイル名を上表から選びます。補助ツールが必要なCMakeオプションも設定します。
エクスポート先は毎回新しいディレクトリを指定してください。

```sh
git submodule update --init --recursive
python tools/build_firmware.py --profile m5stack-cores3 --version local --output artifacts/local
idf.py -B build-m5stack-cores3 -p PORT flash
```

For a direct build, add `-DESP_SDR_M5UNIFIED=ON`:

```sh
idf.py -B build-m5stack-cores3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-m5stack-cores3/sdkconfig -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cores3 -DESP_SDR_M5UNIFIED=ON build
```

M5StickC / Plus use UART0 over their USB bridge at 2,000,000 baud.
The other profiles use native USB Serial/JTAG, with UART disabled to protect
onboard peripherals. PC control takes priority and drains local input.

M5StickC／PlusはUSB変換チップ経由のUART0（2,000,000 baud）、その他はネイティブUSBを使用します。
PC操作中は本体の入力を破棄し、PC側の設定を優先します。

## Controls / 操作

| Device / 機種 | Controls / 操作 |
| --- | --- |
| CoreS3 / PaperS3 | Touch the left / middle / right third of the bottom quarter: decrease / menu / increase. 画面下部の左・中央・右で減少・メニュー・増加。 |
| DinMeter | Turn to adjust; click to advance the menu, hold to select the previous item. 回転で値変更、短押しで次の項目、長押しで前の項目。 |
| StickS3 / StickC / Plus | A decreases, B increases, A+B opens or advances the menu. Aで減少、Bで増加、同時押しでメニュー。 |
| PaperColor | A decreases, B advances the menu, C increases. Aで減少、Bでメニュー、Cで増加。 |

Hold the menu input to select the previous item. The menu normally closes after
five seconds without input; PaperColor keeps it open for two minutes so it can
survive a color refresh.

メニュー入力の長押しで前の項目に移ります。通常は5秒間操作しないと閉じますが、
PaperColorはカラー更新を待てるよう2分間保持します。

## E-paper refresh / 電子ペーパーの更新

Both Paper profiles copy complete frames into PSRAM and refresh on a separate
task. Pending frames are replaced by the latest frame while the display is busy;
reception, local controls and serial commands continue. PaperS3 uses a white
background and waits one second after each refresh. PaperColor uses its six-color
panel and waits 30 seconds after each refresh. The panel's own refresh time is
additional, so PaperColor is intended for slowly changing snapshots. Its menu
and tuning feedback are delayed by that refresh cycle.

描画を別タスクに分け、画面更新中も受信・本体操作・PC通信を継続します。
更新待ちの画像は最新のものに置き換えます。PaperS3は白背景で、更新完了後に1秒待ちます。
PaperColorは6色表示で、更新完了後に30秒待ちます。パネル自体の更新時間も加わるため、
PaperColorはゆっくり変化するスナップショット表示向けです。操作結果の表示にも同じ遅延があります。

## Tab5

Tab5 is not implemented in this release. Its display/application CPU is an
ESP32-P4, which has no RF backend here. Its onboard ESP32-C6 is a separate Wi-Fi
coprocessor; using it for ESP-SDR requires a dedicated C6 receiver image and an
SDIO I/Q/control transport to a P4 display application. The stock Wi-Fi firmware
does not provide the raw I/Q capture used by this project.

Tab5は今回は未対応です。表示側のESP32-P4用受信処理がなく、内蔵ESP32-C6も独立したチップです。
C6用の受信ファームウェアと、P4へのI/Q・制御データ転送を別途実装する必要があります。

## Official references / 公式資料

- [CoreS3](https://docs.m5stack.com/en/core/CoreS3), [DinMeter](https://docs.m5stack.com/en/core/DinMeter), [StickS3](https://docs.m5stack.com/en/core/StickS3)
- [StickC](https://docs.m5stack.com/en/core/StickC), [StickC Plus](https://docs.m5stack.com/en/core/StickC-Plus)
- [PaperColor](https://docs.m5stack.com/en/core/PaperColor), [PaperS3](https://docs.m5stack.com/en/core/PaperS3), [Tab5](https://docs.m5stack.com/en/core/Tab5)
- [M5Unified](https://github.com/m5stack/M5Unified/tree/35903583211e1f39ca88920714525fbcd1eb714a), [M5GFX](https://github.com/m5stack/M5GFX/tree/347a23274081a105cb9fc0fc3bbf0e15c78e79c3)
