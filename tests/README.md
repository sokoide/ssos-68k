# SSOS-68k Tests

Unit tests for SSOS-68k, rebuilt against the current kernel/window API.

Two test families, run from this directory:

| Target          | What it runs                                   | Toolchain                          |
|-----------------|------------------------------------------------|------------------------------------|
| `make test`     | Native C tests (host clang, fast, CI-friendly) | `cc` (Apple clang)                 |
| `make test-asm` | m68k asm samples under QEMU                    | `m68k-elf-as/ld`, `qemu-system-m68k` |
| `make test-qemu`| **SSOS scheduler + real ctx switch on QEMU**   | `m68k-elf-gcc`, `qemu-system-m68k` |

## Quick start

```bash
# Native: runs all C suites for both scheduler variants. Exits non-zero on any failure.
make test

# QEMU asm samples:
make test-asm
# or a single sample:
cd asm && make run S=t01_ctx_save_restore

# QEMU: drive the REAL SSOS cooperative scheduler (with a QEMU port of the
# context switch) and watch two tasks round-robin via movem.l:
make test-qemu
```

## Layout

```
framework/        TEST/RUN_TEST/ASSERT_* macros + HW stubs
  ssos_test.h     test framework (reusable across suites)
  test_runner.c   main(): runs every suite, prints the summary, sets exit code
  test_mocks.c    HW/asm stubs so kernel/window C sources run on the host
unit/
  test_numfmt.c    pure logic — number formatting
  test_mem.c       pure logic — buddy allocator + slab cache
  test_scheduler.c stubbed HW — priority queue, task lifecycle, sleep/wakeup
  test_window.c    stubbed HW — window CRUD, z-order, dirty regions, hit-test
asm/              self-contained m68k samples for QEMU virt (Goldfish TTY)
  t01_hello.s, t02_subroutines.s, t03_ctx_save_restore.s (progressive)
qemu/             SSOS scheduler + ctx switch driven on QEMU (C + asm)
  common/  stub.c, tty.h, linker.ld (shared)
  coop/    ctx_switch.s + t01_single_yield, t02_round_robin, t03_register_save
  pre/     preempt_ctx_switch.s + t01_round_robin, t02_register_save, t03_sleep_wakeup
Makefile.native   native build (SCHED=cooperative|preemptive)
Makefile / Makefile.qemu  top-level routing
```

## QEMU scheduler tests (`qemu/`)

These are the tests the Native suite **cannot** do. The Native `ss_task_yield`
stub drives only the queue rotation — it never swaps register state, so the
real context switch (stack switch + `movem.l`) is untested there. `qemu/` fixes
that, for both threading models. Each test compiles the **unmodified**
`cooperative/scheduler.c` or `preemptive/scheduler.c` with `m68k-elf-gcc` and
links against a QEMU port of the context switch.

- **`coop/`** — cooperative path (`movem.l` save on yield, restore on resume
  via `jmp`). `ctx_switch.s` ports `interrupts.s` (`ss_task_yield` /
  `.resume_task` / `.start_task`) with MFP/Human68K-TRAP dependencies stripped.
  - `t01_single_yield` — one worker yields and returns to main (`TM`)
  - `t02_round_robin` — two workers round-robin (`1212...`)
  - `t03_register_save` — distinct d2-d7 patterns survive each yield
- **`pre/`** — preemptive path (ISR driven by `trap #0`; resume via
  `.resume_interrupted` / `rte`). `preempt_ctx_switch.s` ports
  `ss_timerd_handler` + `.resume_task`. A trap exception frame (SR+PC) is
  byte-identical to a Timer D interrupt frame.
  - `t01_round_robin` — two workers round-robin via `rte`
  - `t02_register_save` — distinct d2-d7 patterns survive each `rte`
  - `t03_sleep_wakeup` — `ss_task_sleep(N)` blocks, ticks advance in the ISR,
    task resumes after N ticks

Scope: `trap` is a **synchronous** exception (the task fires it), so this is
not a true asynchronous hardware preemption — but it validates the ISR-driven
context-switch mechanics, which is the part Native cannot reach. The real
kernel's MFP EOI and Timer-D period setup are out of scope (no MFP on QEMU
virt).

## How native tests work

The kernel (`scheduler.c`) and window (`window.c`) sources are compiled
**unchanged** with the host compiler. Their X68000 HW/asm dependencies are
stubbed in `framework/test_mocks.c`:

| Real dependency                        | Stub in test_mocks.c                       |
|----------------------------------------|--------------------------------------------|
| `ss_disable/enable_interrupts` (asm)   | no-op (tests are single-threaded)          |
| `ss_task_yield` (asm ctx switch)       | calls `ss_do_context_switch()` only — queue rotation, no register swap |
| `ss_tick_counter` (bumped by ISR)      | host-controlled variable (`ADVANCE_TICK`)  |
| `ss_task_stack_base` (from app)        | static 512 KB arena                        |
| `ss_gfx_rect` / `ss_gfx_fill_stipple` (VRAM/DMA) | call counters (assert paint happened) |
| `ss_current_mode`                      | dummy mode (128x128)                       |

The scheduler is built twice via `SCHED=` — `cooperative` and `preemptive`
share one `test_scheduler.c` because their queue/task/wakeup logic is
identical.

## Scope and limitations (read before trusting a green run)

These tests cover **C logic only**. They deliberately do **not** exercise:

- **Graphics / VRAM / DMA / IOCS / MFP** — direct HW access. `ss_gfx_*` are
  stubs; rendering tests only assert that paint calls happened, not pixels.
- **The real context switch** (`interrupts.s`). The `ss_task_yield` stub drives
  the queue rotation but does **not** swap register state, so concurrency is
  not tested. Sleep/wakeup are verified as state transitions, not as actual
  task interleavings.
- **Preemptive ISR preemption path** (Timer D ISR driving the switch). Only the
  shared C logic is tested under both `SCHED=` variants.
- **The boot loader** (`boot/`), `premain.c`, `app/main.c`, `standalone/main.c`.

For those, build both `SCHED=` targets and run them on an emulator or real
hardware (see the top-level README).

The QEMU asm samples under `asm/` are self-contained teaching examples of the
m68k primitives the SSOS context switch is built on (`movem.l` save/restore).
They are **not** the SSOS `interrupts.s` itself — that file is X68000-MFP-
specific and cannot run on QEMU virt.

## Adding a new suite

1. Write `unit/test_<module>.c` using the `TEST(name) { ... }` / `RUN_TEST()`
   / `ASSERT_*` macros. Add `void run_<module>_tests(void);` and call it from
   `framework/test_runner.c`.
2. If the module touches HW, stub the dependency in `framework/test_mocks.c`
   and reset its state in `reset_test_state()`.
3. Add the suite source to `UNIT_SRCS` (and the module source to `OS_SRCS`) in
   `Makefile.native`.
