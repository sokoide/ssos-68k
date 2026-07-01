/* m68k bare-metal linker script for QEMU virt machine.
 * Same layout as /Users/scott/repo/sokoide/asm/m68k/qemu/linker.ld, except
 * code starts at 0x400 so the 0x0-0x3FF exception-vector table (used by
 * trap/IRQ samples like t05_trap_exception) never overlaps code or data. */
ENTRY(_start)

SECTIONS
{
    . = 0x00000400;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
}
