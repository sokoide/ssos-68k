# SSOS 統合ソースツリー

MC68000/X68000 向けの教育用小型 OS。`SCHED=cooperative|preemptive` で方式を選び、Human68K 上の `.x` と IPL 起動の `.xdf` を同じツリーから生成する。両方式とも task は Supervisor mode・共有アドレス空間で動く。

## ビルド

このディレクトリでツールチェーン環境を読み込んで実行する。

```sh
. ~/.elf2x68k
make SCHED=cooperative standalone
make SCHED=preemptive standalone
make SCHED=cooperative all
make SCHED=preemptive all
```

`standalone` は `.x`、`all` は `.x` と `.xdf` を生成する。公開コピーは既定で `~/tmp`、`SS_EXPORT_DIR` で変更できる。方式・形式・profile ごとに中間成果物を分けるため、設定切替時に clean は不要。

## コードを読む入口

| 責務 | 現行の入口 |
| --- | --- |
| 公開 task API | `os/kernel/task.h`、`status.h` |
| 共有 scheduler 内部 | `os/kernel/scheduler.c`、`scheduler.h` |
| IRQ / clock | `os/kernel/irq.h`、`clock.h` |
| CPU 切替・割り込み | `os/kernel/{cooperative,preemptive}/interrupts.s`、`wakeups.c` |
| TCB/frame ABI（内部） | `os/kernel/scheduler_abi.h`、`context_frame.h` |
| `.xdf` 起動・組立て | `boot/main.s` → `os/kernel/entry.s` → `premain.c` → `os/app/main.c` |
| `.x` 起動・復元 | `standalone/main.c`、`trap14.s`、`os/kernel/runtime_guard.c` |
| 共通 UI demo | `os/app/scene.c`、`scene_replay.c` |
| サブシステム | `os/mem`、`os/ipc`、`os/gfx`、`os/win` |
| 共通 build source 一覧 | `common-sources.mk` |

通常 UI は一つの task が window/GVRAM/DMA を所有する。`.x` は work/IPC をリンクせず、`.xdf` はリンクする。scene の共有と、host 固有の初期化・復元・計測を分ける。

## 設計・学習・検証

- [設計方針と学習順](../docs/educational-os-design.md)
- [教育・保守性の改善計画](../docs/educational-os-plan.md)
- [API、所有権、frame、実機確認の文書索引](../docs/README.md)
- [root README](../README.md): 環境設定と起動・debug の詳細。
- [tests README](../tests/README.md): Native、QEMU、実機の検証範囲。

リポジトリ root の `make test` は host C/RAM、`make test-qemu` は共有 scheduler と QEMU 用 CPU port、`make test-asm` は CPU primitive 教材を実行する。`make verify` は自動検証と build をまとめて実行し、`make verify-check` は変更に必要な実機確認を報告する。QEMU の同期 trap を実 MFP の非同期割り込み検証と同一視しない。

以前の mode10、data thread、marching ants などの説明は[旧 UI ノート](../docs/legacy-ui-notes.md)へ保存した。現行 mode/stride は `os/gfx/vram.c` の `SSGfxMode` 定義を参照する。
