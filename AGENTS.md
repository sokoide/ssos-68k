# Repository Guidelines

## Project Structure & Module Organization

SSOS lives in a single unified tree under `ssos/`. The threading model is the only thing that differs between builds and is selected at build time with `SCHED=cooperative` (explicit `ss_task_yield()`) or `SCHED=preemptive` (Timer D ISR-driven context switch). The shared subsystems — `os/gfx`, `os/mem`, `os/ipc`, `os/win`, and the common kernel headers (`os/kernel/kernel.h`, `scheduler.h`, `work_queue.*`, `entry.s`, `linker.ld`) — are compiled unchanged for both models. Only the threading core differs: `os/kernel/cooperative/{scheduler.c,interrupts.s}` vs `os/kernel/preemptive/{scheduler.c,interrupts.s}`. `os/kernel/premain.c` is shared between both models. `os/app/main.c`, `standalone/main.c`, and `boot/` are shared. Boot loader assets sit under `boot/`, the Human68K standalone launcher is in `standalone/`. Unit tests and the native framework are in `tests/`, repo-level references in `docs/`, utilities in `tools/`. Build artifacts such as `.xdf`, `.elf`, and `.x` files are produced into `~/tmp`; keep that directory out of version control.

## Build, Test, and Development Commands

Configure the toolchain once per shell via `export XELF_BASE=/path/to/elf2x68k/m68k-xelf` and `. ~/.elf2x68k`. Build a variant from the unified tree with `cd ssos && make SCHED=cooperative` or `make SCHED=preemptive` (each produces boot + OS kernel `.xdf` and the standalone `.x`). Use `make SCHED=<variant> standalone` for just the Human68K binary, `make dump` for disassembly, and `make readelf` to inspect ELF metadata. Run `make clean` before switching `SCHED` values.

## Coding Style & Naming Conventions

Code is predominantly C with targeted 68000 assembly. Follow 4-space indentation, no hard tabs, and place braces on the same line as function signatures (`void foo(void) {`). Use `snake_case` for functions, file names, and static symbols, reserving uppercase for macros and configuration constants. Keep the `ss_` prefix for kernel-facing APIs and reserve comments for hardware touchpoints or non-obvious control flow.

## Testing Guidelines

The unit test suite under `tests/` has been removed because it referenced source files that no longer exist (e.g., `os/kernel/memory.c`, `os/kernel/task_manager.c`) and tested APIs that no longer match the current codebase (e.g., `ss_mem_alloc()`, `TaskControlBlock`). Only `tests/framework/ssos_test.h` is kept as a starting point for a future rewrite. Treat current test coverage as 0% (test suite removed — was testing old API, needs rewrite). Verify changes by building both `SCHED=cooperative` and `SCHED=preemptive` targets and running them under an emulator or on real hardware. Restore `make test` only after the suite is rewritten against the current kernel and window APIs.

## Commit & Pull Request Guidelines

Git history follows `<type>: short summary` (e.g., `feat: implement hybrid scanline-based occlusion optimization`). Align with `feat`, `fix`, `perf`, `refactor`, `docs`, or similar scopes, and keep the subject under 72 characters. Pull requests should describe the affected subsystem, link related issues, note tooling outcomes (`make` for both SCHED variants), and include emulator screenshots or `dump` excerpts when altering rendering or boot behavior.

## Toolchain & Configuration Tips

Install `elf2x68k` via `brew install yunkya2/tap/elf2x68k` (includes XC library). Source builds may need `texinfo`, `gmp`, `mpfr`, `libmpc`; see the upstream README for details. Keep your environment file (`~/.elf2x68k`) local and verify that `m68k-xelf-gcc --version` succeeds before invoking make targets.
