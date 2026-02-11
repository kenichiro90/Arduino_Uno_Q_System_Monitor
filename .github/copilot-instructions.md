# Arduino RPC Test Project

## プロジェクト概要

Arduino Uno Q を使用した RPC (Remote Procedure Call) 機能のテスト実装プロジェクトです。
**Arduino Uno Q 内蔵の** Qualcomm QRB2210 MPU (Linux) と STM32U585 MCU (Zephyr) の間で、シリアル通信による双方向 RPC を実現します。

## システム構成

```
Arduino Uno Q (単一ボード)
┌─────────────────────────────────────────┐
│  MPU (QRB2210)        MCU (STM32U585)   │
│  ┌──────────────┐     ┌──────────────┐  │
│  │ Debian Linux │     │ Zephyr RTOS  │  │
│  │ Python       │     │ C言語        │  │
│  │              │     │ Arduino API  │  │
│  └──────┬───────┘     └──────┬───────┘  │
│         │                    │          │
│         │   /dev/ttyHS1      │ Serial1  │
│         │  ◄────────────────►│          │
│         │  arduino-router    │          │
│         │  MessagePack RPC   │          │
└─────────┴────────────────────┴──────────┘
```

### 主要コンポーネント

- **MCU側 (STM32U585)**: Zephyr OS 上で Arduino API を使用、C言語でRPC関数を実装
- **MPU側 (QRB2210)**: Debian Linux 上で Python スクリプトを実行、MCU の関数を RPC 経由で呼び出し
- **Arduino Router**: Linux サービスとして常駐、MessagePack RPC による通信を仲介
  - MPU: `/dev/ttyHS1` (Linux側のシリアルデバイス)
  - MCU: `Serial1` (Arduino側のハードウェアシリアル)
  - **重要**: これらは `arduino-router` サービスが占有しており、直接アクセス禁止

## 開発ワークフロー

### Arduino App Lab の使用（推奨）

Arduino Uno Q の開発には **Arduino App Lab** の使用を推奨します。Arduino スケッチ、Python スクリプト、Linuxアプリケーションを統合して開発できます。

```bash
# PC からリモート開発する場合（PC-Hosted Mode）
# Arduino App Lab を起動し、ネットワーク経由で Arduino Uno Q に接続

# または Arduino Uno Q 上で直接開発（SBC Mode）
# ディスプレイ・キーボード・マウスを接続して使用
```

**重要な概念:**
- **Brick (ブリック)**: コードビルディングブロック（Arduino スケッチ、Python スクリプトなど）
- **App**: 複数の Brick を組み合わせたプロジェクト単位
- **Bridge ライブラリ**: MPU ↔ MCU 間の RPC 通信を抽象化

### Arduino IDE での MCU プログラミング（ベータ版）

Arduino IDE 2+ で STM32 MCU のみをプログラム可能（MPU側は非対応）:

```bash
# Arduino IDE で Uno Q Zephyr Core をインストール
# Board Manager → "UNO Q" で検索 → インストール
# ボード: "Arduino UNO Q"
# ポート: /dev/cu.usbserial-* (macOS) または /dev/ttyUSB* (Linux)
```

**制約**: Arduino IDE では MPU (Python) との統合開発は不可。MCU単体のテストやライブラリ検証のみに使用。

### シリアルポート確認 (macOS)

```bash
# USB シリアルポートの確認
ls /dev/cu.* | grep usb

# 権限エラーが出た場合
sudo chmod 666 /dev/cu.usbserial-*
```

## コーディング規約

### 言語標準とスタイルガイド

**C/C++**
- **言語標準**: C11 (ISO/IEC 9899:2011) および C++11 (ISO/IEC 14882:2011) に準拠
- Arduino環境ではC++11の機能を使用可能（`constexpr`, `nullptr`, range-based for等）
- VLA (可変長配列) は使用禁止（C99機能だがC11ではオプション）
- 固定サイズバッファには `static const` で定数定義

**Python**
- **スタイルガイド**: PEP 8 (Style Guide for Python Code) に準拠
- インデント: 4スペース
- 行長: 最大79文字（ドキュメント文字列/コメントは72文字）
- import順序: 標準ライブラリ → サードパーティ → ローカル

### 例外処理の制約（絶対遵守）

**重要**: 例外を握り潰してはならない。

#### Python
- **汎用キャッチ禁止**: `except Exception:` で包括的にキャッチし、デフォルト値を返す実装は禁止
- **適切な例外選択**: キャッチする例外は、プロジェクト仕様と関連コードを理解した上で適切に選ぶ
  - 例: ファイル操作 → `OSError`, `FileNotFoundError`, `PermissionError`
  - 例: ネットワーク → `socket.error`, `ConnectionError`, `TimeoutError`
  - 例: JSON パース → `json.JSONDecodeError`
- **例外情報の記録**: キャッチした例外は必ずログ出力（例外タイプ、メッセージ、トレースバック）
- **再送出の検討**: 上位層で処理すべき例外は `raise` で再送出

#### C/C++
- **戻り値チェック**: 関数の戻り値（成功/失敗、NULL/非NULL）を必ず確認
- **NULL ポインタチェック**: すべての関数で引数の NULL チェックを実施
- **範囲チェック**: 配列インデックス、バッファサイズの境界チェック
- **エラー伝播**: エラー状態は `bool` 戻り値や特殊値（-1, NULL等）で上位に伝える

#### 例外処理実装前の必須確認事項
1. `docs/PROJECT_SPECIFICATION.md` でエラーハンドリング方針を確認
2. 関連コードの例外処理パターンを確認
3. 上位層でのエラー処理方式を確認
4. キャッチすべき具体的な例外タイプを特定

### 命名規則

**Arduino (C/C++言語)**
- 関数名: `snake_case` (例: `rpc_handle_request`)
- 定数: `UPPER_SNAKE_CASE` または `k` + `PascalCase` (例: `RPC_BUFFER_SIZE`, `kBufferSize`)
- 変数: `snake_case` (例: `request_buffer`)
- グローバル変数: 接頭辞 `g_` (例: `g_serial_buffer`)
- 構造体/列挙型: `PascalCase` (例: `SystemStats`, `MetricType`)

**Python**
- 関数名: `snake_case` (例: `send_rpc_request`)
- クラス名: `PascalCase` (例: `RpcClient`)
- 定数: `UPPER_SNAKE_CASE` (例: `DEFAULT_BAUDRATE`)
- モジュール名: `snake_case` (例: `system_monitor.py`)

### Arduino (C言語) の実装パターン

**必須インクルード**:
```c
#include <Arduino_RouterBridge.h>  // Bridge RPC ライブラリ
```

**セットアップ**:
```c
void setup() {
  Bridge.begin();  // RPC 通信を初期化
  Monitor.begin(); // デバッグ出力用（Serial の代替）
  
  // RPC 関数を公開（MCU → MPU から呼び出し可能に）
  Bridge.provide("set_led_state", set_led_state);
  // または provide_safe() でメインループで安全に実行
  Bridge.provide_safe("read_sensor", read_sensor);
}
```

**重要な制約**:
- `Serial` と `Serial1` は `arduino-router` が占有 → **直接使用禁止**
- デバッグ出力は `Monitor.print()` / `Monitor.println()` を使用
- `Bridge.provide()` 内から `Bridge.call()` や `Monitor.print()` を呼ぶと**デッドロック**発生
- ハードウェア操作は `provide_safe()` で登録し、`loop()` 内で実行させる

### C言語での変数初期化の原則（絶対遵守）

**重要**: この制約に従わないコードは受け付けない。同じ過ちを繰り返さないこと。

MISRA-C Rule 1.1（Compliance）に準拠し、すべての自動変数は関数スコープの開始時に宣言し、決定論的に初期化する必要があります。

#### 原則（MISRA-C Rule 1.1 準拠）
1. **宣言位置の統一**: すべての自動変数（ローカル変数）を関数スコープの開始時に宣言する
2. **決定論的初期化**: すべての変数は宣言時に初期化するか、宣言直後に初期化関数で初期化する
3. **中間宣言の禁止**: 関数本体の実行ステートメント開始後に新規変数を宣言してはならない（C99 VLA除外）

#### 違反パターン（絶対禁止）

**違反1：関数の途中での変数宣言**
```c
// ❌ 絶対に禁止：関数の途中で変数を宣言
void example_bad() {
  // ... 初期処理
  uint8_t buffer[kMatrixWidth * kMatrixHeight];  // ← 関数の途中での宣言は禁止
}
```

**違反2：関数の途中での宣言（初期化有）**
```c
// ❌ 絶対に禁止：関数の途中で変数を宣言し初期化
void func() {
  some_operation();
  int value = 42;  // ← 関数の途中での宣言は禁止
}
```

#### 正しいパターン（必須）

**パターン1：宣言時に初期化子で初期化**
```c
// ✅ 正しい：関数開始時に宣言し、初期化子で初期化
void example_good1() {
  uint8_t buffer[kMatrixWidth * kMatrixHeight] = {0};  // 関数開始時
  int value = 0;  // 関数開始時
  
  // buffer と value はすぐに使用可能
}
```

**パターン2：初期化処理を関数内で実行（推奨方法）**
```c
// ✅ 正しい：関数開始時に宣言、関数内で処理を実行
void example_good2() {
  uint8_t buffer[kMatrixWidth * kMatrixHeight];  // 関数開始時に宣言
  
  // ... 処理
  buffer[0] = 0;  // 関数内のどこで使用しても OK
  // ... さらに処理
}
```

**パターン3：計算結果や関数戻り値での代入**
```c
// ✅ 正しい：関数開始時に初期値で宣言、後で計算結果を代入
void example_good3() {
  float result = 0.0f;  // 関数開始時に初期値を指定
  
  // ... 処理
  result = calculate();  // 計算結果を代入（許可）
}
```

**パターン4：実装例**
```c
// ✅ 正しい：関数開始時に宣言・初期化、別行で代入
void example() {
  int status = 0;          // 関数開始時
  bool is_ready = false;   // 関数開始時
  
  operation();
  
  status = do_work();      // 別行で代入（OK）
}
```

#### 初期化パターン判定表

| 初期化パターン | 宣言位置 | 初期化時期 | 備考 |
|-------------------|---------|----------|------|
| リテラル初期化 | 関数開始時 | 宣言と同時 | `int x = 0;` - 必須 |
| 配列ゼロ初期化 | 関数開始時 | 宣言と同時 | `uint8_t buf[N] = {0};` - 必須 |
| ポインタ初期化 | 関数開始時 | 宣言と同時 | `ptr = NULL;` または `ptr = &var;` - 必須 |
| 計算結果による初期化 | 関数開始時のダミー値 | 計算実行後 | 仮初期値: `int x = 0;` 本初期化: `x = calculate();` - 許可 |
| 関数戻り値による初期化 | 関数開始時のダミー値 | 関数呼び出し後 | 仮初期値: `int x = 0;` 本初期化: `x = get_value();` - 許可 |

#### 実装チェックリスト
- [ ] すべてのローカル変数が関数スコープの開始時に宣言されているか
- [ ] 実行ステートメント開始後に新規変数を宣言していないか
- [ ] すべての変数が決定論的に初期化されているか（宣言時またはその直後）
- [ ] ポインタは NULL または有効アドレスで初期化されているか

### Python の実装パターン

**Arduino App Lab 使用時**:
```python
from arduino.app_utils import *
import time

def loop():
    # MCU の関数を RPC 経由で呼び出し
    result = Bridge.call("set_led_state", True)
    time.sleep(1)

App.run(user_loop=loop)
```

**カスタムスクリプト (MessagePack RPC 直接使用)**:
```python
import socket
import msgpack

SOCKET_PATH = "/var/run/arduino-router.sock"

# RPC リクエスト: [type=0, msgid, method, params]
request = [0, 1, "set_led_state", [True]]
packed = msgpack.packb(request)

with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
    client.connect(SOCKET_PATH)
    client.sendall(packed)
    response = msgpack.unpackb(client.recv(1024))
```

## プロトコル設計の考慮事項

- **メッセージフォーマット**: MessagePack RPC (arduino-router が自動処理)
- **通信層**: Unix Domain Socket (`/var/run/arduino-router.sock`) ↔ シリアル (`/dev/ttyHS1` ↔ `Serial1`)
- **エラーハンドリング**: Bridge ライブラリがタイムアウトとエラー伝播を管理
- **双方向通信**: MPU → MCU と MCU → MPU の両方向で関数呼び出しが可能
- **スレッドセーフティ**: `provide()` は高優先度スレッドで実行、`provide_safe()` はメインループで実行

## Arduino Router サービス管理

```bash
# サービスステータス確認
systemctl status arduino-router

# サービス再起動（通信トラブル時）
sudo systemctl restart arduino-router

# ログ確認（リアルタイム）
journalctl -u arduino-router -f

# 詳細ログを有効化
sudo nano /etc/systemd/system/arduino-router.service
# ExecStart 行末に --verbose を追加
sudo systemctl daemon-reload
sudo systemctl restart arduino-router
```

## プロジェクトの実装目的

このプロジェクトでは、**Linux システムモニタリング情報を Arduino Uno Q の LED matrix に表示** する実装を目指しています。

### 処理フロー

```
1. MPU (Linux) 側
   └─ CPU使用率、メモリ使用量、ディスク使用量、ネットワーク通信量を取得
   └─ RPC 経由で MCU へ送信

2. MCU (STM32U585) 側
  └─ RPC で受け取った情報を取得
  └─ 前処理して LED matrix 用の形式に変換
  └─ リングバッファに格納して時系列表示
```

### 実装の分担

**MPU 側 (Python)**:
- `/proc` や `psutil` を使用してシステム情報を収集
- 定期的（例: 1秒ごと）に更新されたデータを MCU へ RPC で送信
- メソッド例: `get_system_stats()` → `{"cpu": 45.2, "memory": 58.5, "disk": 72.1, "network_rx": 1024}`

**MCU 側 (C言語)**:
- MPU から `receive_system_stats` で受信
- 取得した情報を解析し、LED matrix 用に変換
- リングバッファに格納し、時系列表示

```
rpc_test/
├── arduino/
│   ├── rpc_test.ino        # メインスケッチ (MCU側)
│   ├── system_display.h    # LCD表示関連の関数
│   └── i2c_config.h        # I2C通信設定
├── python/
│   ├── main.py             # エントリーポイント
│   ├── system_monitor.py   # システム情報取得モジュール
│   ├── rpc_server.py       # RPC サーバー実装
│   └── requirements.txt    # 依存パッケージ (psutil等)
└── .github/
    └── copilot-instructions.md
```

## トラブルシューティング

- **シリアルポートが見つからない**: `ls /dev/cu.*` または `ls /dev/tty.*` で確認
- **Permission denied エラー**: `sudo chmod 666 /dev/cu.usbserial-*` で権限付与
- **アップロード失敗**: ボードとポート設定を確認、他のシリアルモニターを閉じる
- **RPC 通信が失敗**: `sudo systemctl restart arduino-router` でサービスを再起動
- **デッドロック発生**: `provide()` 内で `Bridge.call()` や `Monitor.print()` を使用していないか確認
- **ネットワークモードで検出されない**: UDP 5353 ポート (mDNS) がファイアウォールでブロックされていないか確認
- **Arduino IDE で MCU アップロード失敗**: Zephyr Core が正しくインストールされているか確認

## コード品質のベストプラクティス（開発知見）

本プロジェクトの開発を通じて得られた、Arduino MCU プログラミングのベストプラクティスを記載します。

### 1. 単一責任の原則による関数設計

**原則**: 1つの関数は1つの責務のみを持つ。複雑な処理は複数の小さな関数に分割する。

**実装パターン**:

```cpp
// ❌ 避けるべき：複数の責務を1つの関数で処理
static void process_all_commands(const char* cmd) {
  // コマンド解析（20行）
  // + metric コマンド処理（30行）
  // + period コマンド処理（25行）
  // + bright コマンド処理（25行）
  // 計100行以上
}

// ✅ 推奨：各責務を個別関数に分離
static void handle_metric_command(const char* arg);
static void handle_period_command(const char* arg);
static void handle_bright_command(const char* arg);

// 親関数はディスパッチャーのみ
static void process_command(const char* cmd) {
  // ...コマンド特定...
  // ...対応するハンドラーを呼び出し...
}
```

**チェックリスト**:
- [ ] 関数の目的が1文で説明できるか
- [ ] 関数内の `if-else` が5个以下か
- [ ] 関数の行数が30行以下か
- [ ] 関数が複数の異なる概念を扱っていないか

### 2. エラー処理の早期return パターン

**原則**: 入力検証とエラーチェックを関数の先頭に配置し、異常系をすぐに処理する（ガード節）

**実装パターン**:

```cpp
static void handle_period_command(const char* arg) {
  uint32_t value = 0;
  bool has_digit = false;
  
  // ①数値パース
  while (*arg && isdigit(static_cast<unsigned char>(*arg))) {
    has_digit = true;
    value = (value * 10u) + (uint32_t)(*arg - '0');
    arg++;
  }
  
  // ②エラーチェック（ガード節）
  if (!has_digit) {
    Monitor.println("[ERR] period <200-5000>");
    return;  // ← 早期 return で異常系を処理
  }
  
  // ③正常系のみ（ネストが浅い）
  if (value < 200u) value = 200u;
  if (value > 5000u) value = 5000u;
  g_display_config.update_period_ms = value;
  
  // ④フィードバック
  char buffer[kResponseBufferSize];
  snprintf(buffer, sizeof(buffer), "[OK] period set to %lu ms", (unsigned long)value);
  Monitor.println(buffer);
  Monitor.flush();
}
```

**メリット**:
- 異常系と正常系が視覚的に分離
- ネストが最小化（読みやすい）
- 制御フローが単純

### 3. 設定値の自動補正（クランピング）の活用

**原則**: ユーザー入力を検証後、自動的に有効範囲に調整し、実際の設定値をフィードバック

**目的**: エラー出力をしつつ、ユーザーの意図を尊重する設計

```cpp
// 入力: 100 ms（下限200 msより小さい）
// → 自動的に200 msに調整
if (value < 200u) {
  value = 200u;
}

// 入力: 10000 ms（上限5000 msより大きい）
// → 自動的に5000 msに調整
if (value > 5000u) {
  value = 5000u;
}

// フィードバック
snprintf(buffer, sizeof(buffer), "[OK] period set to %lu ms", (unsigned long)value);
Monitor.println(buffer);
```

**ユーザー体験**:
```
> period 100
[OK] period set to 200 ms   ← エラーではなく、補正を通知

> bright 10
[OK] brightness set to 7    ← 自動調整された実値を通知
```

### 4. テーブル駆動設計の活用

**原則**: コマンド定義と処理ロジックを分離し、テーブル駆動アーキテクチャを採用

**パターン**:

```cpp
// ① コマンド定義テーブル（static const）
typedef struct {
  const char* name;
  const char* help_line;
} Command;

static const Command kCommands[] = {
  {"metric", "metric cpu|mem|disk     - Display metric"},
  {"period", "period <200-5000>       - Update interval (ms)"},
  {"bright", "bright <0-7>            - LED brightness"},
  {"show",   "show                    - Show config"},
  {"help",   "help                    - Show this help"}
};
static const size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

// ② コマンド検索関数
static int find_command(const char* cmd);

// ③ ハンドラー関数テーブル（インデックスに対応）
static void handle_metric_command(const char* arg);
static void handle_period_command(const char* arg);
static void handle_bright_command(const char* arg);
static void handle_show_command(void);
static void handle_help_command(void);

// ④ ディスパッチャー
static void handle_command(const char* cmd) {
  int cmd_index = find_command(cmd);
  if (cmd_index < 0) {
    Monitor.println("[ERR] unknown cmd (type help)");
    return;
  }
  
  const char* arg = get_command_arg(cmd, cmd_index);
  
  // インデックスに基づいてハンドラーを呼び出し
  if (cmd_index == 0)
    handle_metric_command(arg);
  else if (cmd_index == 1)
    handle_period_command(arg);
  // ...以下省略...
}
```

**メリット**:
- 新しいコマンド追加時にテーブルとハンドラーを追加するだけ
- `kCommandCount` で自動的に正確な要素数を取得
- ヘルプテキストの一元管理
- コマンド処理の追加が直感的

### 5. MISRA-C Rule 1.1 の厳密な遵守

**原則**: すべてのローカル変数を関数スコープの開始時に宣言し、決定論的に初期化する

**ループ内での変数使用パターン**:

| パターン | 宣言位置 | 例 | 推奨 |
|---------|---------|-----|------|
| ループ制御変数のみ | ループスコープ内 | `for (int i = 0; ...)` | ✅ OK |
| ループ内1行のみ使用 | ループスコープ内 | `int x = i * 2;` (この行内) | ✅ OK |
| ループ内複数行で使用 | 関数スコープ開始時 | 複数ステートメント間 | ✅ 必須 |

**違反パターン（避けるべき）**:

```cpp
// ❌ 違反：複数ステートメント内で変数をループ内宣言
for (uint8_t col = 0; col < kMatrixWidth; ++col) {
  size_t idx = something(col);      // ← ループ内で毎回宣言
  float val = get_value(idx);       // ← ループ内で毎回宣言
  result[col] = process(val);       // ← idx と val を複数行で使用
}

// ✅ 改善：複数行で使用される変数は関数スコープで宣言
size_t idx = 0;
float val = 0.0f;

for (uint8_t col = 0; col < kMatrixWidth; ++col) {
  idx = something(col);
  val = get_value(idx);
  result[col] = process(val);
}
```

### 6. グローバル状態の構造化

**原則**: 関連するグローバル変数を構造体にまとめ、状態管理を明確にする

```cpp
// ✅ 推奨：関連状態を1つの構造体で管理
typedef struct {
  DisplayMode mode;           // 表示メトリクス
  uint32_t update_period_ms;  // 更新間隔
  uint32_t last_draw_ms;      // 最終描画時刻
  uint32_t last_sample_ms;    // 最終サンプル時刻
  uint8_t brightness;         // LED輝度
} DisplayConfig;

static DisplayConfig g_display_config = {
  DISPLAY_CPU, 200, 0, 0, 7
};
```

**メリット**:
- 関連する設定が論理的に結合
- 初期化が一箇所で管理可能
- 新しい設定値の追加が簡単
- グローバル変数の数を削減

---

## 変更管理ルール（絶対遵守）

**重要**: この制約は何があっても必ず守らなければならない。

### 変更履歴の事前確認義務

**あらゆる操作を行う前に**、以下の手順を必ず実行すること:

1. `docs/changelogs/` ディレクトリ内の全ファイルを確認
2. 最新の変更内容、影響範囲、注意事項を把握
3. 確認結果を元に、新しいアクションの計画を立案
4. 過去の変更と矛盾しないことを確認してから実行

**確認対象**:
- 削除された機能を再追加していないか
- 過去に修正したバグを再発させていないか
- 既存の設計方針と一貫性があるか

### プロジェクト仕様書の参照義務

**あらゆる実装・変更を行う前に**、以下のドキュメントを必ず確認すること:

1. **`docs/PROJECT_SPECIFICATION.md`** - プロジェクト仕様書
   - 機能要件、データ構造、インターフェース仕様を確認
   - エラーハンドリング方針、制約事項を把握
   - 特に例外処理を実装する際は必須

2. **確認タイミング**:
   - 新機能追加時
   - 既存機能修正時
   - 例外処理実装時
   - RPC インターフェース変更時
   - データ構造変更時

3. **確認事項**:
   - 実装が仕様と一致しているか
   - 制約事項に違反していないか
   - 削除された機能を使用していないか
   - データフォーマットが正しいか

### 変更記録の義務

コードに対して以下のいずれかの操作を行った場合、**必ず変更内容を Markdown ファイルとして記録する**:

- **機能追加** - 新しい機能、コマンド、RPC関数の追加
- **機能削除** - 既存の機能、コマンド、変数、定数の削除
- **機能改変** - 既存機能の動作変更、リファクタリング、バグ修正
- **コーディング規約修正** - 変数初期化の問題など、コーディング標準の遵守

### 変数初期化チェック（コードレビュー項目）

**条件**: すべてのコード変更を対象にレビュー時に実施

#### チェックリスト
- [ ] すべてのローカル変数が関数スコープの開始時に宣言されているか
- [ ] 実行ステートメント開始後に新規変数を宣言していないか
- [ ] すべての変数が決定論的に初期化されているか（宣言時またはその直後）
- [ ] ポインタは NULL または有効アドレスで初期化されているか

#### 違反検出時の対応
1. **即座の修正**: 違反箇所の立件を指示
2. **原因の記録**: codelog に記録（同じ過ちの繰り返し防止）
3. **確認の実施**: 修正前後で必ずチェックリストを再実施

### 記録フォーマット

変更ごとに以下の情報を含む Markdown ファイルを作成する:

```markdown
# [変更日付] 変更タイトル

## 変更内容
- 何を追加/削除/改変したか

## 影響範囲
- どのファイルが変更されたか

## 理由
- なぜこの変更を行ったか

## 検証
- 変更後の動作確認方法
```

### ファイル命名規則

```
CHANGELOG_YYYYMMDD_brief_description.md
例: CHANGELOG_20260211_remove_network_feature.md
```

### 配置場所

プロジェクトルート直下の `docs/changelogs/` ディレクトリに配置。

### 例外なし

- 小さな変更でも記録必須
- コメント修正のみの場合も記録
- タイポ修正のみの場合も記録

この制約に違反した場合、すべての変更をロールバックし、Markdown ファイルを作成してから再実施する。