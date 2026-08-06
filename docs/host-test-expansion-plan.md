# ホスト/QEMU テスト拡張計画

## 目的

X68000 エミュレータでの確認を、実ハードウェアとの接続確認に限定する。
描画結果、ウィンドウ合成、スケジューラ方針、68000 のコンテキスト切替は、
ホストまたは `qemu-system-m68k -M virt` で自動検証する。

MFP 全体を再実装することは目的にしない。SSOS が依存する Timer D のイベント
（tick、割り込み許可/マスク、pending、EOI、一定 tick ごとの切替）を決定論的に
注入する方が、少ないコードでカーネルの不変条件を検証できる。MFP レジスタの
アドレス、バイトアクセス、ベクタ応答そのものは X68000 エミュレータまたは実機に残す。

## テスト層

| 層 | 実行環境 | 検証対象 | 検証しないもの |
|---|---|---|---|
| Native unit/property | host C compiler | scheduler queue、sleep/wakeup、IPC、memory、座標計算 | 68000 register/stack frame |
| Native framebuffer | host C compiler | `vram.c` の画素結果、clip、font、stipple、XOR、page flip | 実 GVRAM wait、DMAC bus cycle |
| Native scene integration | host C compiler | window/scene の最終 framebuffer、dirty region、drag、入力シナリオ | IOCS ABI、実入力デバイス |
| m68k QEMU | `qemu-system-m68k -M virt` | `movem.l`、例外 frame、`rte`、Timer D 相当 tick cadence | MFP MMIO、真の非同期 IRQ |
| X68000 emulator smoke | XM6/px68k 等 | boot、IOCS、MFP/CRTC/VRAM、入力、終了復元 | 詳細なロジック網羅 |
| 実機 | X68000 | DMAC/MFP timing、表示品質、周辺機器 | 通常の回帰テスト |

## 優先順位

### P0: 現在の最大の未検証領域

1. `vram.c` をテスト用 RAM framebuffer に接続する compile-time seam を設ける。
2. 矩形、負座標/画面端 clip、奇数座標、stipple、glyph、region、XOR、flip を
   画素単位で検証する。
3. QEMU preemptive テストに、本番と同じ 10 Timer D ticks ごとの切替を追加する。
   9 ticks では同じ task、10 tick 目で `rte` 経由の切替になることを確認する。
4. `make test` と `make test-qemu` に組み込み、常時回帰テストにする。

### P1: UI と並行処理の統合

1. `scene.c` の IOCS 入力を小さな input provider 境界へ移し、キー/マウス列を注入する。
2. 固定シナリオ（初期描画、content 更新、window drag、ESC）を数 frame 実行し、
   framebuffer hash と注目画素を検証する。
3. `render_region` の対象領域が full render の同領域と一致する differential test を加える。
4. QEMU で複数 sleeper の同時起床、異なる priority、stack canary、32 task stress を加える。
5. Native テストに ASan/UBSan target を加え、ランダム座標と allocator/IPC の境界値を回す。

### P2: ハードウェア境界のモデル化

1. テスト専用 MFP Timer D model を用意し、enable/mask/pending/EOI と分周・reload を検証する。
2. DMAC は success/error/timeout/partial rows の状態遷移だけを fake backend で検証する。
   実転送 timing と bus arbitration はモデル化しない。
3. `palette.c` の IOCS 呼び出しを capture stub に接続し、モード別の色数と値を検証する。
4. boot image parser でロード範囲、entry address、成果物サイズを静的検証する。

## X68000 エミュレータに残す最小 smoke test

- cooperative/preemptive の `.x` と `.xdf` が起動する。
- Timer D fire count が増え、preemptive worker が明示 yield なしで進む。
- 画面 mode、palette、page/scroll、通常 UI、drag、keyboard/mouse、ESC 終了を確認する。
- 割り込み vector と MFP 設定、および終了時の復元を確認する。
- DMAC 利用時は success と CPU fallback の双方を確認する。

この smoke test は画素ロジックや scheduler queue の再検証を行わない。それらは下位層で
網羅し、エミュレータではハードウェア接続だけを見る。

## 完了条件

- `make test` が graphics の画素テストを含む。
- `make test-qemu` が production cadence の Timer D 相当テストを含む。
- 変更した pure C/68000 context-switch 経路は、通常エミュレータ確認対象から外せる。
- `verify-check` は、graphics/window/scene/interrupts を「全面手動」ではなく、
  自動検証済み部分と hardware smoke 必須部分に分けて報告する。

## ハードウェア上の根拠

- MFP、Timer D、割り込み要因、EOI: `x68k-master/resources/04_MFP.md`
- 68000 例外 frame、MFP vector、ISR の `movem`/`rte`: `x68k-master/resources/03_割り込み.md`
- GVRAM の 1 pixel = 1 word、page 配置、CRTC scroll: `x68k-master/resources/07_画面制御.md`
