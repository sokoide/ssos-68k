# Repository Guidelines

## Project Structure & Module Organization
SSOS lives in `ssos/`: `boot/` builds the boot image, `os/` contains the kernel (`kernel/`), graphics (`window/`), main UI (`main/`), and shared utilities (`util/`). Public headers reside in `include/`, and `standalone/` builds the Human68K executable. Native regression assets sit in `docs/`, while helper scripts live in `tools/`. Unit and integration tests mirror production code under `ssos/tests/` (cross-target) and `tests/` (native harness powered by the same framework).

## Build, Test, and Development Commands
Export the toolchain before building: `export XELF_BASE=/path/to/m68k-xelf` and `. ~/.elf2x68k`. Run `make` inside `ssos/` to produce the bootable `ssos.xdf` plus the standalone binary in `~/tmp/`. Use `make -C ssos os` or `make -C ssos boot` when iterating on subsystems, and `make compiledb` to refresh `compile_commands.json`. For analysis, `make dump` disassembles binaries and `make readelf` inspects ELF headers. Tests run via `make -C ssos/tests test` for target coverage, or `make -C tests native` for a fast host run (auto-executes `test_runner_native`).

## Coding Style & Naming Conventions
Code is C99 with four-space indentation and brace-on-same-line formatting. Functions and globals use `snake_case` with the `ss_` prefix for shared symbols, while macros stay upper-snake (e.g., `SS_PERF_*`). Group related `#include` entries alphabetically, keep module state `static`, and prefer explicit width types (`uint32_t`, `int16_t`). Align with existing layering: exported APIs in headers, internals in translation units, and feature flags guarded by `#ifdef LOCAL_MODE` or `SS_CONFIG_*` switches.

## Testing Guidelines
Add new suites under `ssos/tests/unit/` and register them via `RUN_TEST` helpers in the relevant `run_*` function. Maintain the >95% coverage norm by pairing features with mocks in `ssos/tests/framework/`. Use `make -C ssos/tests test-verbose` when diagnosing failures, and perform a native smoke-check with `make -C tests native` before submitting. Keep tests deterministic, avoid emulator dependencies, and clean artifacts with `make clean` in each test directory.

## Commit & Pull Request Guidelines
Repository history favors descriptive subject lines such as `Phase 5: Implement comprehensive performance optimization system`; emulate that format or scope-based alternatives (`kernel: fix timer overflow`). Squash noisy commits, document linked issues, and note toolchain or environment changes. Pull requests should summarize behavioral impact, list test commands executed, and attach screenshots or logs when altering UI or performance reporting. Ensure reviewers can reproduce results with the documented Make targets.
