ENTRY(main)

MEMORY
{
	/* 77 tracks x 2 sides x 8 sectors = 1261568 bytes */
	/* -1024 (boot sector) = 1260544 = 0x133c00 = 0x1231 KB*/
	ROM (rx)	: ORIGIN = 0x00010000, LENGTH = 1000K
	/* 0x143c00-0x14FFFF: OS work */
	/* 0x150000-0xBFFFFF: HEAP+Stack work */
	RAM (rwx)	: ORIGIN = 0x0010A000, LENGTH = 100K
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
