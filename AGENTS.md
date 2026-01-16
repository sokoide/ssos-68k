# Repository Guidelines

## Project Structure & Module Organization

The SSOS sources live in `ssos/`, with `os/` split into `kernel/`, `main/`, `window/`, and `util/` for the core C modules. Boot loader assets sit under `ssos/boot/`, while the Human68K launcher is in `ssos/standalone/`. Unit tests and the native framework are contained in `ssos/tests/`, and repo-level references land in `docs/` with utilities in `tools/`. Build artifacts such as `.xdf`, `.elf`, and `.X` files are produced into `~/tmp`; keep that directory out of version control.

## Build, Test, and Development Commands

Configure the toolchain once per shell via `export XELF_BASE=/path/to/elf2x68k/m68k-xelf` and `. ~/.elf2x68k`. Build the bootable disk with `cd ssos && make`, or generate the Human68K binary with `make standalone`. Use `make compiledb` for `compile_commands.json`, `make dump` for disassembly, and `make readelf` to inspect ELF metadata. Run `make clean` before switching targets.

## Coding Style & Naming Conventions

Code is predominantly C with targeted 68000 assembly. Follow 4-space indentation, no hard tabs, and place braces on the same line as function signatures (`void foo(void) {`). Use `snake_case` for functions, file names, and static symbols, reserving uppercase for macros and configuration constants. Keep the `ss_` prefix for kernel-facing APIs and reserve comments for hardware touchpoints or non-obvious control flow.

## Testing Guidelines

Tests compile with `m68k-xelf-gcc` and rely on `LOCAL_MODE`, so ensure the toolchain is on PATH before running `cd ssos/tests && make test`. The `unit/` directory holds suites such as `test_memory.c` and `test_scheduler.c`; name new suites `test_<module>.c` and register them in the test Makefile. Maintain the existing high coverage (>95%) by adding assertions for corner cases and running `make test` before every push.

## Commit & Pull Request Guidelines

Git history follows `<type>: short summary` (e.g., `feat: implement hybrid scanline-based occlusion optimization`). Align with `feat`, `fix`, `perf`, `refactor`, `docs`, or similar scopes, and keep the subject under 72 characters. Pull requests should describe the affected subsystem, link related issues, note tooling outcomes (`make`, `make test`), and include emulator screenshots or `dump` excerpts when altering rendering or boot behavior.

## Toolchain & Configuration Tips

Install elf2x68k along with Homebrew `texinfo`, `gmp`, `mpfr`, and `libmpc` before first build. If the configure script cannot find the math libraries on macOS, update `scripts/binutils.sh` with the Homebrew cellar paths from the README. Keep your environment file (`~/.elf2x68k`) local and verify that `m68k-xelf-gcc --version` succeeds before invoking make targets.
