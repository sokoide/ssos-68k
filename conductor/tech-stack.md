# Technology Stack

## Core Technologies
-   **Target Platform:** Sharp X68000 (Motorola 68000 CPU)
-   **Primary Language:** C (C99/C11 Standard) - Used for Kernel, Drivers, and Applications.
-   **Secondary Language:** Motorola 68000 Assembly - Used for Bootloader (`boot/`), context switching, and low-level HAL.

## Build & Toolchain
-   **Build System:** GNU Make
-   **Compiler:** GCC (Cross-compiler targeting m68k)
-   **Linker/Utils:** GNU Binutils (m68k-elf), `elf2x68k` (ELF to X68k executable converter).
-   **Disk Utility:** Custom `makedisk` tool (C) for generating bootable XDF disk images.

## Development Environment
-   **Host OS:** macOS (Darwin) / Unix-like environment.
-   **Testing:** Native C testing framework (Host-based unit tests) + Emulator integration.
