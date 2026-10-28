# Documentation index / ドキュメント一覧

Use this page to find the guide for your task. The root README is the project
overview and supported-device list; detailed steps and reference material live
in the guides below.

目的別にガイドを探すための一覧です。ルートのREADMEにはプロジェクト概要と対応機種を、
詳しい手順や仕様は以下のガイドにまとめています。

## Getting started / はじめに

| Task / 目的 | Guide / ガイド |
| --- | --- |
| Set up ESP-IDF and prepare the repository / ESP-IDFとリポジトリを準備する | [Build setup (English)](build-setup.md), [ビルド環境の準備 (日本語)](build-setup.ja.md) |
| Set up or troubleshoot Windows / Windowsの環境構築・トラブル対応 | [Windows development setup](windows-development-setup.md) |
| Find the firmware profile and pinned ESP-IDF revision / プロファイルとESP-IDFの指定リビジョンを調べる | [firmware-targets.json](../firmware-targets.json) |

## Device guides / 機種別ガイド

| Device / 機種 | Guide / ガイド |
| --- | --- |
| All M5Stack, Cardputer and M5Stick models / M5Stack・Cardputer・M5Stick各機種 | [M5Stack device guide](m5stack.md) |
| T-Display S3 Pro, Pager, T-Embed, T-Deck and T-Deck Pro | [LILYGO devices](lilygo.md) |
| ATS-mini | [ATS-mini](ats-mini.md) |

## Receiver and display / 受信・表示

| Topic / 内容 | Guide / ガイド |
| --- | --- |
| Practical tuning, gain, bandwidth and GPIO controls / 周波数・ゲイン・帯域幅・GPIO設定 | [RX controls](rx-controls.md) |
| FFT and on-chip spectrum processing / FFTとスペクトラム処理 | [On-chip spectra](spectrum.md) |
| Capture path and snapshot/ring modes / サンプル取得とスナップショット・リング動作 | [How the capture works](how-it-works.md) |
| Display memory, rendering and DPX performance / 表示メモリ・描画・DPXの性能 | [M5Stack performance](performance.md) |

## Host connection and protocols / PC接続・プロトコル

| Topic / 内容 | Guide / ガイド |
| --- | --- |
| Burst serial protocol / バースト式シリアルプロトコル | [Serial protocol](protocol.md) |
| Continuous IQ streaming on ESP32-S3 / ESP32-S3の連続I/Qストリーミング | [Continuous IQ stream](iq-stream.md) |
| Experimental ESP32-S31 Ethernet and high-speed USB receiver / 実験的なESP32-S31受信機 | [ESP32-S31 streaming](s31-streaming.md) |
| Firmware build and version reporting / ファームウェアのビルド情報・バージョン表示 | [Firmware version reporting](firmware-version.md) |

## Other chip backends / その他のチップ

- [ESP32-C2 backend](esp32c2.md)
- [ESP32-H2 backend](esp32h2.md)

## Development and regression coverage / 開発・回帰確認

- [Host regression test instructions](../tests/README.md)
