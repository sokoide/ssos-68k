# SSOS (Simple/Simple Operating System) アーキテクチャ

## システム概要

SSOSはX68000コンピュータ向けに開発されたシンプルなオペレーティングシステムです。フルブート可能バージョンとローカル実行可能バージョンの両方をサポートしています。

### 主要特徴
- **リアルタイムOS**: 16個までのタスクをサポート
- **メモリ管理**: 4KBブロック単位の動的メモリ割り当て
- **グラフィカルUI**: レイヤーベースのウィンドウシステム
- **ハードウェア制御**: MFPタイマー、キーボード、VRAM直接アクセス
- **デュアルモード**: ディスクブート + ローカル実行

## アーキテクチャ図

### 全体アーキテクチャ

```mermaid
graph TB
    subgraph "SSOSシステムアーキテクチャ"
        A[アプリケーション層] --> B[OSメイン層]
        B --> C[カーネル層]
        C --> D[ハードウェア層]

        subgraph "OSメイン層"
            B1[ssosmain.c] --> B2[ssoswindows.c]
            B2 --> B3[タスク管理]
            B3 --> B4[メモリ管理]
        end

        subgraph "カーネル層"
            C1[タスクマネージャー] --> C2[メモリマネージャー]
            C2 --> C3[割り込みハンドラ]
            C3 --> C4[キーボードドライバ]
            C4 --> C5[VRAMドライバ]
        end

        subgraph "ハードウェア層"
            D1[X68000ハードウェア]
            D1 --> D2[MFPタイマー]
            D1 --> D3[キーボードコントローラ]
            D1 --> D4[VRAMコントローラ]
        end
    end
```

### メモリマップアーキテクチャ

```mermaid
graph LR
    subgraph "メモリレイアウト (ディスクブート)"
        A[0x000000<br/>割り込みベクタ] --> B[0x002000<br/>ブートセクタ]
        B --> C[0x010000<br/>SSOS .text<br/>128KB]
        C --> D[0x030000<br/>SSOS .data<br/>64KB]
        D --> E[0x150000<br/>SSOS .ssos<br/>10MB<br/>動的メモリ]
    end

    subgraph "メモリ管理システム"
        F[メモリマネージャー] --> G[4KBブロック割り当て]
        G --> H[2816ブロック<br/>11MB管理]
        H --> I[タスクスタック<br/>動的割り当て]
    end
```

### タスク管理アーキテクチャ

```mermaid
graph TD
    subgraph "タスクシステム"
        A[タスク管理器] --> B[TCBテーブル<br/>最大16タスク]
        B --> C[レディキュー<br/>優先度別]
        C --> D[ウェイトキュー<br/>イベント待機]
        D --> E[実行中タスク]

        subgraph "タスク状態"
            F[TS_READY<br/>実行可能]
            G[TS_WAIT<br/>待機中]
            H[TS_DORMANT<br/>休止中]
            I[TS_NONEXIST<br/>未使用]
        end
    end

    subgraph "スケジューリング"
        J[タイマー割り込み] --> K[コンテキストスイッチ]
        K --> L[優先度チェック]
        L --> M[タスク切り替え]
    end
```

### ウィンドウシステムアーキテクチャ

```mermaid
graph TB
    subgraph "レイヤーシステム"
        A[レイヤーマネージャー] --> B[最大256レイヤー]
        B --> C[Zオーダー管理]
        C --> D[ダーティ矩形追跡]

        subgraph "描画最適化"
            E[DMA転送] --> F[高速描画]
            F --> G[部分更新]
            G --> H[垂直同期同期]
        end
    end

    subgraph "ウィンドウ構成"
        I[レイヤー1<br/>背景] --> J[レイヤー2<br/>ステータス]
        J --> K[レイヤー3<br/>ウィンドウ]
        K --> L[レイヤー4+<br/>オーバーレイ]
    end
```

### ビルドシステム

```mermaid
graph LR
    subgraph "ビルドプロセス"
        A[メインMakefile] --> B[boot/Makefile]
        A --> C[os/Makefile]
        B --> D[アセンブリコード<br/>main.s]
        C --> E[OSコンポーネント<br/>カーネル/メイン/ユーティリティ]

        D --> F[BOOT.X.bin]
        E --> G[SSOS.X.bin]
        F --> H[makedisk]
        G --> H
        H --> I[ssos.xdf<br/>最終ディスクイメージ]
    end

    subgraph "クロスコンパイル環境"
        J[m68k-xelf-gcc] --> K[Cコードコンパイル]
        L[アセンブラ] --> M[スタートアップコード]
        N[リンカ] --> O[実行可能ファイル]
    end
```

## コアコンポーネント詳細

### 1. ブートシステム (`ssos/boot/`)

```mermaid
graph TD
    A[電源ON] --> B[IPL読み込み]
    B --> C[main.s実行]
    C --> D[メモリ初期化]
    D --> E[OSイメージ読み込み]
    E --> F[ssosmain()呼び出し]
```

**主要ファイル:**
- `boot/main.s`: アセンブリスタートアップコード
- `boot/linker.ld`: ブートローダー用リンカスクリプト

### 2. カーネルコア (`ssos/os/kernel/`)

**主要コンポーネント:**

#### 割り込み管理 (`interrupts.s`)
```c
// タイマー割り込みハンドラ
timer_interrupt_handler()
├── コンテキスト保存
├── タイマーカウンター更新
├── スケジューリング判定
└── コンテキスト復元
```

#### タスク管理 (`task_manager.c`)
- **最大タスク数**: 16
- **優先度レベル**: 16段階
- **タスク状態**: READY, WAIT, DORMANT, NONEXIST
- **スケジューリング**: 優先度ベースプリエンプティブ

#### メモリ管理 (`memory.c`)
```c
// メモリ割り当てシステム
typedef struct {
    int num_free_blocks;
    SsMemFreeBlock free_blocks[MEM_FREE_BLOCKS];
} SsMemMgr;

ss_mem_alloc4k(uint32_t sz)  // 4KBアライメント
ss_mem_free4k(uint32_t addr, uint32_t sz)
```

### 3. ウィンドウシステム (`ssos/os/window/`)

**レイヤーシステム:**
```c
typedef struct {
    uint16_t x, y, z;      // 位置とZオーダー
    uint16_t w, h;         // サイズ
    uint16_t attr;         // 属性（可視性等）
    uint8_t* vram;         // VRAMバッファ
    uint16_t dirty_x, dirty_y, dirty_w, dirty_h;  // ダーティ領域
    uint8_t needs_redraw;  // 再描画フラグ
} Layer;
```

**最適化機能:**
- DMA転送による高速描画
- ダーティ矩形追跡
- 部分更新レンダリング
- 垂直同期同期

### 4. ユーティリティ (`ssos/os/util/`)

**printfシステム:**
- 標準Cライブラリ互換出力関数
- VRAM直接書き込み
- フォーマット済み文字列表示

## ハードウェアインターフェース

### MFP (Multi Function Peripheral)
```c
#define MFP_ADDRESS 0xe88001
#define VSYNC_BIT 0x10

// タイマー管理
volatile uint32_t ss_timera_counter;
volatile uint32_t ss_timerb_counter;
volatile uint32_t ss_timerc_counter;
volatile uint32_t ss_timerd_counter;
```

### VRAMアクセス
```c
// 直接VRAM操作
ss_clear_vram_fast()
ss_wait_for_clear_vram_completion()

// 解像度: 768x512, 16色
#define VRAMWIDTH 768
#define VRAMHEIGHT 512
```

### キーボードインターフェース
```c
// キーバッファシステム
struct KeyBuffer {
    int data[KEY_BUFFER_SIZE];
    int idxr, idxw, len;
};

ss_kb_read()     // キーコード読み取り
ss_kb_is_empty() // バッファ状態チェック
```

## デュアル実行モード

### ディスクブートモード
```mermaid
graph TD
    A[X68000電源ON] --> B[IPL実行]
    B --> C[ブートセクタ読み込み]
    C --> D[OSイメージ展開]
    D --> E[完全制御取得]
    E --> F[SSOS起動]
```

### ローカル実行モード
```mermaid
graph TD
    A[Human68K環境] --> B[SSOS.X実行]
    B --> C[メモリ割り当て要求]
    C --> D[OS初期化]
    D --> E[Human68K内実行]
    E --> F[ESCで終了]
```

## 性能最適化

### 1. メモリ管理最適化
- 4KBブロック単位割り当て
- メモリプール管理
- 断片化防止

### 2. グラフィック最適化
- DMA転送による高速描画
- ダーティ領域追跡
- 垂直同期同期
- 部分更新レンダリング

### 3. タスク管理最適化
- 固定優先度スケジューリング
- コンテキストスイッチ最適化
- タイマー割り込み効率化

## 開発環境

### クロスコンパイル環境
```bash
# ビルドツールチェーン
export XELF_BASE=/path/to/your/cloned/elf2x68k/m68k-xelf
export PATH=$XELF_BASE/bin:$PATH

# 必須パッケージ
brew install texinfo gmp mpfr libmpc
```

### ビルドプロセス
```mermaid
graph LR
    A[ソースコード] --> B[m68k-xelf-gcc]
    B --> C[アセンブリ] --> D[リンカ]
    D --> E[実行可能ファイル] --> F[makedisk]
    F --> G[ディスクイメージ]
```

## まとめ

SSOSは教育目的とレトロコンピューティング愛好家向けに設計された、シンプルながら機能的なオペレーティングシステムです。以下にその主要な設計理念を示します：

1. **シンプルさ**: 最小限の機能で明確なアーキテクチャ
2. **教育性**: すべてのコンポーネントが理解しやすい構造
3. **性能**: 限られたハードウェアリソースを最大限活用
4. **拡張性**: モジュール化された設計による機能追加の容易さ

このアーキテクチャドキュメントは、SSOSの全体像を理解し、さらなる開発や学習の参考として活用できます。