# ESP-SDR for M5Stack

[English](README.md) | 日本語

M5Stackの内蔵ESP32を使い、スペクトラムとウォーターフォールを本体の画面に表示します。
単体で動作するため、外付けのSDRハードウェアやPCは必要ありません。

このプロジェクトは、[ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr)を
M5Stackの画面と操作部で単体動作するように改良した派生版です。
ESP32がI/Qサンプルの取得、FFT計算、画面描画を行います。
ビルドとファームウェアの書き込みにはPCが必要ですが、書き込み後は本体だけで使えます。

## 特徴

- 本体のLCDにスペクトラム波形とスクロールするカラーウォーターフォールを表示。
- 受信周波数、チューニングステップ、表示スパン、FFTサイズ、平均化回数を本体で変更。
- M5Stack BASIC、Core2、Cardputer、Cardputer ADV、M5StickC Plus2向けのビルド設定を用意。
- FFTサイズはBASIC・Core2・M5StickC Plus2で256〜8192点、Cardputer・Cardputer ADVで256〜4096点。
- 既存のESP-SDR USB/UARTプロトコルを利用し、PCからのI/Q取得やスペクトラム表示にも対応。

## 対応機種と実機確認状況

**M5Stack BASIC、Core2、通常版Cardputer（K132、非ADV）は実機での動作を確認済みです**。
Cardputer ADVとM5StickC Plus2にはビルド設定がありますが、実機での動作は未確認です。

| 機種 | チップ | 画面 | ビルドプロファイル | 実機確認状況 |
| --- | --- | --- | --- | --- |
| M5Stack BASIC | ESP32 | 320 × 240 | `m5stack-basic` | **確認済み** |
| M5Stack Core2 | ESP32 | 320 × 240 | `m5stack-core2` | **確認済み** |
| Cardputer 通常版 K132 | ESP32-S3 | 240 × 135 | `m5stack-cardputer` | **確認済み** |
| Cardputer ADV | ESP32-S3 | 240 × 135 | `m5stack-cardputer-adv` | 未確認 |
| M5StickC Plus2 | ESP32 | 135 × 240、縦画面 | `m5stickc-plus2` | 未確認 |

M5StickC向けの設定は、**M5StickC Plus2専用**です。
初代M5StickCやM5StickC Plusとの互換性を示すものではありません。
上流から引き継いだその他のデバイス・チップ向け設定も、この派生版では実機確認していません。

## 動作画面

写真は後から追加できます。画像を`docs/`に保存し、該当する仮テキストを削除して、
Markdown画像行のコメントを外してください。

### M5Stack BASIC

*動作写真は後日追加予定です。*

<!-- ![M5Stack BASICのスペクトラム・ウォーターフォール表示](docs/m5stack-basic-screen.jpg) -->

### Cardputer 通常版 K132

*動作写真は後日追加予定です。*

<!-- ![通常版Cardputerのスペクトラム・ウォーターフォール表示](docs/cardputer-screen.jpg) -->

### M5StickC Plus2

*写真は後日追加予定です。実機での動作は未確認です。*

<!-- ![M5StickC Plus2のスペクトラム・ウォーターフォール表示](docs/m5stickc-plus2-screen.jpg) -->

## ビルド環境の準備

### 1. リポジトリの取得

GitとPythonをインストールし、ESP-DSPサブモジュールを含めてリポジトリを取得してください。
以下のコマンドはshとPowerShellの両方で使えます。

```sh
git clone --recursive https://github.com/sake846/esp-sdr-m5.git
cd esp-sdr-m5
```

サブモジュールを含めずに取得済みの場合は、次のコマンドを実行してください。

```sh
git submodule update --init --recursive
```

### 2. 指定されたESP-IDFのインストールと有効化

M5Stack向けの5プロファイルは、現在すべてESP-IDFのコミット
`25fe69f946311abdaf9ad56591f25fedbc20ac98`を使用します。
各プロファイルで必要なリビジョンは[firmware-targets.json](firmware-targets.json)に記載しています。
ビルドには指定のリビジョンを使用してください。

先に、利用するOSに応じたESP-IDFの前提ツールをインストールしてください。
WindowsではPython 3.12を使い、PythonとGitにPATHを通してください。
詳しくは[Windows開発環境のセットアップ](docs/windows-development-setup.md)を参照してください。

以下から利用するシェルのコマンドを選び、`esp-sdr-m5`ディレクトリで実行してください。
どちらもリポジトリと同じ親ディレクトリに指定のSDKをインストールし、最後にリポジトリのルートへ戻ります。

**sh（Linux/macOS、BashまたはZsh）**

```sh
cd ..
git clone https://github.com/espressif/esp-idf.git esp-idf-esp-sdr
cd esp-idf-esp-sdr
git checkout 25fe69f946311abdaf9ad56591f25fedbc20ac98
git submodule update --init --recursive
./install.sh esp32,esp32s3
. ./export.sh
cd ../esp-sdr-m5
```

**PowerShell（Windows）**

```powershell
Set-Location ..
git clone https://github.com/espressif/esp-idf.git esp-idf-esp-sdr
Set-Location esp-idf-esp-sdr
git checkout 25fe69f946311abdaf9ad56591f25fedbc20ac98
git submodule update --init --recursive
.\install.ps1 'esp32,esp32s3'
. .\export.ps1
Set-Location ..\esp-sdr-m5
```

新しいターミナルを開くたびに、`idf.py`の実行前にESP-IDF環境を有効化してください。
リポジトリのルートで、利用するシェルのコマンドを実行します。

**sh**

```sh
. ../esp-idf-esp-sdr/export.sh
```

**PowerShell**

```powershell
. ..\esp-idf-esp-sdr\export.ps1
```

## 機種ごとのビルド・書き込み

データ通信に対応したUSBケーブルで本体を接続し、ESP-IDF環境を有効化したターミナルで、
機種とシェルに合ったコマンドをリポジトリのルートから実行してください。
shではバックスラッシュ（`\`）、PowerShellではバッククォート（`` ` ``）で行を継続します。
行継続の記号は行末に置き、その後に空白を入れないでください。

`PORT`は本体のシリアルポート名に置き換えてください。
Windowsでは`COM5`、Linuxでは`/dev/ttyUSB0`や`/dev/ttyACM0`、
macOSでは`/dev/cu.usbmodem...`などです。
以下の例のように、プロファイルごとにビルドディレクトリを分けてください。

### M5Stack BASIC

**sh**

```sh
idf.py -B build-m5stack-basic -DIDF_TARGET=esp32 \
  -DSDKCONFIG=build-m5stack-basic/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-basic build
idf.py -B build-m5stack-basic -p PORT flash
```

**PowerShell**

```powershell
idf.py -B build-m5stack-basic -DIDF_TARGET=esp32 `
  -DSDKCONFIG=build-m5stack-basic/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-basic build
idf.py -B build-m5stack-basic -p PORT flash
```

この設定は4 MBのフラッシュを使用し、新しい16 MBモデルのBASICにも書き込めます。
標準のLCDドライバーはILI9342Cです。
ILI9341を搭載した初期のBASICでは、`idf.py -B build-m5stack-basic menuconfig`を実行し、
**ESP-SDR board > Early Basic with ILI9341 LCD**を有効にしてから、再度ビルド・書き込みを行ってください。

### M5Stack Core2

**sh**

```sh
idf.py -B build-m5stack-core2 -DIDF_TARGET=esp32 \
  -DSDKCONFIG=build-m5stack-core2/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-core2 build
idf.py -B build-m5stack-core2 -p PORT flash
```

**PowerShell**

```powershell
idf.py -B build-m5stack-core2 -DIDF_TARGET=esp32 `
  -DSDKCONFIG=build-m5stack-core2/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-core2 build
idf.py -B build-m5stack-core2 -p PORT flash
```

Core2のLCDと画面下部の3つのタッチ領域を使用します。
電源管理ICはAXP192とAXP2101に対応する実装を含みます。

### Cardputer 通常版 K132

**sh**

```sh
idf.py -B build-m5stack-cardputer -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG=build-m5stack-cardputer/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer build
idf.py -B build-m5stack-cardputer -p PORT flash
```

**PowerShell**

```powershell
idf.py -B build-m5stack-cardputer -DIDF_TARGET=esp32s3 `
  -DSDKCONFIG=build-m5stack-cardputer/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer build
idf.py -B build-m5stack-cardputer -p PORT flash
```

通常版のCardputerにはこのプロファイルを使用してください。
内蔵キーボードのマトリクスと、ネイティブUSB Serial/JTAGを使用します。

### Cardputer ADV

**sh**

```sh
idf.py -B build-m5stack-cardputer-adv -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG=build-m5stack-cardputer-adv/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer-adv build
idf.py -B build-m5stack-cardputer-adv -p PORT flash
```

**PowerShell**

```powershell
idf.py -B build-m5stack-cardputer-adv -DIDF_TARGET=esp32s3 `
  -DSDKCONFIG=build-m5stack-cardputer-adv/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stack-cardputer-adv build
idf.py -B build-m5stack-cardputer-adv -p PORT flash
```

ADVのキーボードはTCA8418 I²Cコントローラーを使用するため、専用プロファイルが必要です。
通常版Cardputerのプロファイルで代用しないでください。

### M5StickC Plus2

**sh**

```sh
idf.py -B build-m5stickc-plus2 -DIDF_TARGET=esp32 \
  -DSDKCONFIG=build-m5stickc-plus2/sdkconfig \
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc-plus2 build
idf.py -B build-m5stickc-plus2 -p PORT flash
```

**PowerShell**

```powershell
idf.py -B build-m5stickc-plus2 -DIDF_TARGET=esp32 `
  -DSDKCONFIG=build-m5stickc-plus2/sdkconfig `
  -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.m5stickc-plus2 build
idf.py -B build-m5stickc-plus2 -p PORT flash
```

縦向きのLCDと、本体のA/B/Cボタンを使用します。

書き込み後に本体の電源を入れるかリセットすると、単体表示を開始します。
初期設定は中心周波数2412 MHz、表示スパン16 MHz、FFTサイズ256点、自動ゲイン制御です。

## 本体での操作

### BASIC・Core2・M5StickC Plus2

Core2では、A/B/Cに対応する画面上の3つのタッチ領域を使います。

| 操作部 | 短押し | 0.6秒長押し |
| --- | --- | --- |
| A | 選択中のステップ分、周波数を下げる | 表示スパンを切り替える |
| B | チューニングステップを切り替える | FFTサイズを切り替える |
| C | 選択中のステップ分、周波数を上げる | 平均化回数を切り替える |

### Cardputer・Cardputer ADV

| キー | 動作 |
| --- | --- |
| `,` | 選択中のステップ分、周波数を下げる |
| `.`または`/` | 選択中のステップ分、周波数を上げる |
| `0`〜`9` | 4桁の数字で周波数をMHz単位で入力する。例：`2412` |
| `T` | チューニングステップを切り替える |
| `P` | 表示スパンを切り替える |
| `F` | FFTサイズを切り替える |
| `A` | 平均化回数を切り替える |
| `Shift` + `T`、`P`、`F`、`A` | 該当する設定を逆順に切り替える |
| `Space` | キーボード操作のヘルプを表示・非表示にする |

チューニングステップは1・5・20・100 MHz、表示スパンは2・4・8・16・40・80 MHzです。
2・4・8 MHzの表示は、16 MS/sで取得したサンプルの中央部分を拡大します。
平均化回数は1・2・4・8・16回から選べます。

## PCとの接続

単体動作にはPCは不要です。シリアルプロトコルを使う場合はUSBでPCに接続し、
[ESP-SDRブラウザービューアー](https://espargos.net/espsdr/app/)などの対応クライアントを開いてください。
BASIC・Core2・M5StickC Plus2は本体のUSB-UARTブリッジを**2,000,000 baud**で使用し、
CardputerシリーズはネイティブUSB Serial/JTAGを使用します。

PCからのコマンドが優先され、その間は本体の操作とサンプル取得を一時停止します。
`RELEASE`コマンドを受け取るか、5秒間コマンドがなければ単体動作に戻ります。
詳しくは[プロトコル仕様](docs/protocol.md)を参照してください。

## 受信・表示の制限

- これらのM5Stack向けプロファイルは、主に**2.4 GHz付近**を受信します。
  実験的なチューニングで約1.85 GHzまで下げられますが、設定可能な100〜6000 MHzの
  全域で受信できるわけではありません。[受信設定](docs/rx-controls.md)も参照してください。
- 単体表示では断続的にサンプルを取得するため、取得と取得の間に空白期間があります。
- スペクトラムとウォーターフォールのレベルは相対値で、dBmでは校正していません。
- スペクトラム表示とI/Q取得のための受信専用ファームウェアです。音声復調には対応していません。

## 関連ドキュメント

- [M5Stack BASICの表示・ハードウェアに関する説明](docs/m5stack-basic.md)
- [Core2・Cardputer・M5StickC Plus2のハードウェアに関する説明](docs/m5stack-core2-cardputer.md)
- [スペクトラム処理](docs/spectrum.md)
- [ESP-SDRが無線サンプルを取得する仕組み](docs/how-it-works.md)
- [ファームウェアのプロファイルとESP-IDFの指定リビジョン](firmware-targets.json)

## 上流プロジェクトと謝辞

無線サンプルの取得機構と元のESP-SDRファームウェアは、
[ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr)によるものです。
この派生版では、M5Stackの画面表示と本体操作による単体動作に重点を置いています。
元のプロジェクトの幅広いチップ対応や研究については、
[上流リポジトリ](https://github.com/ESPARGOS/esp-sdr)と
[プロジェクト概要](https://espargos.net/espsdr/)を参照してください。

[Florian Euchner](https://github.com/Jeija)、
[Zoltan Doczi](https://github.com/zodoczi)、[14sea](https://github.com/14sea)をはじめとする
上流の貢献者に感謝します。また、低周波数側のチューニングの参考となった5/6 LOの調査を行った
[h0m3us3r](https://github.com/h0m3us3r/eSpDR)にも感謝します。

## ライセンス

ESP-SDRのライセンスは**GPL-3.0-or-later**です。[LICENSE](LICENSE)を参照してください。
改変版を配布する場合は、GPLに従ってソースコードを提供する必要があります。

第三者のコードには、それぞれのライセンスが適用されます。
これには、`platform/esp32s2/esp_usb_cdc_rom_console/`内のApache-2.0ライセンスのESP-IDFコード、
[ESP-DSPコンポーネント](components/esp-dsp/LICENSE)、およびESP-DSP由来のFFTカーネル
`main/targets/esp32s3/s3_fft_rnd.S`と`main/targets/esp32s31/burst/s31_fft_rnd.S`が含まれます。
