ENTRY(main)

MEMORY
{
	ROM (rx)	: ORIGIN = 0x00010000, LENGTH = 1024K
	RAM (rwx)	: ORIGIN = 0x00020000, LENGTH = 1024K
}

SECTIONS
{
	_start = .;

	.text : {
		__text_start = .;
		/* .text.startup section is used for main */
		/* *(.text.startup) */
		*(.text)
		*(.rela.text)
		*(.rodata)
		__text_end = .;
	} > ROM
	__test_size = __text_end - __text_start;

	.data : {
		__data_start = .;
		*(.data)
		__data_end = .;
	} > RAM
	__data_size = __data_end - __data_end;

	.bss : {
		__bss_start = .;
		*(.bss)
		*(COMMON)
		__bss_end = .;
	} > RAM
	__bss_size = __bss_end - __bss_end;
}
