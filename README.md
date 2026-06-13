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
