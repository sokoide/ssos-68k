# SSOS for X68000

**SSOS** は X68000（MC68000 プロセッサ）向けの小型オペレーティングシステムである。協調的マルチタスク（`ssos-cooperative/`）とプリエンプティブマルチタスク（`ssos-preemptive/`）の 2 つのモデルを比較実装し、グラフィックス管理、マウス対応ウィンドウシステム、メモリ管理を備える。Human68K 上で動作するスタンドアロン版（`.x`）と、XDF ディスクイメージから直接 IPL 起動するベアメタル版（`.xdf`）の 2 形態でビルド可能。

## 目次

- [前提条件](#前提条件)
- [ビルド](#ビルド)
- [メモリマップ](#メモリマップ)
- [ブートフロー](#ブートフロー)
- [アーキテクチャ概要](#アーキテクチャ概要)
- [モジュール依存](#モジュール依存)
- [2 つのマルチタスクモデル](#2-つのマルチタスクモデル)
- [スタンドアロン vs OS モード](#スタンドアロン-vs-os-モード)
- [MFP 割り込み設定](#mfp-割り込み設定)
- [デバッグ](#デバッグ)
- [知見](#知見)
- [テスト](#テスト)
- [参考](#参考)

## 前提条件

- <https://github.com/yunkya2/elf2x68k> からクロスコンパイルツールセットをセットアップ・ビルドする
- macOS 15 Sequoia で `make all` する前に、以下の変更を加えて elf2x68k をコンパイルする

```sh
brew install texinfo gmp mpfr libmpc
```

- `scripts/binutils.sh` に Homebrew のライブラリパスを追記:

```bash
--with-gmp=/opt/homebrew/Cellar/gmp/6.3.0 \
--with-mpfr=/opt/homebrew/Cellar/mpfr/4.2.1 \
--with-mpc=/opt/homebrew/Cellar/libmpc/1.3.1
```

- 詳細なセットアップ手順は <https://github.com/sokoide/x68k-cross-compile> を参照

## ビルド

### 環境設定

```bash
export XELF_BASE=/path/to/your/cloned/elf2x68k/m68k-xelf
export PATH=$XELF_BASE/bin:$PATH

# 環境ファイルを読み込む（推奨）
. ~/.elf2x68k
```

### スタンドアロン開発ビルド（テスト推奨）

高速な開発イテレーションのため、Human68K 実行形式としてビルド:

```bash
cd ssos-cooperative    # または ssos-preemptive
make clean             # ターゲット切り替え時に必須
make standalone
# 出力: ~/tmp/ssos_cop.x または ~/tmp/ssos_prm.x
```

### OS ビルド（起動可能ディスク）

```bash
cd ssos-cooperative    # または ssos-preemptive
make clean             # ターゲット切り替え時に必須
make
# 出力: ~/tmp/ssos_cop.xdf または ~/tmp/ssos_prm.xdf（起動可能ディスクイメージ）
```

ルート Makefile から両バリアントを一括ビルドすることも可能:

```bash
# リポジトリルートで
make all
# 成果物: ssos-cooperative/ssos_cop.xdf, ssos-preemptive/ssos_prm.xdf
```

生成された XDF ファイルを X68000 エミュレータで起動:

![ssos](./docs/ssos.png)

### その他のビルドコマンド

```bash
# 各 variant ディレクトリで
make compiledb      # LSP 対応の compile_commands.json を生成
make dump           # バイナリの逆アセンブル出力（デバッグ用）
make readelf        # ELF ファイル情報を表示
make clean          # ビルド成果物を全て削除

# リポジトリルートで
make format         # markdownlint / textlint による自動整形
```

## メモリマップ

SSOS の OS 版（`.xdf`）は X68000 の物理アドレス空間に以下を配置する。

| 領域         | 範囲            | サイズ  | 用途                                                            |
| :---         | :---            | :---    | :---                                                            |
| ブートセクタ | `0x00002000` 〜 | 1KB     | IPL から `boot.X` 1KB をロード（`boot/main.s`）                 |
| OS イメージ  | `0x00010000` 〜 | 192KB   | `boot/main.s` がセクタ 2-207 をロード、`.text`+`.rodata` 配置先 |
| RAM          | `0x00040000` 〜 | 80KB    | `.data`+`.bss` 領域。SS_MAX_WINDOWS=32 対応で 64KB→80KB に拡張 |
| SSOSRAM      | `0x00150000` 〜 | 10944KB | OS ヒープ（`ss_mem_init` の対象、`__ssosram_size=0x00AB0000`）  |
| Text VRAM    | `0x00E00000` 〜 | 512KB   | テキスト画面（IPL で使用、OS 起動時に 0 クリア）                |
| GVRAM Page 0 | `0x00C00000` 〜 | 512KB   | 16 色モード (CRTMOD 16) のフレームバッファ                      |
| GVRAM Page 1 | `0x00C80000` 〜 | 512KB   | 256 色モード (CRTMOD 8) のダブルバッファ用                      |
| CRTC         | `0x00E80000` 〜 | —       | 画面制御（V-DISP, スクロール等）                                |
| DMAC Ch.2    | `0x00E84080` 〜 | —       | 矩形塗りつぶし高速化（DMA メモリ→VRAM）                         |
| MFP          | `0x00E88000` 〜 | —       | 割り込みコントローラ（タイマ A〜D, USART, GPIO）                |
| Video Ctl    | `0x00E82600`    | 1 word  | 画面レイヤ ON/OFF（bit0=テキスト）                              |

### メモリアロケータ

SSOSRAM 領域は **Buddy + Slab ハイブリッド**アロケータで管理する（`os/mem/`）。

- **Buddy system**（`buddy.c`）: 16B〜64KB（`SS_BUDDY_MIN_ORDER=4`〜`SS_BUDDY_MAX_ORDER=16`）の可変長ブロック。`ss_alloc(size)`/`ss_free(ptr)` で使用
- **Slab cache**（`slab.c`）: 64KB 固定 (`SS_SLAB_SIZE`) の特定サイズ専用キャッシュ。以下の 4 種を事前定義（`memory.h`）

| Slab キャッシュ  | 用途                            |
| :---             | :---                            |
| `ss_slab_task`   | `SSTask` 構造体（タスク管理）   |
| `ss_slab_window` | `SSWindow` 構造体（ウィンドウ） |
| `ss_slab_msg`    | `SSMessage` 構造体（IPC）       |
| `ss_slab_rect`   | 矩形構造体（描画）              |

## ブートフロー

`.xdf` イメージから起動したときの処理順序を示す。

```text
[IPL ROM]
  │ ディスク sector 1（ブートセクタ）を 0x00002000 にロードしてジャンプ
  ▼
[0x002000: boot/main.s:boot]    ──▶ entry に分岐
  │
[0x002000: boot/main.s:entry]
  │ • "Booting Scott & Sandy OS..." 等のバナー表示
  │ • IOCS _B_READ で sector 2-207（最大 207KB）を 0x00100000 にロード
  │ • "Sector 2 copied to 0x010000. Ready to go!" 表示
  │ • IOCS _MS_INIT / _MS_CURON で IPL-ROM マウスハンドラを
  │   IPL レベル 5（ベクタ 0x140-0x15C）に登録
  │ • 0x00100000 にジャンプ（→ os/kernel/entry.s:entry）
  ▼
[0x010000: os/kernel/entry.s:entry]   (RAM 0x010000 に SP 設定)
  │ • SP ← 0x010000
  │ • ss_set_interrupts()        ← MFP 全レジスタ初期化、IMR=$FF/$7F
  │ • premain() にジャンプ
  ▼
[os/kernel/premain.c:premain]
  │ 1. clear_bss()
  │ 2. IOCS CRTMOD(16)             ← 16 色モードへ
  │ 3. CRTC R20.SIZ=1 強制セット   ← IPL ROM の CRTMOD(16) バグ回避
  │ 4. 16 色パレット設定 (IOCS 0x94)
  │ 5. IOCS _B_CUROFF / ファンクションキー非表示 / ソフトキーボード無効
  │ 6. Text VRAM 512KB を 0 クリア
  │ 7. CRTC スクロールレジスタ 0
  │ 8. Text VRAM 追加クリア + Video Ctl 0xE82600=$003E
  │    （テキストレイヤー OFF、IPL-ROM マウスカーソル隠蔽）
  │ 9. ss_set_interrupts()        ← IOCS が書き換えた MFP を再初期化
  │ 10. IOCS _MS_INIT / _MS_CURON   ← クリーンの MFP=$FF/$7F 状態で再登録
  │ 11. AER = $06                  ← GPIP4 を 1→0 エッジ検出に設定
  │ 12. IMRA / IMRB 設定:
  │     • cooperative:  $FF / $7F   (Human68K 互換)
  │     • preemptive:   $21 / $10   (Timer A + Key RXF + Timer D のみ)
  │ 13. ss_gfx_set_mode(CRTMOD_16)  ← SSGfxMode 初期化
  │ 14. ss_init()                    ← モジュール初期化
  │ ▼
[os/app/main.c:ss_init]
  │ • ss_mem_init()                 ← SSOSRAM を Buddy で初期化
  │ • ss_task_stack_base = ss_alloc(SS_MAX_TASKS * SS_TASK_STACK)
  │ • ss_sched_init() / ss_work_init() / ss_ipc_init()
  │ • ss_gfx_init() / ss_win_init()
  │ • ss_run() に fall through
  ▼
[os/app/main.c:ss_run]
  │ • 3 つのウィンドウ（Timer / Keyboard / Mouse）を z=1,2,3 で作成
  │ • render_win コールバック登録
  │ • ss_win_render_all()
  │ • メインループ（wait_vsync → 更新 → yield）
  ▼
[idle]  vsync / mouse / keyboard を待つ
```

### なぜ `ss_set_interrupts()` を 2 回呼ぶか

`entry.s` で 1 回目の `ss_set_interrupts()` を呼ぶが、その後 `premain()` 内の `IOCS CRTMOD(16)` / `_B_CUROFF` / `_MS_INIT` 等の IOCS 呼び出しが **MFP レジスタと割り込みベクタを上書き** してしまう。これにより V-DISP（`0x134`）/ Timer D（`0x110`）の自前ハンドラが消え、`ss_vsync_counter` が進まなくなり `wait_vsync()` でフリーズする。`premain()` 末尾で 2 回目を呼ぶことでクリーンな状態に戻す。

これは `.xdf` 版（OS 版）に固有の問題で、`standalone/` は `ss_set_interrupts()` を 1 回しか呼ばない（IOCS 呼び出しが全て終わった後）。

## アーキテクチャ概要

### プロジェクト構成

```text
ssos-68k/
├── ssos-cooperative/                    # 協調的マルチタスク版
│   ├── boot/                            # ブートセクタ (1KB 制限、IPL→OS ロード)
│   ├── os/                              # OS 本体
│   │   ├── kernel/                      # カーネルコア
│   │   │   ├── entry.s                  #   OS エントリ（SP 設定、ss_set_interrupts 呼び出し）
│   │   │   ├── premain.c                #   C 初期化（IOCS 呼び出し群、再 ss_set_interrupts）
│   │   │   ├── interrupts.s             #   MFP 初期化、ISR、コンテキストスイッチ
│   │   │   ├── scheduler.{c,h}          #   タスクスケジューラ（16 優先度ビットマップ）
│   │   │   ├── work_queue.{c,h}         #   遅延処理キュー
│   │   │   └── linker.ld                #   OS イメージ用リンカスクリプト
│   │   ├── mem/                         # メモリ管理（Buddy + Slab）
│   │   ├── gfx/                         # グラフィックス（VRAM 直接アクセス、DMAC fill）
│   │   ├── ipc/                         # メッセージング（タスク間キュー）
│   │   ├── win/                         # 汎用ウィンドウシステム (SS_MAX_WINDOWS=32)
│   │   └── app/                         # デモアプリ（3 ウィンドウ + ドラッグ）
│   ├── include/                         # iocscall.mac（IOCS マクロ定義）
│   └── standalone/                      # スタンドアロン (.x) ビルド（s30 以降は os/win/window.c を共有）
├── ssos-preemptive/                     # プリエンプティブマルチタスク版
│   └── （同構造。ISR 内でコンテキストスイッチを行う点が異なる）
├── tests/                               # 単体テスト（m68k-xelf-gcc + native runner）
│   ├── unit/                            # test_scheduler.c, test_memory.c, test_layers.c 等
│   ├── framework/                       # テストフレームワーク
│   └── Makefile{,.native}               # クロス / ネイティブ両対応ビルド
├── tools/makedisk/                      # XDF ディスクイメージ作成ツール
├── docs/                                # アーキテクチャ / 品質レポート
├── boot/                                # リポジトリルートのブート関連（あれば）
├── .ai-handoff.md                       # セッション引き継ぎメモ
├── AGENTS.md                            # 開発ガイドライン
└── Makefile                             # ルート Makefile（両 variant 一括ビルド）
```

### 主要コンポーネント

| コンポーネント          | 役割                                                                                                |
| :---                    | :---                                                                                                |
| **kernel/entry.s**      | OS エントリ。SP を 0x010000 に設定し `ss_set_interrupts()` を呼び `premain()` に制御を渡す          |
| **kernel/premain.c**    | C 初期化。IOCS 呼び出し群、`ss_set_interrupts()` 再呼び出し、AER/IMR 設定、`ss_init()` → `ss_run()` |
| **kernel/interrupts.s** | MFP 初期化、Timer D / V-DISP / TRAP #14 ハンドラ、`ss_context_switch` / `ss_task_yield`             |
| **kernel/scheduler.c**  | タスク管理。16 優先度レディーキュー、ラウンドロビン、`ss_task_yield` / `ss_task_sleep`              |
| **kernel/work_queue.c** | 遅延処理。ISR から post してメインループで `ss_work_drain`                                          |
| **mem/buddy.c**         | Buddy system（16B〜64KB、可変長）                                                                   |
| **mem/slab.c**          | Slab cache（64KB 固定、4 種: task/window/msg/rect）                                                 |
| **gfx/vram.c**          | 5x8 フォントデータ、CRTMOD 8/16 切替、DMAC Ch.2 fill                                                |
| **win/window.c**        | ウィンドウ API。z-order、hit-test、`render_all` / `render_region`、8x8 block occlusion map          |
| **ipc/message.c**       | タスク間メッセージ。固定長キュー、ブロッキング受信                                                  |
| **app/main.c**          | デモ。3 ウィンドウ（Timer/Keyboard/Mouse）+ ドラッグ + marching-ants 枠線                           |

### タスク管理 API

`os/kernel/scheduler.h` の主要 API:

```c
typedef struct SSTask SSTask;
struct SSTask {
    void*    context;       /* Saved stack pointer */
    SSTask*  prev, *next;   /* Ready queue linkage */
    void*    stack_base;    /* Top of stack */
    uint32_t stack_size;
    void*    (*entry)(void*);
    uint32_t wait_until;    /* ss_task_sleep 用起床時刻 */
    uint8_t  state;         /* SS_TS_NONE/DORMANT/READY/WAIT */
    uint8_t  pri;           /* 0 = 最高優先度 */
    uint8_t  ctx_level;
    uint8_t  pad;           /* resume_type (yield=1, timer-int=0) */
};

uint16_t ss_task_create(SSTaskInfo* info);  /* DORMANT で作成 */
uint16_t ss_task_start(uint16_t id);        /* READY に遷移 */
void     ss_task_yield(void);               /* 協調版: 自発的コンテキストスイッチ */
uint16_t ss_task_sleep(uint32_t ticks);     /* ss_tick_counter + ticks まで WAIT */
```

優先度 0 が最高、15 が最低。レディーキューは 16 ビットビットマップで高速検索する（`pri_bitmap` の bit 15 が pri 0、bit 0 が pri 15）。

## モジュール依存

各モジュールは明確に分離され、依存方向は **内側（kernel）→ 外側（app）** のみ。

```text
        ┌─────────────────────────────────────┐
        │  app/main.c （デモアプリ）          │
        │  （タイマ/キー/マウスウィンドウ）   │
        └────┬──────────┬──────────┬──────────┘
             │          │          │
             ▼          ▼          ▼
   ┌──────────┐   ┌──────────┐   ┌──────────┐
   │ win/     │   │ gfx/     │   │ ipc/     │
   │ window.c │   │ vram.c   │   │ message.c│
   └────┬─────┘   └────┬─────┘   └────┬─────┘
        │              │              │
        └──────┬───────┴──────┬───────┘
               │              │
               ▼              ▼
         ┌──────────┐  ┌─────────────┐
         │ mem/     │  │ kernel/     │
         │ buddy.c  │  │ interrupts.s│
         │ slab.c   │  │ scheduler.c │
         └──────────┘  │ work_queue.c│
                       │ premain.c   │
                       │ entry.s     │
                       └─────────────┘
```

- **kernel** は MFP 割り込み、`ss_set_interrupts()`、コンテキストスイッチを提供する最も内側の層
- **mem** は `ss_alloc()` / `ss_free()` を提供し、kernel にも依存しない（`__ssosram_start` シンボルのみ必要）
- **gfx** は GVRAM および IOCS グラフィックス API をラップ
- **win** はウィンドウ API。`gfx`、`kernel` に依存
- **ipc** はメッセージキュー。`kernel` の `ss_task_yield` を使用
- **app** はこれらを組み合わせて 3 ウィンドウデモを実装

## 2 つのマルチタスクモデル

両 version は同じ API（`ss_task_create` / `ss_task_start` / `ss_task_yield` / `ss_task_sleep`）を提供するが、内部実装が大きく異なる。

| 観点                     | 協調的 (`ssos-cooperative/`)                                                  | プリエンプティブ (`ssos-preemptive/`)                        |
| :---                     | :---                                                                          | :---                                                         |
| コンテキストスイッチ契機 | タスクが `ss_task_yield()` を呼んだ時                                         | Timer D ISR (5ms 毎、200Hz)                                  |
| レジスタ保存             | yield 時 `d0-d7/a0-a6` 全 16 レジスタ                                         | ISR 内で `d0-d7/a0-a6` 全保存（10 ティックに 1 回）          |
| ISR の重み               | 軽い（カウンタ + flag のみ、6 命令）                                          | 重い（10 ティック毎に全レジスタ保存 + C 関数呼び出し）       |
| `ss_task_yield()` の有無 | 必須（タスクの責任）                                                          | 任意（即座に切り替わるので呼ぶ必要なし）                     |
| 起床処理                 | メインループが `ss_wakeups_needed` フラグを見て `ss_process_wakeups()` を呼ぶ | ISR 内で直接 `ss_do_wakeups()` を呼ぶ（`ss_switch_tick=10`） |
| スタック                 | 全タスクで同じ SP から始める（main の stack を共有）                          | 各タスクに独立したスタック（`ss_task_stack_base` 配下）      |
| 起動ファイル             | `cooperative/os/app/main.c` のみ                                              | `preemptive/os/app/main.c` のみ                              |
| 検証                     | 実機 / SX-Window emulator 両方で動作                                          | 実機未検証（コード上は cooperative と対称）                  |

### 協調的 yield の実装（`interrupts.s: ss_task_yield`）

```text
ss_task_yield:
  pea   .yield_resume      ; 復帰先 PC を積む
  move.w #0x2000, -(sp)    ; 復帰時 SR (IPL=0) を積む
  movem.l d0-d7/a0-a6, -(sp) ; 全レジスタ保存
  ; resume_type = 1 (yielded) を TCB に記録
  move.l ss_curr_task, a1
  move.b #1, 31(a1)
  move.l sp, (a1)          ; SP を TCB に保存
  bsr    ss_do_context_switch ; C のスケジューラを呼ぶ
  bra.w  .resume_task      ; 戻り先タスクのコンテキストに切替
.yield_resume:
  rts
```

C のスケジューラは `ready_queue` から最高優先度タスクを選び、`.resume_task` が新タスクのスタックから `d0-d7/a0-a6` を復元し、手動で `SR` / `PC` を積み直して `rts`（実際には `jmp (%a0)`）で戻る。

### プリエンプティブ ISR の実装（`preemptive/interrupts.s: ss_timerd_handler`）

```asm
ss_timerd_handler:
  movem.l d0/a0, -(sp)        ; 最小保存
  addq.l  #1, ss_tick_counter
  lea     ss_switch_tick, a0
  addq.b  #1, (a0)
  cmpi.b  #10, (a0)            ; 10 ティックに 1 回だけコンテキストスイッチ
  bne.s   .no_switch

  movem.l (sp)+, d0/a0         ; 最小保存を戻す
  movem.l d0-d7/a0-a6, -(sp)   ; 全保存
  move.b  #0, 31(a1)           ; resume_type = 0 (timer-interrupted)
  bra.w   ss_context_switch
```

10 ティック毎（50ms 毎）にコンテキストスイッチすることでオーバーヘッドを抑える。

## スタンドアロン vs OS モード

- **スタンドアロンモード** (`.x`): `LOCAL_MODE` 定義ありで Human68K 実行形式としてコンパイル。**s30 からは** `os/win/window.c` もリンクし、`.xdf` と同じ z-order / hit-test / コンテンツ管理 API を使う。レンダリングループ・marching-ants・watchdog など `.x` 固有の UX 層は維持。開発イテレーションが高速
- **OS モード** (`.xdf`): カスタムブートローダを持つ完全起動可能システム。スタンドアロンモードから切り替える際は `make clean` が必要

### s30 後の統一: ウィンドウ実装の共有

s30 以前は `.x` は独自の `Win` 構造体 / `zmap[3]` / `bring_to_front` / `hit_test` を持っていた。s30 でこれらを削除し、両ビルドが `os/win/window.c` を共有する。z-order / ヒットテスト / コンテンツ管理は単一の `SSWindow` API に統一された。

**統一された API**（`os/win/win.h`）:

| 機能 | API |
|------|-----|
| 作成/破棄 | `ss_win_create` / `ss_win_destroy` / `ss_win_show` / `ss_win_hide` |
| 移動/再描画 | `ss_win_move` / `ss_win_damage` / `ss_win_mark_dirty` / `ss_win_render_all` / `ss_win_render_region` |
| Z-order | `ss_win_set_z` / `ss_win_get_z` / `ss_win_active_z`（最大 z、`render_*` で更新）|
| ヒットテスト | `ss_win_hit_test`（z-order 順、最前面のウィンドウを返す。s30 で z-order ベースに修正）|
| タイトル/コンテンツ | `ss_win_set_title` / `ss_win_set_content_line` / `ss_win_get_ptr` |
| 描画コールバック | `ss_win_set_render(win, render_fn)` でウィンドウ単位のカスタム描画を登録 |

**`.x` のみが残す独自実装**（`os/win/window.c` とは別レイヤ）:

- 256 色パレット（`-8` フラグ）
- `wait_vsync` 5 秒 watchdog + MFP 再初期化
- `save_win_bitmap` / `restore_win_bitmap`（ドラッグ中ウィンドウ退避）
- marching-ants ドラッグ枠線（`draw_march_outline`）
- outline save/restore（`ol_save` / `ol_restore`）
- 独自の `draw_frame`（タイトルストライプ付き）

これらは `os/win/window.c` 内に置くと `.x` の 256 色パレットで色ずれするため、`.x` 側に残している。OS モード側は `os/win/window.c` の `draw_frame`（16 色ハードコード）を使う。

### ウィンドウ Z-Order 設計（両ビルド共通）

| 観点 | 実装 |
|------|------|
| Z-order 保存 | `SSWindow` の `uint16_t z` フィールド |
| アクティブ判定ルール | `self->z == ss_win_active_z`（最大 z）|
| ドラッグ操作 | `ss_win_set_z(hid, next_z++)`（単調増加）|
| ヒットテスト | `ss_win_hit_test`（z-order 順、最前面を返す）|
| レンダリング z 走査 | `ss_win_render_all` / `ss_win_render_region` で z=0..255 をイテレート |
| オクルージョン | `rebuild_zmap` で 8×8 ブロック (= 96×64 ブロック) の max-z を計算 |
| 初期 z 衝突回避 | `next_z=4` 開始、255 到達後 4 折り返し（[s29](#s29-初回ドラッグの-z-衝突解決済み)）|

設計の要点:

1. **ウィンドウごとに `z` を保存**: 各 `SSWindow` が自己記述的になる。外部の順序簿なしでウィンドウの作成/破棄/再作成が可能
2. **z=0..255 の汎用ループ**: 任意のウィンドウ数で動く
3. **ブロックレベル max-z**: 各ウィンドウが z 値を持つことで成立
4. **単調増加 z でドラッグ前面化**: 順序配列のスキャン不要

### `.x` ビルドでリンクされるオブジェクト

`s30 以降` の `ssos-cooperative/standalone/Makefile` の `SRCS` から:

```makefile
SRCS=	main.c \
		../os/kernel/scheduler.c \
		../os/mem/buddy.c \
		../os/mem/slab.c \
		../os/gfx/vram.c \
		../os/win/window.c    # s30 で追加
ASRCS=	../os/kernel/interrupts.s
```

`os/app/main.c` のみ含まれない。`os/win/window.c` / `os/win/win.h` の変更は両ビルドに影響する。

## MFP 割り込み設定

### 現在の設定

| レジスタ     | アドレス  | 値                                       | 効果                                     |
| :---         | :---      | :---                                     | :---                                     |
| IERA         | `$E88007` | `$FF`                                    | 全ソース有効（IOCS 互換性）              |
| IERB         | `$E88009` | `$7F`                                    | 全ソース有効（IOCS 互換性）              |
| IMRA         | `$E88013` | `$FF` (cooperative) / `$21` (preemptive) | マスク設定（下記参照）                   |
| IMRB         | `$E88015` | `$7F` (cooperative) / `$10` (preemptive) | マスク設定（下記参照）                   |
| VR           | `$E88017` | `$41`                                    | Auto-EOI、ベクタベース 0x40              |
| TACR         | `$E88019` | `$08`                                    | イベントカウントモード（Human68k 互換）  |
| TCDCR        | `$E8801D` | `$77`                                    | Timer C/D プリスケーラ /200              |
| TDDR         | `$E88025` | `100`                                    | Timer D: 4MHz / 200 / 100 = 200Hz (5ms)  |
| AER          | `$E88003` | `$06`                                    | GPIP4 を 1→0 エッジ検出（V-DISP 計測用） |
| Vector 0x110 | —         | `ss_timerd_handler`                      | Timer D ISR                              |
| Vector 0x134 | —         | `ss_vdisp_handler`                       | V-DISP / Timer A ISR                     |

### IMR の協調 / プリエンプティブ差

| 設定 | cooperative             | preemptive                     | 理由                                                                                                                                                                  |
| :--- | :---                    | :---                           | :---                                                                                                                                                                  |
| IMRA | `$FF` (全許可)          | `$21` (Timer A + Key RXF のみ) | s23 の教訓。cooperative は Human68K 全 ISR を動作させて IOCS キーボード/マウスを有効化。preemptive は未使用ソース発火で未定義ベクタへ飛ぶのを防ぐため最小セットに絞る |
| IMRB | `$7F` (bit7 除く全許可) | `$10` (Timer D のみ)           | 同上                                                                                                                                                                  |

この不一致は s23〜s26 の歴史的経緯による。**協調版は s23 修正済み（`$FF/$7F`）、プリエンプティブ版は s23 以前の値（`$21/$10`）のまま**で残されている。プリエンプティブ版でキーボード/マウスが応答しない症状が出た場合は `$FF/$7F` への変更が必要。

### 設計根拠

- **IER は `$FF/$7F` のまま**: IOCS 関数は対応する IER ビットがセットされている必要がある。IER を変更するとキーボード USART やタイマのボーレートジェネレータが動作しなくなる
- **cooperative IMR 全マスク解除（`$FF/$7F`）**: Human68K のキーボード、マウス、CRTC の ISR は動作し続ける必要がある。当コードはベクタ 0x134（V-DISP）と 0x110（Timer D）のみ上書きし、その他のベクタはすべて Human68K のハンドラを指したまま。IMR ビットをマスクすると IOCS のキーボード/マウス入力が動作しなくなる
- **preemptive IMR 最小セット（`$21/$10`）**: 逆に、baremetal では Human68K 標準ベクタのいくつかが空（IPL ROM ハンドラ未登録）のため、未使用ソースが発火すると未定義ベクタへ飛んでハングする。Timer A / Timer D / Key RXF のみに絞る
- **`ss_set_interrupts()` は全 IOCS 初期化の後に呼び出すこと**（CRTMOD、MS_INIT 等）。IOCS 呼び出しが MFP を再プログラムするため。`premain()` では **2 回呼ぶ** ことで対応

### ISR 設計

Timer D と V-DISP の両 ISR は:

1. エントリで割り込み禁止 (`move.w #0x2700, %sr`)
2. 最小限のレジスタを保存（協調: `d0/a0`、プリエンプティブ: `d0-d7/a0-a6`）
3. カウンタをインクリメントしフラグを設定（ISR 内で C 関数呼び出しなし、協調版）
4. ISR の in-service ビットをクリア
5. 割り込み再許可 (`move.w #0x2000, %sr`)
6. `rte`

## デバッグ

### MFP レジスタ追跡

`os/kernel/kernel.h` の `SSMfpRegs` 構造体と関連 API で MFP の状態変化を追跡できる:

- `ss_dump_mfp_regs(SSMfpRegs* out)`: 15 個の MFP レジスタをスナップショット
- `ss_diff_mfp_regs(before, after, buf, bufsize)`: 2 つのスナップショットを比較、差分を整形表示
- `ss_mfp_track_begin(snapshot)` / `ss_mfp_track_end(before, label)`: IOCS 呼び出しのラッパー、変更をテキスト VRAM に記録
- `ss_mfp_watch_ctrl(label)`: 監視対象 MFP レジスタの選択

これらは `ssos-cooperative/` のみで提供される（`preemptive/os/kernel/kernel.h` でも extern 宣言はあるが、`stubs.o` で未実装スタブ）。

### 例外ハンドラ（TRAP #14）

`os/kernel/interrupts.s` の `ss_trap14_handler` が Human68K 発行の TRAP #14（バスエラー/アドレスエラー/不当命令）を捕捉する。

1. 例外情報を `ss_trapbuf_flag` / `ss_trapbuf_sr` / `ss_trapbuf_pc` に保存
2. `_ABORTJOB` を発行し、Human68K にプロセス abort ベクタ（0xFFF2 / 0xFFF1）へ飛ばせる
3. `ss_trap14_abort`（同じくベクタに登録）が PC, SR, 例外種別を `_B_PRINT` 経由でテキスト VRAM に出力
4. `_ABORTRST` でプロセス終了

スタック破壊やコンテキストスイッチ失敗の検出に有用。

## 知見

ここでは歴史的に解決したバグと、現在の制限事項を分けて記載する。

### バグ履歴（解決済み）

#### s27-s29: OS モードのアクティブタイトル表示

OS モードのウィンドウタイトルレンダリングは、スタンドアロンと同等の表示になるまで 3 回のイテレーションを経た。詳細は s27-s29 の章末尾にまとめた（[s27](#s27-3-ウィンドウのコンテンツ--アクティブ用ハッシュストライプ解決済み)・[s28](#s28-ドラッグによる非アクティブウィンドウのタイトル固着解決済み)・[s29](#s29-初回ドラッグの-z-衝突解決済み)）。

#### s23: 協調スレッドのレジスタ保存不足と IMR マスク過多

- **症状**: タイマが約 82–109 ティックで停止、キーボード/マウス応答なし、約 20–30 秒後に画面破損
- **根本原因（2 つのバグ）**:
  1. **`ss_task_yield` のレジスタ保存不足**: 協調的 yield は `d0/a0`（2 レジスタ）しか保存せず、`d0-d7/a0-a6`（16 レジスタ）を保存する必要があった。コンテキストスイッチ時に、もう一方のタスクによって callee-saved レジスタ（`d2-d7/a2-a6`）が破壊され、C コンパイラのレジスタ仮定が破綻 → ポインタ破損 → フリーズ、最終的に VRAM 破損
  2. **IMR の過剰マスク**: `IMRA=$21, IMRB=$10` は Timer A と Timer D 以外の全割り込みをマスクしていた。Human68K のキーボード/マウス ISR がブロックされ、IOCS 関数がデータを返さなくなった
- **修正**:
  - `ss_task_yield`、yield-resume、`resume_interrupted` の各パスで全レジスタ保存/復元（`d0-d7/a0-a6`）に修正
  - `IMRA=$FF, IMRB=$7F`（全マスク解除）に戻し、Human68K ISR が動作するよう修正
  - **協調版にのみ反映済み**。プリエンプティブ版は依然として `$21/$10`

#### s20: ISR 関連

| 変更                                      | 効果                                            |
| :---                                      | :---                                            |
| TACR 0xC8→0x08                            | 誤った変更を差し戻し。イベントカウントモード ✅ |
| ISR: `move.w #0x2700, %sr` を追加         | 割り込みネスト防止 → スタック破損防止 ✅        |
| `.resume_interrupted`: 三重 SR 復元を削除 | `rte` が SR を自動的にポップ。重複コード削除 ✅ |

#### s16-s14-s12: 段階的な IMR/IER 調整

| Session | 変更                     | 効果                                               |
| :---    | :---                     | :---                                               |
| Pre-s12 | IMR=$FF/$7F              | 全マスク → Timer D が発火せず → データスレッド停止 |
| s12     | IMR=IERA=$38/$3C         | 未ハンドルのソースがマスク解除 → クラッシュ        |
| s14     | IMR=IERA=$30/$10         | Timer B がマスク解除、ベクタ未初期化 → クラッシュ  |
| s16     | IMR=$20/$10、IER=$FF/$7F | Timer A + D のみマスク解除。IER 維持 ✅            |

### s21: Timer D ISR 分析

**所見**: Timer D ISR の実装は正しく、フリーズの原因ではなかった。

| 指標            | 値                                       |
| :---            | :---                                     |
| 命令数          | 11                                       |
| サイクル数      | 90（10MHz: 9.0μs、16MHz: 5.6μs）         |
| スタック使用量  | 14 バイト（8 レジスタ + 6 例外フレーム） |
| ネスト防止      | ✅ エントリで `move.w #0x2700, %sr`      |
| V-DISP への干渉 | ✅ 最大 9.2μs の遅延（5ms 周期の 0.18%） |

### s21: MFP 初期化コード検証

**所見**: `ss_set_interrupts()` で設定する MFP レジスタ値は正しい。スタンドアローンモードの初期化順序も正しい — `ss_set_interrupts()` は全ての IOCS 呼び出しの後に呼ばれている。

**ただし**: 実行時の IOCS 呼び出し（`_iocs_b_keyinp()`、`_iocs_ms_getdt()`、`_iocs_ms_curgt()`）が MFP レジスタを再プログラムする可能性があり、Timer D が約 82 ティック後に停止することがある。これは OS 版での `premain()` 内 2 回呼びで解決済み。

### s26: ベアメタル (`.xdf`) マウス／ウィンドウ入力

**影響ビルド**: ベアメタルブート（`ssos_cop.xdf`、IPL-ROM ブート）。s26 当時の standalone（`.x`）は独自ウィンドウ実装だったため影響なし。s30（後述）で `.x` も `os/win/window.c` を使うように変更されたため、現状は `window.c` の変更が両ビルドに影響する。

**症状**: 背景＋空ウィンドウ 2 つが表示され、マウス・キーボードに反応しない（ように見えた）。

#### 根本原因

**マウス入力チェーンは最初から動作していた。問題は「カーソルが見えない」こと。**

`premain.c` がビデオコントローラ `0xE82600=$003E` で**テキストレイヤー(bit0)をOFF**にしている。IPL-ROM のマウスカーソルはテキスト画面プレーン 2/3 に描かれるため、テキスト OFF 下では不可視。`_MS_CURGT` の座標は内部的に更新されているが、画面に描画されないため「入力不能」に見えた。

#### s26 計測で判明した事実（実機確認）

| 計測値        | 値                             | 意味                                                                                              |
| :---          | :---                           | :---                                                                                              |
| `V0`=`V2`     | `$3F003039`                    | SCC ベクタ 0x140/0x148 は **RAM上の有効ハンドラ**を指す（0/FFFFFFFF ではない → ハンドラ登録済み） |
| `SR`          | `$2004`                        | Supervisor + **IPL0**（割込全許可）+ Z旗標。割込マスクなし                                        |
| `MX`          | 同一起動中に変化               | `_MS_GETDT` がマウス移動量を取得（s24では常に0）                                                  |
| `CG`          | `(188,254)→(476,360)` 等に変化 | `_MS_CURGT` 絶対座標が更新されている                                                              |
| `VS/VD/TD/LB` | 増加中                         | Timer D/V-DISP 発火、メインループ正常回転                                                         |

SCC 受信 → 割込(0x148) → IPL-ROM ハンドラ($3F003039) → 座標更新 → `_MS_CURGT`/`_MS_GETDT` 返値更新、の全チェーンが機能。s25 の `boot.s`/`premain.c` で `_MS_INIT`/`_MS_CURON` を呼んだことで SCC 受信が有効化されていた。

#### 修正（`ssos-cooperative/os/app/main.c`、`os/win/window.c`）

1. **マウス座標**: `_MS_GETDT`Δ累積 → `_MS_CURGT`絶対座標（standalone 互換、同一ソース）
2. **ソフトウェアカーソル**: GVRAM に XOR 枠線描画（テキストレイヤー非依存）。XOR は自分自身で相殺するため保存バッファ不要
3. **ドラッグ枠線方式**: ドラッグ中は XOR 枠線のみ（ウィンドウ本体不動）、ドロップ時のみ `ss_win_render_all()`。ちらつかない（standalone 準拠）
4. **z 一意化**: `next_z=4` 開始、255→3 折り返し（[s29](#s29-初回ドラッグの-z-衝突解決済み) を参照）
5. **`rebuild_zmap` 修正**: max z 化 + `memset(zmap,0)`（旧 `0xFF` 初期値だと max 更新 `win->z>255` が常に false → 全ウィンドウ occluded → 非表示）

#### standalone 無影響の根拠（s26 当時）

s26 当時の standalone ビルドは `obj/interrupts.o main.o scheduler.o buddy.o slab.o vram.o` のみリンクし、`window.o` を含んでいなかった。`standalone/main.c` は `win.h` 未参照・独自 `Win`/`draw_frame`/`ol_save`/`draw_march_outline` 実装を持っていた。s30（後述）で standalone も `os/win/window.c` をリンクするように変更されたため、現状は `window.c` 変更が両ビルドに影響する。

### s27: 3 ウィンドウのコンテンツ + アクティブ用ハッシュストライプ（解決済み）

- **目標**: OS モードでスタンドアロンと同じ 3 ウィンドウデモ（`Timer` / `Keyboard` / `Mouse`）と、同じグレー+ハッシュストライプのアクティブタイトルを表示する
- **変更箇所** (`os/app/main.c`, `os/win/window.c`, `os/win/win.h`):
  - ウィンドウごとに `WinContent`（タイトル + 3 行 + 前回値）、`render_win` コールバック
  - アクティブ検出: `ss_win_active_z`（表示中の最大 z、`ss_win_render_all` / `ss_win_render_region` で設定）
  - 差分コンテンツ更新: `memcmp` で行ごとに前回値と比較 — 全画面再描画なし
  - Marching-ants ドラッグ枠線: `draw_march_outline`（frame & 7）、下地を `ol_save` / `ol_restore`
  - 領域限定レンダリング: `ss_win_render_region` — ドロップ時は影響領域のみ再描画、全画面ではない
- **スタンドアロン影響なし**: `standalone/main.c` は `os/app/main.c` や `os/win/window.c` をリンクしない

### s28: ドラッグによる非アクティブウィンドウのタイトル固着（解決済み）

- **症状**: あるウィンドウを前面にドラッグした後、別のドラッグで別のウィンドウが新たな最前面になっても、*ドラッグされなかった*他のウィンドウが古いアクティブタイトル（グレー+ハッシュストライプ）のままになる
- **根本原因**: `ss_win_render_region` はドロップされた矩形と重なるウィンドウのみ再描画する。`set_z` が `ss_win_active_z` を上げるとき、アクティブを*失った*ウィンドウは再描画領域の外にあるため、古い `is_fg=true` のピクセルが残る
- **修正**: ドラッグ開始時に、新しい `ss_win_get_z` ゲッター（`win.h` / `window.c`）で従前のアクティブウィンドウの `(x, y, w, h)` を取得。ドラッグ終了時に `ss_win_render_region(new_pos)` の後、`ss_win_render_region(prev_active_pos)` を呼び出してアクティブを失ったウィンドウを再描画
- **ファイル**: `os/app/main.c`、`os/win/win.h`、`os/win/window.c`。スタンドアロン影響なし

### s29: 初回ドラッグの z 衝突（解決済み）

- **症状**: 間欠的に、ドラッグ後に起動時にアクティブだったウィンドウがアクティブ表示のまま残る
- **根本原因**: 修正前の `next_z` が `3` から始まっていたが、これは既存の最前面ウィンドウ（z=3 で作成された `W3`）と同じ値だった。`ss_win_set_z(hid, 3)` により、ドラッグされたウィンドウが `W3` と z=3 を共有することになった。`ss_win_active_z` は `z==3` に一致するため、`is_fg = (z == ss_win_active_z)` が**両方に true** となる — 両方がアクティブ（グレー+ハッシュ）として描画される
- **「間欠的」である理由**: 衝突するのは*最初の*ドラッグのみ（next_z が 3 → 4 に変わった後）。以降のドラッグは z=4, 5, … を生成し、固定の起動時 z=1..3 と衝突しない
- **修正**: `next_z` を `4` に初期化（初期 z の最大値より 1 大きい値）。255 到達後は `4` に折り返し（z=4 はドラッグで生成された値なので、起動時 z=1..3 と衝突しない設計判断）

### s30: `.x` と `.xdf` のウィンドウ実装統一

- **目標**: `.x` (standalone) が持っていた独自ウィンドウ実装を削除し、両ビルドが `os/win/window.c` を共有する
- **変更**:
  - `os/win/{win.h,window.c}` (両 variant): `SSWindow` に `title[20]`, `content[3][30]`, `content_prev[3][30]` フィールドを追加。新 API: `ss_win_set_title`, `ss_win_set_content_line`, `ss_win_get_ptr`
  - `standalone/{Makefile,main.c}` (両 variant): `os/win/window.c` をリンク。`Win` 構造体 / `zmap[3]` / `bring_to_front` / `hit_test` / `win_overlap` / `draw_text_clip` / `pad` を削除し、`ss_win_create` + `ss_win_set_title` + `ss_win_set_content_line` で再構築。`next_z=4` 開始・255 到達後 4 折り返し
  - `os/kernel/linker.ld` (両 variant): RAM 領域 64KB→80KB 拡張（SSWindow 拡張で `.bss` が約 6KB 増加したため）
  - `cooperative/standalone/main.c`: `data_thread` 内の `sprintf(buf[30], "VDisp:%lu Tick:%lu", ...)` のオーバーフロー（最大 33 文字）を `snprintf` に修正
- **副次的に修正した既存バグ**（code review で発見）:
  - `ss_win_hit_test` を z-order ベースに変更（元は `windows[]` 配列順で返却しており、z-order と矛盾していた）
  - preemptive 版の `ss_win_set_content_line` を `ss_disable_interrupts()` / `ss_enable_interrupts()` で割り込み保護（メインスレッドの `memcmp` + 描画中の競合を回避）
- **ビルド**: 4 ターゲット (cooperative OS, preemptive OS, cooperative `.x`, preemptive `.x`) 全て exit 0
- **メモリ**: `.bss` 63.6KB → 68.3KB、RAM 80KB に 11.7KB 余裕
- **`.x` のみが残す独自実装**: 256 色パレット、`wait_vsync` 5 秒 watchdog、bitmap save/restore、marching-ants outline、独自の `draw_frame`（タイトルストライプ付き）。これらは `os/win/window.c` 内に置くと `.x` の 256 色モードで色ずれするため、`.x` 側に残している
- **残課題**: `os/app/main.c` の `WinContent` 配列が `SSWindow.title`/`content` と重複。統合すれば重複コード削減可能（別タスク）

### 現在の制限事項

| 項目                                                                             | 内容                                                                                                                                                                                             |
| :---                                                                             | :---                                                                                                                                                                                             | :--- |
| **プリエンプティブ版 IMR が `$21/$10` のまま**                                   | s23 の協調版修正（`$FF/$7F`）が反映されていない。キーボード/マウスが応答しない症状が出る場合は `$FF/$7F` へ変更が必要                                                                            |
| **プリエンプティブ版に `ss_vdisp_fire_count` / `ss_timerd_fire_count` が未実装** | cooperative のみ。デバッグ用途（[デバッグ > MFP レジスタ追跡](#デバッグ)）が preemptive では使用不可                                                                                             |
| **プリエンプティブ版に `mfp_debug.c` の実装が未提供**                            | `kernel.h` で extern 宣言はあるが、`stubs.o` で未実装。実機検証時に要対応                                                                                                                        |
| **OS 版で非アクティブタイトルが「白」に見えない**（s27 残）                      | コードとパレットはスタンドアロンと同一だが視覚的に差がある。要エミュレータ/実機検証。仮説: (1) IOCS 0x94 経由のパレット設定が baremetal で反映されない、(2) CRTMOD 16 のパレットレジスタ挙動の差 |
| **メモリ確保失敗時のフォールバックなし**                                         | `ss_alloc()` が NULL を返した時、`ss_init()` 内で落ちる。`__ssosram_size` を増やして対応                                                                                                         |
| **Z-order の 255 到達後折り返し**                                                | 長いセッションで `next_z=255 → 4` に戻り、初回ドラッグと類似の z 衝突が起きる可能性（起動時 z=1..3 とは衝突しないが、過去にドラッグしたウィンドウの z と衝突する可能性）。要リファクタリング案: z=255 ではなく hid の最低 z を探し再採番                                              |
| **`os/app/main.c` の `WinContent` が SSWindow と重複**（s30 残）                | s30 で `SSWindow` に `title`/`content` フィールドが追加されたが、`os/app/main.c` は依然として独自の `WinContent win_content[APP_MAX_WINS]` 配列を保持。両者を統合すれば重複コード削減可能。別タスク。 |

### IOCS マウスルーチン番号（参考）

| 番号  | ルーチン    | 機能                                                    |
| :---  | :---        | :---                                                    |
| `$70` | `_MS_INIT`  | マウス初期化                                            |
| `$71` | `_MS_CURON` | カーソル表示                                            |
| `$72` | `_MS_CUROF` | カーソル非表示                                          |
| `$73` | `_MS_STAT`  | カーソル表示状態（-1=表示中、0=非表示）                 |
| `$74` | `_MS_GETDT` | 移動量: bit24-31=Δx, bit16-23=Δy, bit8-15=左, bit0-7=右 |
| `$75` | `_MS_CURGT` | 位置（X<<16 \| Y）                                      |
| `$76` | `_MS_CURST` | 位置設定                                                |
| `$77` | `_MS_LIMIT` | 制限設定                                                |

## テスト

`tests/` 配下にクロス（M68K ターゲット）/ ネイティブ（macOS）両対応の単体テストがある。

```bash
# クロス（M68K ターゲット）
cd tests && make test
# ネイティブ（macOS 上でロジック検証）
cd tests && make -f Makefile.native
```

### テスト構成

| ファイル                        | 対象                                                           |
| :---                            | :---                                                           |
| `tests/unit/test_scheduler.c`   | タスクスケジューラ（16 優先度レディーキュー、`ss_task_yield`） |
| `tests/unit/test_memory.c`      | Buddy + Slab アロケータ                                        |
| `tests/unit/test_layers.c`      | ウィンドウ z-order / occlusion                                 |
| `tests/unit/test_kernel.c`      | カーネル（割り込み、ベクタ）                                   |
| `tests/unit/test_errors.c`      | エラーパス / エッジケース                                      |
| `tests/unit/test_performance.c` | 性能ベンチマーク                                               |
| `tests/unit/test_render_perf.c` | 描画性能                                                       |
| `tests/framework/`              | テストフレームワーク（mocks, runner）                          |

カバレッジは 95% 以上を維持する（`AGENTS.md` 参照）。

## 参考

- `docs/SSOS_ARCHITECTURE_REPORT.md` — 詳細アーキテクチャレポート
- `docs/SSOS_QUALITY_ANALYSIS_REPORT.md` — 品質分析レポート
- `DRAW_IMPROVEMENT_ja.md` — 描画改善の歴史（s21 以前）
- <https://github.com/yunkya2/elf2x68k> — クロスコンパイルツールチェーンバイナリ
- <https://github.com/sokoide/x68k-cross-compile> — ビルド手順詳細

## コミット規約

`<type>: short summary` 形式（`feat:`、`fix:`、`perf:`、`refactor:`、`docs:` 等）。Subject は 72 文字以内。PR には影響サブシステムの説明、ツール実行結果（`make`、`make test`）、エミュレータ/実機スクリーンショットや `dump` 抜粋を添付する。
