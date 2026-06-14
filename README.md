# SSOS for X68000

**SSOS** is a simple operating system for the X68000 computer (Motorola 68000 processor), featuring cooperative and preemptive multitasking, graphics management, and a window system with mouse support.

## Prerequisites

- Set up and build a cross compile toolset from <https://github.com/yunkya2/elf2x68k>
- Make the following changes to compile elf2x68k before `make all` on macOS 15 Sequoia

```sh
brew install texinfo gmp mpfr libmpc
```

- modify `scripts/binutils.sh` to include Homebrew library paths:

```bash
--with-gmp=/opt/homebrew/Cellar/gmp/6.3.0 \
--with-mpfr=/opt/homebrew/Cellar/mpfr/4.2.1 \
--with-mpc=/opt/homebrew/Cellar/libmpc/1.3.1
```

- See <https://github.com/sokoide/x68k-cross-compile> for detailed setup instructions

## Build

### Environment Setup

```bash
export XELF_BASE=/path/to/your/cloned/elf2x68k/m68k-xelf
export PATH=$XELF_BASE/bin:$PATH

# Source the environment (recommended)
. ~/.elf2x68k
```

### Standalone Development Build (Recommended for Testing)

For faster development iteration, build as a standalone Human68K executable:

```bash
cd ssos-cooperative    # or ssos-preemptive
make clean             # Required when switching between targets
make standalone
# Output: ~/tmp/ssos_cop.x or ~/tmp/ssos_prm.x
```

### OS Build (Bootable Disk)

```bash
cd ssos-cooperative    # or ssos-preemptive
make clean             # Required when switching between targets
make
# Output: ~/tmp/ssos.xdf (bootable disk image)
```

Boot from the generated XDF file in your X68000 emulator:

![ssos](./docs/ssos.png)

### Additional Build Commands

```bash
make compiledb      # Generate compile_commands.json for LSP support
make dump           # Disassemble the binary for debugging
make readelf        # Show ELF file information
make clean          # Clean all build artifacts
```

## Architecture Overview

### Project Structure

```
ssos-68k/
├── ssos-cooperative/          # Cooperative multitasking version
│   ├── boot/                  # Assembly-based boot sector
│   ├── os/
│   │   ├── kernel/            # Core OS: interrupts.s, scheduler.c, kernel.c, task_manager.c
│   │   ├── app/               # Application logic
│   │   ├── gfx/               # Graphics primitives
│   │   ├── mem/               # Memory management
│   │   └── window/            # Window layering and dirty region system
│   └── standalone/            # Standalone build (Human68K .x executable)
├── ssos-preemptive/           # Preemptive multitasking version
│   └── (same structure)
├── ssos/                      # Legacy unified build (deprecated)
└── docs/                      # Documentation and references
```

### Key Components

| Component | Description |
|-----------|-------------|
| **interrupts.s** | MFP initialization, Timer D (200Hz tick) ISR, V-DISP (60Hz vsync) ISR |
| **scheduler.c** | Cooperative: explicit yield / Preemptive: timer-based context switch |
| **task_manager.c** | Task creation, state management, sleep/wakeup |
| **kernel.c** | Kernel main loop, V-sync handling, key input management |
| **memory.c** | Custom allocator with 4KB page alignment |
| **layer.c** | Window layering with dirty region optimization |

### Two Multitasking Models

- **Cooperative** (`ssos-cooperative/`): Tasks yield explicitly via `ss_task_yield()`. ISR only increments counters and sets flags — all wakeup processing happens in the main loop.
- **Preemptive** (`ssos-preemptive/`): Timer D ISR performs context switching. Full register save/restore on each switch.

### Standalone vs OS Mode

- **Standalone Mode**: Compiles as Human68K executable (`.x`) with `LOCAL_MODE` define. Shares most kernel code but skips low-level hardware initialization. Faster for development iteration.
- **OS Mode**: Full bootable system with custom boot loader. Requires `make clean` when switching from standalone mode.

## MFP Interrupt Configuration

### Current Configuration

| Register | Address | Value | Effect |
|----------|---------|-------|--------|
| IERA | `$E88007` | `$FF` | All sources enabled (IOCS compatibility) |
| IERB | `$E88009` | `$7F` | All sources enabled (IOCS compatibility) |
| IMRA | `$E88013` | `$FF` | All sources unmasked (Human68K compatibility) |
| IMRB | `$E88015` | `$7F` | All sources unmasked except bit 7 |
| VR | `$E88017` | `$41` | Auto-EOI, vector base 0x40 |
| TACR | `$E88019` | `$08` | Event count mode (Human68k compatible) |
| TCDCR | `$E8801D` | `$xx7` | Timer D prescaler /200 |
| TDDR | `$E88025` | `100` | Timer D: 4MHz / 200 / 100 = 200Hz (5ms) |
| Vector 0x110 | — | `ss_timerd_handler` | Timer D ISR |
| Vector 0x134 | — | `ss_vdisp_handler` | V-DISP / Timer A ISR |

### Design Rationale

- **IER stays `$FF/$7F`**: IOCS functions require corresponding IER bits set. Changing IER breaks keyboard USART and timer baud rate generators.
- **IMR all-unmasked (`$FF/$7F`)**: Human68K's keyboard, mouse, and CRTC ISRs must remain active. Our code overrides only vectors 0x134 (V-DISP) and 0x110 (Timer D); all other vectors still point to Human68K handlers. Masking IMR bits breaks IOCS keyboard/mouse input.
- **`ss_set_interrupts()` must be called AFTER all IOCS initialization** (CRTMOD, MS_INIT, etc.) because IOCS calls reprogram the MFP.

### ISR Design

Both Timer D and V-DISP ISRs:
1. Disable interrupts (`move.w #0x2700, %sr`) at entry
2. Save minimal registers (`d0/a0`)
3. Increment counter + set flag (no C function calls in ISR)
4. Clear ISR-in-service bit
5. Re-enable interrupts (`move.w #0x2000, %sr`)
6. `rte`

## Debugging

### MFP Register Tracking

Both versions include `mfp_debug.c` for tracking MFP register changes around IOCS calls:

- `ss_dump_mfp_regs()`: Snapshot 15 MFP registers
- `ss_diff_mfp_regs()`: Compare two snapshots, format differences
- `ss_mfp_track_begin()` / `ss_mfp_track_end()`: Wrapper for IOCS calls, logs changes to text VRAM
- Keyboard window line[2] displays Timer D-related registers (IMRA, IMRB, ISRB, TCDCR, TDDR) in real time

### Exception Handler

A TRAP #14 handler catches illegal instructions and displays PC, SR, and the offending opcode on screen — useful for detecting stack corruption from interrupt nesting.

## Findings

### Timer D ISR Analysis (Session 21)

**Finding**: The Timer D ISR implementation is correct and not the cause of the freeze.

| Metric | Value |
|--------|-------|
| Instructions | 11 |
| Cycles | 90 (10MHz: 9.0μs, 16MHz: 5.6μs) |
| Stack usage | 14 bytes (8 regs + 6 exception frame) |
| Nesting prevention | ✅ `move.w #0x2700, %sr` at entry |
| V-DISP interference | ✅ Max 9.2μs delay (0.18% of 5ms period) |

### MFP Initialization Code Verification (Session 21)

**Finding**: MFP register values set by `ss_set_interrupts()` are correct. The initialization order in standalone mode is correct — `ss_set_interrupts()` is called after all IOCS calls.

**However**: Runtime IOCS calls (`_iocs_b_keyinp()`, `_iocs_ms_getdt()`, `_iocs_ms_curgt()`) may reprogram MFP registers, causing Timer D to stop after ~82 ticks.

### Cooperative ssos_cop.x Freeze Issue (Resolved)

**Symptom**: Timer stopped at ~82–109 ticks, keyboard/mouse unresponsive, screen corrupted after ~20–30 seconds.

**Root Cause (Two Bugs)**:

1. **Register save insufficient in `ss_task_yield`**: The cooperative yield only saved `d0/a0` (2 registers) instead of `d0-d7/a0-a6` (16 registers). On context switch, callee-saved registers (`d2-d7/a2-a6`) were clobbered by the other task, causing the C compiler's register assumptions to break → corrupted pointers → freeze and eventual VRAM corruption.

2. **IMR over-masked**: `IMRA=$21, IMRB=$10` masked all interrupts except Timer A and Timer D. Human68K's keyboard and mouse ISRs were blocked, so IOCS functions (`_iocs_b_keysns()`, `_iocs_ms_getdt()`) returned no data.

**Fix**:
- Restored full register save/restore (`d0-d7/a0-a6`) in `ss_task_yield`, yield-resume, and `resume_interrupted` paths
- Restored `IMRA=$FF, IMRB=$7F` (all unmasked) so Human68K ISRs remain active

### MFP IMR Configuration History

| Session | Change | Effect |
|---------|--------|--------|
| Pre-s12 | IMR=$FF/$7F | All masked → Timer D never fires → data thread stuck |
| s12 | IMR=IERA=$38/$3C | Unmasked unhandled sources → crash |
| s14 | IMR=IERA=$30/$10 | Timer B unmasked, vector uninitialized → crash |
| s16 | IMR=$20/$10, IER=$FF/$7F | Only Timer A + D unmasked. IER preserved. ✅ |
| s20 | TACR 0xC8→0x08 | Reverted erroneous change. Event count mode. ✅ |
| s20 | ISR: added `move.w #0x2700, %sr` | Prevent interrupt nesting → stack corruption. ✅ |
| s20 | `.resume_interrupted`: removed triple SR restore | `rte` pops SR automatically. Redundant code removed. ✅ |
| s23 | `ss_task_yield`: `d0/a0` → `d0-d7/a0-a6` | Cooperative yield must save ALL regs (context switch clobbers callee-saved). ✅ |
| s23 | IMR `$21/$10` → `$FF/$7F` | All Human68K ISRs must remain active for keyboard/mouse. ✅ |

### Cooperative vs Preemptive ISR Differences

| Aspect | Cooperative | Preemptive |
|--------|-------------|------------|
| Timer D ISR | Counter++ + flag, no context switch | Counter++ + context switch |
| Register save | `d0-d7/a0-a6` (full save in yield) | `d0-d7/a0-a6` (full switch) |
| Wakeup | Main loop polls `ss_wakeups_needed` flag | ISR triggers context switch directly |
| Stack | Current task stack only | Each task has own stack |

### Baremetal (`.xdf`) Mouse/Window Input — RESOLVED (s26)

**Affected build**: ベアメタルブート（`ssos_cop.xdf`、IPL-ROM ブート）。standalone（`.x`）は**影響なし**。

**Symptom**: 背景＋空ウィンドウ2つが表示され、マウス・キーボードに反応しない（ように見えた）。

#### 根本原因

**マウス入力チェーンは最初から動作していた。問題は「カーソルが見えない」こと。**

`premain.c` がビデオコントローラ `0xE82600=$003E` で**テキストレイヤー(bit0)をOFF**にしている。IPL-ROM のマウスカーソルはテキスト画面プレーン2/3に描かれるため、テキストOFF下では不可視。`_MS_CURGT` の座標は内部的に更新されているが、画面に描画されないため「入力不能」に見えた。

#### s26 計測で判明した事実（実機確認）

| 計測値 | 値 | 意味 |
|--------|-----|------|
| `V0`=`V2` | `$3F003039` | SCC ベクタ 0x140/0x148 は **RAM上の有効ハンドラ**を指す（0/FFFFFFFF ではない → ハンドラ登録済み） |
| `SR` | `$2004` | Supervisor + **IPL0**（割込全許可）+ Z旗標。割込マスクなし |
| `MX` | 同一起動中に変化 | `_MS_GETDT` がマウス移動量を取得（s24では常に0） |
| `CG` | `(188,254)→(476,360)` 等に変化 | `_MS_CURGT` 絶対座標が更新されている |
| `VS/VD/TD/LB` | 増加中 | Timer D/V-DISP 発火、メインループ正常回転 |

SCC受信 → 割込(0x148) → IPL-ROMハンドラ($3F003039) → 座標更新 → `_MS_CURGT`/`_MS_GETDT` 返値更新、の全チェーンが機能。s25 の `boot.s`/`premain.c` で `_MS_INIT`/`_MS_CURON` を呼んだことで SCC 受信が有効化されていた。

#### 修正（`ssos-cooperative/os/app/main.c`, `os/win/window.c`）

1. **マウス座標**: `_MS_GETDT`Δ累積 → `_MS_CURGT`絶対座標（standalone互換、同一ソース）
2. **ソフトウェアカーソル**: GVRAM に XOR 枠線描画（テキストレイヤー非依存）。XOR は自分自身で相殺するため保存バッファ不要
3. **ドラッグ枠線方式**: ドラッグ中は XOR 枠線のみ（ウィンドウ本体不動）、ドロップ時のみ `ss_win_render_all()`。ちらつかない（standalone 準拠）
4. **z 一意化**: `next_z=3` 開始、255→3 折り返し。複数ウィンドウが同 z=255 にならない（w1消失バグ解消）
5. **`rebuild_zmap` 修正**: max z 化 + `memset(zmap,0)`（旧 `0xFF` 初期値だと max 更新 `win->z>255` が常に false → 全ウィンドウ occluded → 非表示）

#### standalone 無影響の根拠

standalone ビルドは `obj/interrupts.o main.o scheduler.o buddy.o slab.o vram.o` のみリンク（`window.o` なし）。`standalone/main.c` は `win.h` 未参照・独自 `Win`/`draw_frame`/`ol_save`/`draw_march_outline` 実装を持つ。よって `window.c`/`win.h`/`app/main.c` の変更は standalone に無影響。

### IOCS Mouse Routine Numbers (reference)

| Number | Routine | Function |
|--------|---------|----------|
| `$70` | `_MS_INIT` | Initialize mouse |
| `$71` | `_MS_CURON` | Show cursor |
| `$72` | `_MS_CUROF` | Hide cursor |
| `$73` | `_MS_STAT` | Cursor visibility (-1=shown, 0=hidden) |
| `$74` | `_MS_GETDT` | Deltas: bit24-31=Δx, bit16-23=Δy, bit8-15=left, bit0-7=right |
| `$75` | `_MS_CURGT` | Position (X<<16 \| Y) |
| `$76` | `_MS_CURST` | Set position |
| `$77` | `_MS_LIMIT` | Set limits |
