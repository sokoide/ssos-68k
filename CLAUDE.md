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

### Local Development Build
```bash
cd ssos
make clean  # Required when switching between targets  
make local
# Outputs: ~/tmp/local.x (executable for Human68K)
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
- `ssos/os/kernel/kernel.c`: Core kernel functionality and event loop
- `ssos/os/main/ssosmain.c`: Main application initialization and GUI setup
- `ssos/os/window/layer.c`: Window layering and graphics system

### Build System
The project uses recursive Makefiles. The top-level build process:
1. Builds `makedisk` utility
2. Compiles boot loader to `BOOT.X.bin`
3. Compiles OS kernel to `SSOS.X.bin`  
4. Uses `makedisk` to create bootable `.xdf` disk image

### Local vs OS Mode
- **OS Mode**: Full bootable system with custom boot loader
- **Local Mode**: Compiles as Human68K executable with `LOCAL_MODE` define, skips low-level hardware initialization

## Toolchain
Uses m68k-xelf GCC cross-compiler with X68000-specific libraries (`-lx68kiocs`). Assembly files use Motorola syntax with `--register-prefix-optional` for compatibility.