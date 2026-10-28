# ESP-SDR for M5Stack, LILYGO and ATS-mini

[English](README.md) | 日本語

M5Stack・LILYGO・ATS-miniの内蔵ESP32を使い、スペクトラムとウォーターフォールを本体の画面に表示します。外付けSDR機器やPCを接続せず、単体で動作します。

[ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr)をベースに、各機種の画面と操作部で動作するようにした派生版です。ファームウェアのビルドと書き込みにはPCが必要ですが、書き込み後は本体だけで使えます。

## 特徴

- 本体画面にスペクトラムとスクロール式ウォーターフォールを表示。
- 周波数、表示スパン、FFTサイズ、平均化、ゲインを本体で操作。
- M5Stack・LILYGO・ATS-mini向けのプロファイルを用意。
- ESP-SDRのUSB/UARTプロトコルを使ったPC接続にも対応し、I/Q取得やスペクトラム表示が可能。
- ESP32・ESP32-S3プロファイルで256〜4096点のFFTに対応。

## はじめに

1. 下の一覧で機種と実機での動作確認状況を確認します。
2. [ビルド環境ガイド](docs/build-setup.ja.md)に従ってGit・Python・指定ESP-IDFを準備します。
3. 機種別ガイドでビルドコマンド、操作方法、ハードウェア固有の注意を確認します。

各プロファイルで必要なESP-IDFのリビジョンは
[firmware-targets.json](firmware-targets.json)に記載しています。
ビルド方法と機種ごとの操作説明は、それぞれのガイドを参照してください。

単体起動時は中心周波数2440 MHz、周波数ステップ10 MHz、スパン80 MHz、FFT 1024点、
平均化16回です。M5Stack機種はソフトウェアAGCで起動します。
共通するメニューと表示設定は[M5Stack機種別ガイド](docs/m5stack.md#standalone-operation)を参照してください。

## 対応機種

M5Stack BASIC・Core2・Cardputer通常版K132・Cardputer ADV・M5StickC Plus2は、このプロジェクトで実機動作を確認しています。それ以外のプロファイルは未確認です。

| 機種 | チップ | 画面 | プロファイル | 実機確認 |
| --- | --- | --- | --- | --- |
| [M5Stack BASIC](docs/m5stack.md#m5stack-basic) | ESP32 | 320 × 240 | m5stack-basic | **確認済み** |
| [M5Stack Core2](docs/m5stack.md#core2) | ESP32 | 320 × 240 | m5stack-core2 | **確認済み** |
| [M5Stack CoreS3](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 320 × 240 | m5stack-cores3 | 未確認 |
| [M5Stack DinMeter](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 240 × 135、横画面 | m5stack-din-meter | 未確認 |
| [Cardputer 通常版 K132](docs/m5stack.md#cardputer-k132) | ESP32-S3 | 240 × 135 | m5stack-cardputer | **確認済み** |
| [Cardputer ADV](docs/m5stack.md#cardputer-adv) | ESP32-S3 | 240 × 135 | m5stack-cardputer-adv | **確認済み** |
| [M5StickC](docs/m5stack.md#additional-m5stack-devices) | ESP32 | 160 × 80、横画面 | m5stickc | 未確認 |
| [M5StickC Plus](docs/m5stack.md#additional-m5stack-devices) | ESP32 | 240 × 135、横画面 | m5stickc-plus | 未確認 |
| [M5StickC Plus2](docs/m5stack.md#m5stickc-plus2) | ESP32 | 135 × 240、縦画面 | m5stickc-plus2 | **確認済み** |
| [M5StickS3](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 240 × 135、横画面 | m5sticks3 | 未確認 |
| [M5Stack PaperColor](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 600 × 400、カラー電子ペーパー | m5stack-paper-color | 未確認 |
| [M5Stack PaperS3](docs/m5stack.md#additional-m5stack-devices) | ESP32-S3 | 960 × 540、白黒電子ペーパー | m5stack-papers3 | 未確認 |
| [LILYGO T-Display S3 Pro](docs/lilygo.md) | ESP32-S3 | 480 × 222 | lilygo-t-display-s3-pro | 未確認 |
| [LILYGO T-LoRa Pager](docs/lilygo.md) | ESP32-S3 | 480 × 222 | lilygo-t-pager | 未確認 |
| [LILYGO T-Embed SI4732](docs/lilygo.md) | ESP32-S3 | 320 × 170 | lilygo-t-embed-si4732 | 未確認 |
| [LILYGO T-Embed CC1101](docs/lilygo.md) | ESP32-S3 | 320 × 170 | lilygo-t-embed-cc1101 | 未確認 |
| [LILYGO T-Embed CC1101 Plus](docs/lilygo.md) | ESP32-S3 | 320 × 170 | lilygo-t-embed-cc1101-plus | 未確認 |
| [LILYGO T-Deck / T-Deck Plus](docs/lilygo.md) | ESP32-S3 | 320 × 240 | lilygo-t-deck | 未確認 |
| [LILYGO T-Deck Pro V1.0](docs/lilygo.md) | ESP32-S3 | 240 × 320、電子ペーパー | lilygo-t-deck-pro | 未確認 |
| [LILYGO T-Deck Pro V1.1](docs/lilygo.md) | ESP32-S3 | 240 × 320、電子ペーパー | lilygo-t-deck-pro-v11 | 未確認 |
| [ATS-mini](docs/ats-mini.md) | ESP32-S3 | 320 × 170、並列LCD | ats-mini | 未確認 |

## 機種別ガイド

- [M5Stack機種別ガイド](docs/m5stack.md)
- [LILYGO機種](docs/lilygo.md)
- [ATS-mini](docs/ats-mini.md)

T-Deck Proは白黒電子ペーパーのため、画面更新が低速です。LILYGO機種はESP32-S3内蔵無線回路で受信し、基板上のSI4732・CC1101・LoRa無線モジュールは使用しません。詳しくは[LILYGOガイド](docs/lilygo.md)を参照してください。Tab5は今回は未対応です。画面・アプリ側のESP32-P4用受信処理がなく、内蔵ESP32-C6には専用受信ファームウェアとP4へのデータ転送経路が必要です。

## 動作画面

### M5Stack BASIC

![M5Stack BASICのスペクトラム・ウォーターフォール表示](docs/m5stack-basic-screen.png)

### Cardputer ADV

![Cardputer ADVのスペクトラム・ウォーターフォール表示](docs/cardputer-adv-screen.png)

### M5StickC Plus2

![M5StickC Plus2のスペクトラム・ウォーターフォール表示](docs/m5stickc-plus2-screen.png)

## 受信・表示の制限

- 主な受信帯域は**2.4 GHz付近**です。実験的に約1.85 GHzまで設定できますが、100〜6000 MHzの設定範囲全体で受信できるわけではありません。[受信設定](docs/rx-controls.md)を参照してください。
- 本体画面は間隔を空けたスナップショット受信を使います。
- 表示レベルは符号付き整数dBFS（1 dB単位）で、アンテナ入力電力をdBmで校正した値ではありません。
- 受信専用ファームウェアです。スペクトラム表示とI/Q取得に対応しますが、音声復調は行いません。

## ドキュメント一覧

機種別ガイド、ビルド環境、受信処理、シリアルプロトコル、実装説明は[ドキュメント一覧](docs/README.md)から目的別に探せます。

## 上流プロジェクトと謝辞

無線サンプル取得機構とESP-SDRの基本ファームウェアは[ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr)に由来します。この派生版ではM5Stack・LILYGO・ATS-miniの画面表示と操作に重点を置いています。[上流リポジトリ](https://github.com/ESPARGOS/esp-sdr)と[プロジェクト概要](https://espargos.net/espsdr/)も参照してください。

上流の貢献者である[Florian Euchner](https://github.com/Jeija)、[Zoltan Doczi](https://github.com/zodoczi)、[14sea](https://github.com/14sea)、および低周波チューニングの検討に役立つ情報を公開した[h0m3us3r](https://github.com/h0m3us3r/eSpDR)に感謝します。

## ライセンス

ESP-SDRは**GPL-3.0-or-later**で公開しています。[LICENSE](LICENSE)を参照してください。改変版を配布する場合は、GPLに従ってソースコードも公開する必要があります。

サードパーティーのコードには個別のライセンスが適用されます。ESP-IDF由来のApache-2.0コードはplatform/esp32s2/esp_usb_cdc_rom_console/、[ESP-DSP](components/esp-dsp/LICENSE)、および同コンポーネント由来のFFTカーネルはmain/targets/esp32s3/s3_fft_rnd.Sとmain/targets/esp32s31/burst/s31_fft_rnd.Sにあります。
