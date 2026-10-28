# ビルド環境の準備

最初にこのガイドでリポジトリとESP-IDFを準備し、その後、機種別ガイドの
ビルド・書き込み手順に進んでください。

## 1. リポジトリを取得する

GitとPythonを用意し、ESP-DSP・M5Unified・M5GFXのサブモジュールを含めて取得します。

~~~sh
git clone --recursive https://github.com/sake846/esp-sdr-m5.git
cd esp-sdr-m5
~~~

サブモジュールを含めずに取得済みの場合は、リポジトリのルートで初期化します。

~~~sh
git submodule update --init --recursive
~~~

## 2. 指定ESP-IDFをインストールする

M5Stack・LILYGO・ATS-mini向けの各プロファイルは、現在ESP-IDFのコミット
`25fe69f946311abdaf9ad56591f25fedbc20ac98`を使用します。
選択したプロファイルに必要なリビジョンは
[firmware-targets.json](../firmware-targets.json)を正としてください。
先にOSごとのESP-IDF前提ツールをインストールします。WindowsではPython 3.12を使い、
GitとPythonにPATHを通してください。詳しくは
[Windows開発環境のセットアップ](windows-development-setup.md)を参照してください。

以下のコマンドはリポジトリと同じ親ディレクトリにESP-IDFをインストールし、
最後にリポジトリのルートへ戻ります。`esp-sdr-m5`ディレクトリから実行してください。

**sh（Linux/macOS、BashまたはZsh）**

~~~sh
cd ..
git clone https://github.com/espressif/esp-idf.git esp-idf-esp-sdr
cd esp-idf-esp-sdr
git checkout 25fe69f946311abdaf9ad56591f25fedbc20ac98
git submodule update --init --recursive
./install.sh esp32,esp32s3
. ./export.sh
cd ../esp-sdr-m5
~~~

**PowerShell（Windows）**

~~~powershell
Set-Location ..
git clone https://github.com/espressif/esp-idf.git esp-idf-esp-sdr
Set-Location esp-idf-esp-sdr
git checkout 25fe69f946311abdaf9ad56591f25fedbc20ac98
git submodule update --init --recursive
.\install.ps1 'esp32,esp32s3'
. .\export.ps1
Set-Location ..\esp-sdr-m5
~~~

このターゲットパッケージはREADMEに載せたM5Stack・LILYGO・ATS-mini機種向けです。
それ以外のプロファイルをビルドする場合は、
`firmware-targets.json`に記載されたチップのツールチェーンも追加してください。

## 3. ターミナルごとにESP-IDFを有効化する

新しいターミナルを開くたびに、リポジトリのルートから以下を実行してから
`idf.py`を使います。

**sh**

~~~sh
. ../esp-idf-esp-sdr/export.sh
~~~

**PowerShell**

~~~powershell
. ..\esp-idf-esp-sdr\export.ps1
~~~

## 4. 機種別にビルドして書き込む

データ通信対応のUSBケーブルで本体を接続し、機種別ガイドのビルドコマンドと
ハードウェア上の注意に従います。`PORT`は本体のシリアルポート名に置き換えてください。
例：`COM5`、`/dev/ttyUSB0`、`/dev/ttyACM0`、`/dev/cu.usbmodem...`。
プロファイルごとに別のビルドディレクトリを使います。

バージョン情報付きのファームウェアを出力する場合は、機種別ガイドにある
`tools/build_firmware.py`を使います。ターゲット、設定、必要なオプション、
ESP-IDFの指定リビジョンは`firmware-targets.json`から取得します。
