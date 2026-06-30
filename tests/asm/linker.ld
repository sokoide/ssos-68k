/* m68k bare-metal linker script for QEMU virt machine.
 * Same layout as /Users/scott/repo/sokoide/asm/m68k/qemu/linker.ld. */
ENTRY(_start)

SECTIONS
{
    . = 0x00000000;

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
