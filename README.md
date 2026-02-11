# Arduino Uno Q System Monitor

Arduino Uno Q の LED マトリックスに、Linux システムの CPU・メモリ・ディスク使用率をリアルタイム表示するプロジェクトです。

## 📋 概要

このプロジェクトは、Arduino Uno Q に内蔵された 2つのプロセッサを連携させて動作します：

- **MPU (QRB2210)**: Linux 上で Python プログラムを実行し、システム情報を収集
- **MCU (STM32U585)**: Arduino スケッチで LED マトリックス（13×8）に情報を表示

両者は RPC (Remote Procedure Call) 通信で連携し、リアルタイムにシステム状態を可視化します。

## ✨ 特徴

- **リアルタイム監視**: 200ms 間隔でシステム情報を更新（設定変更可能）
- **3種類のメトリクス**: CPU使用率、メモリ使用率、ディスク使用率
- **LED マトリックス表示**: 13×8 LED で時系列グラフを表示
- **コマンドライン操作**: シリアルモニターから表示設定を変更可能
- **自動起動**: Arduino App Lab で起動後、自動的に監視を開始

## 🛠️ 必要なもの

### ハードウェア

- **Arduino Uno Q** (1台)
  - 内蔵 13×8 LED マトリックス
  - USB-C ケーブル

### ソフトウェア

- **Arduino App Lab** (Arduino Uno Q 開発環境)
  - PC-Hosted Mode での使用を推奨
  - Arduino_RouterBridge ライブラリが必要

## 📥 Arduino App Lab のセットアップ

1. **Arduino App Lab を PC にインストール**
   - Arduino 公式サイトから Arduino App Lab をダウンロード
   - インストーラーの指示に従ってインストール

2. **Arduino App Lab を起動し、Arduino Uno Q に接続**
   - Arduino App Lab を起動
   - **USB** のアイコンが付いている方の「ARDUINO UNO Q」を選択

## 📦 必要なライブラリ

このプロジェクトでは、MPU ↔ MCU 間の RPC 通信に **Arduino_RouterBridge** ライブラリが必要です。

## 🚀 プロジェクトのデプロイ手順

### ステップ1: 新しい App を作成

1. Arduino App Lab を起動
2. 「My Apps」をクリック
3. 「Create new app」をクリック
4. App 名を入力（例: `SystemMonitor`）
5. App が作成され、プロジェクトディレクトリが自動生成されます
   - Arduino Uno Q 上のパス: `/home/arduino/ArduinoApps/SystemMonitor/`

### ステップ2: プロジェクトファイルを配置

プロジェクトファイルを Arduino Uno Q の App ディレクトリに転送します。

#### PC-Hosted Mode の場合

**方法A: SCP コマンドで転送（macOS / Linux）**

1. Arduino App Lab の画面左下にある **Connect to the board's shell** をクリック
2. ターミナルが開くので、以下コマンドを実行

```bash
# sketch フォルダを転送
scp -r sketch/* arduino@${hostname}:/home/arduino/ArduinoApps/SystemMonitor/sketch/

# python フォルダを転送
scp -r python/* arduino@${hostname}:/home/arduino/ArduinoApps/SystemMonitor/python/
```

**方法B: Arduino App Lab の統合エディタで直接編集**

1. Arduino App Lab で作成した App を選択
2. 左側のファイルツリーで `sketch/` フォルダを展開
3. 各ファイルを開き、プロジェクトのコードをコピー&ペースト
   - `sketch/sketch.ino`
   - `sketch/system_display.h`
   - `sketch/system_display.cpp`
4. 同様に `python/` フォルダ内のファイルも配置
   - `python/main.py`
   - `python/system_monitor.py`
   - `python/requirements.txt`

#### SBC Mode の場合

1. このリポジトリを Git でクローン、または USB メモリでコピー
2. ファイルマネージャーで `/home/arduino/ArduinoApps/SystemMonitor/` を開く
3. プロジェクトの `sketch/` および `python/` フォルダの内容を対応するディレクトリにコピー

### ステップ3: アプリの実行

1. **Arduino App Lab でプロジェクトを開く**
   - My Apps から `SystemMonitor` を選択

2. **RUN ボタンを押す**
   - 右上の **「RUN」** ボタンをクリック

3. **動作確認**
   - Arduino Uno Q の LED マトリックスに棒グラフが表示されることを確認
   - 右端が最新のデータ、左端が古いデータです
   - グラフは自動的にスクロールします

## 📊 LED マトリックスの見方

```
LED マトリックス (13列 × 8行)
┌─────────────────────────┐
│                       ■ │
│                     ■ ■ │
│                   ■ ■ ■ │
│                 ■ ■ ■ ■ │
│               ■ ■ ■ ■ ■ │
│             ■ ■ ■ ■ ■ ■ │
│           ■ ■ ■ ■ ■ ■ ■ │
│         ■ ■ ■ ■ ■ ■ ■ ■ │
└─────────────────────────┘
 ← 古い              最新 →
```

- **縦軸**: 使用率 (0%～100%)
- **横軸**: 時間（左が古いデータ、右が最新）
- **高さ**: 使用率に応じて0～8段階で表示

## 🎮 コマンドライン操作

Arduino App Lab のシリアルモニター、または Arduino Uno Q のターミナルから MCU に直接コマンドを送信できます。

### シリアルモニターの開き方

#### Arduino App Lab の場合
- RUN ボタンを押すと Serial Monitor が自動起動します。
- ボーレート: **9600 bps**（固定、変更不可）
- 改行コード: **NL (Newline)**

**注意** Arduino App Lab の Serial Monitor では、`help` コマンドなどの実行結果が正しく表示されません。Arduino IDE の Serial Monitor の使用を推奨します。

#### Arduino IDE の場合
- ボードマネージャーから、**Arduino UNO Q Board** をインストール (初回のみ)
- ツール メニューから ボード を選択: **Arduino UNO Q**
- ツール メニューから ポート を選択: **Arduino UNO Q のポート**
- ツール メニューから シリアルモニター を選択
- ボーレート: **9600 bps**
- 改行コード: **NL (Newline)**

### 利用可能なコマンド

| コマンド       | 説明                      | 例            |
|---------------|--------------------------|---------------|
| `metric cpu`  | CPU使用率を表示            | `metric cpu`  |
| `metric mem`  | メモリ使用率を表示          | `metric mem`  |
| `metric disk` | ディスク使用率を表示        | `metric disk` |
| `period <値>` | 更新間隔を設定 (200-5000ms) | `period 500` |
| `bright <値>` | LED 輝度を設定 (0-7)       | `bright 5`   |
| `show`        | 現在の設定を表示           | `show`        |
| `help`        | ヘルプを表示              | `help`        |

### コマンドの実行例

```
> help
===== COMMANDS =====
metric cpu|mem|disk     - Display metric
period <200-5000>       - Update interval (ms)
bright <0-7>            - LED brightness
show                    - Show config
help                    - Show this help
====================

> metric cpu
[OK] metric set to cpu

> period 200
[OK] period set to 200 ms

> bright 3
[OK] brightness set to 3

> show
[cpu 200ms b3]
```

## 📚 プロジェクト構成

```
arduino_uno_q_system_monitor/
├── README.md                      # このファイル
├── sketch/                        # MCU側プログラム (C/C++)
│   ├── sketch.ino                 # メインスケッチ
│   ├── system_display.h           # LED表示ヘッダー
│   └── system_display.cpp         # LED表示実装
└── python/                        # MPU側プログラム (Python)
    ├── main.py                    # Arduino App Lab エントリーポイント
    ├── system_monitor.py          # システム情報取得モジュール
    └── requirements.txt           # 依存パッケージ
```

## 🔗 関連リンク

- [Arduino Uno Q 公式ページ](https://www.arduino.cc/)
- [Arduino App Lab ドキュメント](https://www.arduino.cc/app-lab)
