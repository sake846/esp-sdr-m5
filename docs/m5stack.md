# M5Stack devices

This guide brings together the build steps, controls and hardware notes for all
M5Stack profiles. Prepare the repository and pinned ESP-IDF once with the
[build setup guide](build-setup.md). The required ESP-IDF revision for each
profile is listed in [firmware-targets.json](../firmware-targets.json).

The M5Stack BASIC menu and display section also describes the shared settings
used by Core2 and M5StickC Plus2. Cardputer uses keyboard controls; the other
profiles have their own controls below.

The CPU percentage alternates with `BAT` every five seconds when battery telemetry
is available on any standalone M5Stack profile. Failed or unsupported battery
readings keep the CPU percentage visible. ADC-based devices and AXP192 devices
estimate the percentage from battery voltage; I2C-equipped Basics report it in
25% steps. Plus2 shows CPU/BAT above its button hints; the 160-pixel StickC uses
a smaller header font.

PaperColor's full screen refresh takes longer than five seconds, so it alternates
CPU/BAT on completed refreshes instead. This avoids repeatedly sampling the same
five-second phase during its slow screen updates. Other e-paper panels show the
current five-second phase when their next refresh completes.

## In this guide

- [M5Stack BASIC](#m5stack-basic)
- [Core2](#core2)
- [Cardputer K132](#cardputer-k132)
- [Cardputer ADV](#cardputer-adv)
- [M5StickC Plus2](#m5stickc-plus2)
- [Additional M5Stack devices](#additional-m5stack-devices)
- [Tab5 status](#tab5)

## M5Stack BASIC

M5Stack Basic (the original ESP32 model) runs a standalone spectrum display
and the existing USB-to-UART ESP-SDR protocol. No external RF module or PSRAM
is required.

### Build and flash

Prepare the repository using the common [build setup guide](build-setup.md).

Activate the ESP-IDF commit pinned by the `m5stack-basic` profile in
[firmware-targets.json](../firmware-targets.json), then run from the repository root:

```sh
idf.py -B build-m5stack-basic -DIDF_TARGET=esp32 \
  -DSDKCONFIG=build-m5stack-basic/sdkconfig \
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-basic' build
idf.py -B build-m5stack-basic -p PORT flash
```

**PowerShell (Windows)**

~~~powershell
idf.py -B build-m5stack-basic -DIDF_TARGET=esp32 -DSDKCONFIG=build-m5stack-basic/sdkconfig '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-basic' build
idf.py -B build-m5stack-basic -p PORT flash
~~~

Replace `PORT` with the board's port, such as `COM5` or `/dev/ttyUSB0`.
The build uses 4 MB of flash, DIO at 40 MHz,
and also fits newer 16 MB Basics.

On Windows, if flashing resets into normal boot and reports boot mode `0x17`,
see the [paired DTR/RTS reset workaround](windows-development-setup.md#basic-resets-into-normal-boot-instead-of-download-mode).

The default LCD driver is ILI9342C. For an early Basic fitted with ILI9341,
enable **ESP-SDR board > Early Basic with ILI9341 LCD** in
`idf.py -B build-m5stack-basic menuconfig`, then rebuild and flash.
LCD inversion is selected using the reset pin's external pull-up, following
the [M5Stack display driver](https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp).

For release artifacts, use:

```sh
python tools/build_firmware.py --profile m5stack-basic --version YOUR_VERSION
```

### Standalone operation

Power on to start at 2440 MHz, with an 80 MHz span, a 10 MHz tuning step, a 1024-point FFT,
16-spectrum averaging and software gain control targeting a raw IQ peak of -12 dBFS. It
starts from PHY gain index 30. Each display update averages FFT results captured
during a nominal 100 ms interval; LCD drawing adds a little to refresh time.

The normal button hints are `TUNE-`, `MENU`, `TUNE+`. Left/right tune on
release. Press the center button to open the menu or advance one item; hold it
for 0.6 seconds to move back one item (once per hold). Left/right decrease or
increase the selected value. Five seconds without input returns to tuning.
The selected item and its value appear in a panel at the center of the screen,
with the value in larger text. Menu navigation wraps between the first and last
items; numeric settings stop at their limits.

| Menu item, in order | Values / action |
| --- | --- |
| FREQ STEP | 1, 5, 10, 20, 100 MHz; default 10 MHz |
| FREQ SPAN | 2, 4, 8, 16, 40, 80 MHz; default 80 MHz |
| Y POS | Trace offset −40 to +40 dB, in 5 dB steps; positive raises the trace |
| WF BRIGHT | Waterfall offset −30 to +30 dB, in 5 dB steps |
| WF CONTRAST | 25–200%, in 25% steps; default 100% |
| WF SPEED | 1/10×, 1/5×, 1/2×, 1×; default one waterfall row per valid display update |
| FFT NUM | 256, 512, 1024, 2048, 4096; default 1024 |
| FFT AVERAGE | 1, 2, 4, 8, 16; default 16 |
| SPEC MODE | NORMAL (default) or DENSITY display |
| AGC TYPE | MANUAL, SOFTWARE (default), HARDWARE; cycles in either direction |
| RF GAIN | Calibrated PHY gain index; changing it selects MANUAL mode |
| FILTER | AUTO (follow span, default), WIDE (DAC 0), DAC 1–127; larger codes narrow the filter |
| ABOUT | Left/right switch between the firmware name and board name |
| RESET | Right restores startup frequency and settings; left exits |

RESET returns to 2440 MHz, STEP 10 MHz, SPAN 80 MHz, FFT 1024, AVE 16,
zero Y POS/brightness offsets, 100% contrast, 1× waterfall speed, NORMAL spectrum, SOFTWARE AGC
starting at gain index 30 (limited to the calibrated range), and AUTO filter.
Settings are kept in RAM and return to defaults on reboot.

M5Stack profiles start with software AGC in standalone operation; serial gain
commands can explicitly override it. Gain changes stay within the calibrated
PHY range. The CPU
percentage estimates time spent capturing, calculating FFTs and drawing the
LCD during each integration/update interval; it is a local display workload
estimate, not a system-wide idle-task measurement.

The center frequency and plot span appear above the plots; frequencies below
them are in MHz. The CPU estimate is at the upper right. The Basic and Core2
omit the settings line below the title and extend the spectrum graph upward
into that space. The selected number
of snapshots is scheduled at evenly spaced positions within each 100 ms
interval and averaged in the power domain. If a capture runs late, missed
positions are skipped rather than bunched together, so the actual number may be
lower than selected. Spans of 2, 4 and 8 MHz keep the receiver at 16 MS/s and
zoom the display into the center of its 16 MHz capture; 16, 40 and 80 MHz spans
use their matching sample rates. Averaging smooths the displayed level without
changing the FFT frequency resolution. `LIVE` means at least one local capture and FFT
succeeded in the current interval; it does not mean a transmitter was detected. `WAIT`
means the latest local capture failed. `DENS` marks density mode, and `PC` means a
serial client owns the receiver. At FFT sizes above 256, the strongest
bin in each displayed frequency group is shown so narrow peaks remain visible.
Both plots use nearly the full LCD width. In NORMAL mode the upper plot is a
thin blue trace with a translucent blue fill beneath it, over a dark grid.
The colors follow SRdeck: a `#2896FF` stroke and `#0050D2` fill at 120/255 opacity.
The RGB565 renderer preblends the fill over black and grid pixels so the grid
stays visible without an extra buffer. Negative frequency offsets are on the left.
Its vertical grid divides the selected span into eight intervals (2 MHz each
at a 16 MHz span). Frequency marks sit between the plots. The lower waterfall
puts each averaged spectrum at the top and scrolls older spectra down, showing
about twelve seconds of history at the nominal 100 ms interval, plus LCD drawing
time. The display converts the upstream 0–255 power code to signed integer
dBFS, rounded to 1 dB (`code / 2 - 84.3 dBFS`). It estimates the minimum FFT-bin
level in the displayed frequency band and smooths it with a 1/32 exponential
average per frame. The trace spans 80 dB, from that floor to 80 dB above it.
The waterfall uses each row's smoothed floor as its reference: the floor is
black, +10 dB blue, +20 dB cyan, +30 dB green, +40 dB yellow, +50 dB red, and
+60 dB or more white, with gradients between those points. The floor reference resets
when tuning, span or FFT size changes. dBFS is referenced to a full-scale,
bin-centered tone with the current IQ and Hann-window normalization; antenna
input power is not calibrated in dBm, and software-controlled PHY gain changes
the corresponding RF input level. Captures are
snapshots with gaps, as on the original ESP32 backend. DC correction estimates
the I/Q mean over each captured waveform before windowing and FFT. A stationary
signal exactly at the LO cannot be distinguished from a DC offset. The three
button hints sit directly above their corresponding physical buttons along the
bottom edge.

The software accepts center frequencies from 100 to 6000 MHz. This does not
mean the radio works over that entire range: reception is primarily around
2.4 GHz, with experimental tuning down to roughly 1.85 GHz. See
[RX controls](rx-controls.md). The speaker is muted; this firmware displays
spectra and captures I/Q, without audio demodulation.

`SPEC MODE` switches the upper graph to a density display. Each averaged FFT
frame contributes once to the density history, weighted by the number of successful
captures in that average; the waterfall uses the same averaged spectrum. The display stores
64 fixed level buckets (about 2 dB each), so moving
Y POS or tracking the floor does not change the saved levels. Blue marks sparse
hits and warmer colors mark more repeated hits; brightness fades with elapsed
time. These weighted, saturating values are qualitative, not probabilities.

Density mode needs up to an additional 19,968 bytes on Basic/Core2, 14,848 bytes
on Cardputer/ADV and 8,128 bytes on Plus2. Pixels sharing a zoomed FFT bin
share one history column;
for example 1024 points at 2 MHz use 8,192 bytes on Basic/Core2 and Cardputer/ADV.
It allocates internal RAM when enabled and releases it
when disabled. Allocation failure keeps the normal trace and shows `RAM` in the
status area (`NO RAM` in the menu). Tuning, span, FFT, averaging, capture gain,
filter or DC changes clear density history. A PC lease freezes it; local resume
clears it. Gain changes during AGC can therefore shorten visible persistence;
manual gain is preferable when comparing signals. Snapshot gaps still exist:
signals outside captured windows cannot appear in either display.

### PC connection

Connect the Basic's USB port and select its serial port in the
[browser SDR viewer](https://espargos.net/espsdr/app/) at **2,000,000 baud**.
Install the USB-UART driver for the bridge on your board if the port is missing.

PC commands have priority. While the PC owns the receiver, local buttons and
local captures pause. The last LCD spectrum and waterfall history stay visible
during a streaming command; idle periods show `PC CONTROL`. Standalone control resumes after
`RELEASE` or five seconds without commands. Settings selected by the PC,
including frequency and gain, remain in effect.

### Board pins

The [official Basic pin map](https://docs.m5stack.com/en/core/Basic) assigns
LCD/SD SPI to GPIO18/19/23, LCD control to GPIO14/27/32/33, SD select to GPIO4,
the speaker to GPIO25, buttons to GPIO39/38/37 and the power-management I2C
bus to GPIO21/22. The Basic profile excludes all these pins from `GPIO?`
and rejects attempts to drive them through the serial GPIO command.
UART0 uses GPIO1/3 through the onboard USB bridge.

## Core2

Core2 uses the ESP32 receiver backend, its 320×240 LCD and three touch zones
along the bottom of the display. The firmware detects the AXP192 or AXP2101
power controller and configures the LCD, backlight and shared LCD/touch reset.
The display touch zones map to the Basic A/B/C controls. The USB connection
uses the onboard USB-UART bridge at 2,000,000 baud. The normal controls are
A: TUNE-, B: MENU, C: TUNE+. B advances the menu; holding B for 0.6 seconds
moves back one item. A/C adjust the selected value. Five seconds without input
returns to tuning. The [BASIC menu](#standalone-operation)
includes display settings, AGC, RF gain, filter, About and Reset.

**sh (Linux/macOS)**

~~~sh
idf.py -B build-m5stack-core2 -DIDF_TARGET=esp32 \
  -DSDKCONFIG=build-m5stack-core2/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-core2 build
idf.py -B build-m5stack-core2 -p PORT flash
~~~

**PowerShell (Windows)**

```powershell
idf.py -B build-m5stack-core2 -DIDF_TARGET=esp32 `
  -DSDKCONFIG=build-m5stack-core2/sdkconfig `
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-core2' build
idf.py -B build-m5stack-core2 -p COMx flash
```

## Cardputer K132

Cardputer K132 uses the ESP32-S3 receiver backend, the 240×135 ST7789 display
and the built-in keyboard matrix. Key `,` tunes down; `.` or `/` tunes up.
Keys `T`, `P`, `F` and `A` cycle STEP, SPAN, FFT and AVE. Hold Shift with `T`,
`P`, `F` or `A` to cycle that setting in reverse. Enter four digits to set the
frequency in MHz, and press Space to show or hide keyboard help. Shift+Space
toggles density mode for the upper graph. Density history clears when capture gain changes.
Software AGC stays enabled during standalone operation; serial gain commands can explicitly
override it. The standalone defaults are FFT 1024, AVE 16 and gain index 30.
The local FFT menu goes up to 4096 points and uses internal RAM.
Its screen uses a compact spectrum trace and waterfall layout. The USB
connection uses native USB Serial/JTAG. The settings line is omitted so the
spectrum graph can extend upward.

**sh (Linux/macOS)**

~~~sh
idf.py -B build-m5stack-cardputer -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG=build-m5stack-cardputer/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer build
idf.py -B build-m5stack-cardputer -p PORT flash
~~~

**PowerShell (Windows)**

```powershell
idf.py -B build-m5stack-cardputer -DIDF_TARGET=esp32s3 `
  -DSDKCONFIG=build-m5stack-cardputer/sdkconfig `
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer' build
idf.py -B build-m5stack-cardputer -p COMx flash
```

## Cardputer ADV

Cardputer ADV uses the same 240×135 display and ESP32-S3 backend as K132, but
its keyboard is connected through a TCA8418 I²C controller. The firmware
uses the same keyboard controls as K132. Its local FFT menu also goes up to
4096 points. Use its separate profile and defaults file:

**sh (Linux/macOS)**

~~~sh
idf.py -B build-m5stack-cardputer-adv -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG=build-m5stack-cardputer-adv/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer-adv build
idf.py -B build-m5stack-cardputer-adv -p PORT flash
~~~

**PowerShell (Windows)**

```powershell
idf.py -B build-m5stack-cardputer-adv -DIDF_TARGET=esp32s3 `
  -DSDKCONFIG=build-m5stack-cardputer-adv/sdkconfig `
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer-adv' build
idf.py -B build-m5stack-cardputer-adv -p COMx flash
```

## M5StickC Plus2

M5StickC Plus2 uses the ESP32 backend, its portrait 135×240 ST7789 display and
the built-in A/B/C buttons. Button C (GPIO35, left side) tunes down,
A (GPIO37, center) opens/advances the menu, and B (GPIO39, right side) tunes up.
In the menu, C/B adjust the value and holding A for 0.6 seconds moves back one
item. Five seconds without input returns to tuning. The firmware asserts GPIO4
early to keep the power rail on. Both settings lines are omitted, and the
spectrum graph extends upward to just below the title. USB serial uses its
onboard USB-UART bridge at 2,000,000 baud.

**sh (Linux/macOS)**

~~~sh
idf.py -B build-m5stickc-plus2 -DIDF_TARGET=esp32 \
  -DSDKCONFIG=build-m5stickc-plus2/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc-plus2 build
idf.py -B build-m5stickc-plus2 -p PORT flash
~~~

**PowerShell (Windows)**

```powershell
idf.py -B build-m5stickc-plus2 -DIDF_TARGET=esp32 `
  -DSDKCONFIG=build-m5stickc-plus2/sdkconfig `
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc-plus2' build
idf.py -B build-m5stickc-plus2 -p COMx flash
```

The three-button builds share the [BASIC menu](#standalone-operation).
Cardputer builds use the keyboard controls listed above. Release artifacts can be built with
`python tools/build_firmware.py --profile m5stack-core2 --version VERSION` or
`python tools/build_firmware.py --profile m5stack-cardputer --version VERSION`,
`--profile m5stack-cardputer-adv` or `--profile m5stickc-plus2`.

## Additional M5Stack devices

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
ESP32 and ESP32-S3 profiles allow 256–4096-point FFTs.
CoreS3, StickS3 and both Paper profiles use OPI PSRAM for display storage. DinMeter
and the original StickC / Plus use internal memory. RF and FFT buffers stay in internal SRAM.

Stick系とDinMeterは横画面です。電子ペーパーでは操作表示を読みやすくするため2倍に拡大します。
FFTはESP32・ESP32-S3ともに256〜4096点です。画面用PSRAMと受信・FFT用の内蔵SRAMを使い分けます。

### Build and flash / ビルド・書き込み

Initialize submodules and activate the pinned ESP-IDF environment using the
[build setup guide](build-setup.md).
Select a profile from the table. This helper also enables the required
`ESP_SDR_M5UNIFIED` CMake option. Use a fresh output directory for each export.
The one-line commands below can be run from sh or PowerShell.

サブモジュールの取得と指定ESP-IDFの有効化は[ビルド環境ガイド](build-setup.ja.md)を参照してください。
以下のプロファイル名を上表から選びます。補助ツールが必要なCMakeオプションも設定します。
エクスポート先は毎回新しいディレクトリを指定してください。以下は一行コマンドのため、shとPowerShellのどちらでも実行できます。

```console
git submodule update --init --recursive
python tools/build_firmware.py --profile m5stack-cores3 --version local --output artifacts/local
idf.py -B build-m5stack-cores3 -p PORT flash
```

For a direct build, add `-DESP_SDR_M5UNIFIED=ON`:

```console
idf.py -B build-m5stack-cores3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-m5stack-cores3/sdkconfig -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cores3 -DESP_SDR_M5UNIFIED=ON build
```

M5StickC / Plus use UART0 over their USB bridge at 2,000,000 baud.
The other profiles use native USB Serial/JTAG, with UART disabled to protect
onboard peripherals. PC control takes priority and drains local input.

M5StickC／PlusはUSB変換チップ経由のUART0（2,000,000 baud）、その他はネイティブUSBを使用します。
PC操作中は本体の入力を破棄し、PC側の設定を優先します。

### Controls / 操作

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

### E-paper refresh / 電子ペーパーの更新

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

### Tab5

Tab5 is not implemented in this release. Its display/application CPU is an
ESP32-P4, which has no RF backend here. Its onboard ESP32-C6 is a separate Wi-Fi
coprocessor; using it for ESP-SDR requires a dedicated C6 receiver image and an
SDIO I/Q/control transport to a P4 display application. The stock Wi-Fi firmware
does not provide the raw I/Q capture used by this project.

Tab5は今回は未対応です。表示側のESP32-P4用受信処理がなく、内蔵ESP32-C6も独立したチップです。
C6用の受信ファームウェアと、P4へのI/Q・制御データ転送を別途実装する必要があります。

### Official references / 公式資料

- [CoreS3](https://docs.m5stack.com/en/core/CoreS3), [DinMeter](https://docs.m5stack.com/en/core/DinMeter), [StickS3](https://docs.m5stack.com/en/core/StickS3)
- [StickC](https://docs.m5stack.com/en/core/StickC), [StickC Plus](https://docs.m5stack.com/en/core/StickC-Plus)
- [PaperColor](https://docs.m5stack.com/en/core/PaperColor), [PaperS3](https://docs.m5stack.com/en/core/PaperS3), [Tab5](https://docs.m5stack.com/en/core/Tab5)
- [M5Unified](https://github.com/m5stack/M5Unified/tree/35903583211e1f39ca88920714525fbcd1eb714a), [M5GFX](https://github.com/m5stack/M5GFX/tree/347a23274081a105cb9fc0fc3bbf0e15c78e79c3)
