# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SSOS is a simple operating system for the X68000 computer (Motorola 68000 processor). The project builds both a bootable disk image (.xdf) and a local executable (.x) that runs on Human68K.

## Prerequisites

- Cross-compilation toolchain from https://github.com/yunkya2/elf2x68k
- Environment variables:
  ```bash
  export XELF_BASE=/path/to/your/cloned/elf2x68k/m68k-xelf
  export PATH=$XELF_BASE/bin:$PATH
  ```

### macOS Setup Notes
On macOS 15 Sequoia, install dependencies and modify elf2x68k before building:
```bash
brew install texinfo gmp mpfr libmpc
```
Modify `scripts/binutils.sh` in elf2x68k to include Homebrew library paths:
```bash
--with-gmp=/opt/homebrew/Cellar/gmp/6.3.0 \
--with-mpfr=/opt/homebrew/Cellar/mpfr/4.2.1 \
--with-mpc=/opt/homebrew/Cellar/libmpc/1.3.1
```
See https://github.com/sokoide/x68k-cross-compile for detailed setup instructions.

## Build Commands

**Important:** Before building, source the elf2x68k environment:
```bash
. ~/.elf2x68k
```

### Full OS Build (Bootable Disk)
```bash
cd ssos
make clean  # Required when switching between targets
make
# Outputs: ~/tmp/ssos.xdf (bootable disk image)
```

### Standalone Development Build
```bash
cd ssos
make clean  # Required when switching between targets
make standalone
# Outputs: ~/tmp/standalone.x (executable for Human68K)
```

### Other Commands
```bash
make clean          # Clean all build artifacts
make compiledb      # Generate compile_commands.json for LSP
make dump           # Disassemble the binary
make readelf        # Show ELF file information
```

## Architecture

The OS consists of three main components:

1. **Boot Loader** (`ssos/boot/`): Assembly-based boot sector that loads the OS
2. **OS Kernel** (`ssos/os/`): Core OS functionality including:
   - Memory management (`kernel/memory.c`)
   - Task management (`kernel/task_manager.c`) 
   - Hardware abstraction (`kernel/dma.c`, `kernel/vram.c`)
   - Interrupt handling (`kernel/interrupts.s`)
3. **Applications** (`ssos/os/main/`): Window system and main application logic

### Key Files
- `ssos/os/kernel/entry.s`: OS entry point and low-level initialization
- `ssos/os/kernel/kernel.c`: Core kernel functionality, V-sync handling, and key input management
- `ssos/os/kernel/task_manager.c`: Preemptive multitasking with timer-based context switching
- `ssos/os/main/ssosmain.c`: Main application initialization, graphics mode setup, and memory allocation
- `ssos/os/window/layer.c`: Window layering and graphics system
- `ssos/standalone/main.c`: Standalone mode entry point that bypasses boot loader

### Build System
The project uses recursive Makefiles. The top-level build process:
1. Builds `makedisk` utility
2. Compiles boot loader to `BOOT.X.bin`
3. Compiles OS kernel to `SSOS.X.bin`  
4. Uses `makedisk` to create bootable `.xdf` disk image

### Standalone vs OS Mode
- **OS Mode**: Full bootable system with custom boot loader. Requires `make clean` when switching from standalone mode.
- **Standalone Mode**: Compiles as Human68K executable (`.x` file) with `LOCAL_MODE` define. Shares most kernel code but skips low-level hardware initialization. Faster for development iteration.

## Development Workflow

### Typical Development Cycle
1. Make changes to kernel or application code
2. For quick testing: `cd ssos && make clean && make standalone` → test `~/tmp/standalone.x` on Human68K
3. For full system testing: `cd ssos && make clean && make` → boot from `~/tmp/ssos.xdf`
4. Use `make compiledb` to generate LSP support for code editors

### Key Architectural Patterns
- **Task Management**: Preemptive multitasking with timer interrupts every `CONTEXT_SWITCH_INTERVAL` ticks
- **Memory Management**: Custom allocator with 4KB page alignment for task stacks
- **Graphics**: Direct VRAM manipulation with hardware V-sync synchronization
- **Input Handling**: Interrupt-driven keyboard input with circular buffer (`KEY_BUFFER_SIZE`)

## Toolchain
Uses m68k-xelf GCC cross-compiler with X68000-specific libraries (`-lx68kiocs`). Assembly files use Motorola syntax with `--register-prefix-optional` for compatibility.

### Compiler Flags
- **OS Mode**: Standard compilation with hardware-specific initialization
- **Local Mode**: Adds `-DLOCAL_MODE` define to conditionally compile for Human68K execution