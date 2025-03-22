ENTRY(main)

__ssosram_size = 0x00AB0000;;

MEMORY
{
	/* 128K = 128 sectors */
	/* modify the following line in boot/main.s if you load more */
	/* move.l	#1024*(X), %d3 */
	ROM (rx):		ORIGIN = 0x00010000, LENGTH = 128K
	RAM (rwx):		ORIGIN = 0x00030000, LENGTH = 64K
	/* 0x010000-0x150000: 1280KiB for SSOS .text+.data+.bss */
	SSOSRAM (rwx):	ORIGIN = 0x00150000, LENGTH = 10944K

	/* 77 tracks x 2 sides x 8 sectors = 1261568 bytes */
	/* -1024 (boot sector) = 1260544 = 0x133c00 = 0x1231 KB*/
	/* ROM (rx)	: ORIGIN = 0x00010000, LENGTH = 1000K */
	/* 0x143c00-0x14FFFF: OS work */
	/* 0x150000-0xBFFFFF: HEAP+Stack work */
	/* RAM (rwx)	: ORIGIN = 0x0010A000, LENGTH = 100K */
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
	__text_size = __text_end - __text_start;

	.data : {
		__data_start = .;
		*(.data)
		__data_end = .;
	} > RAM
	__data_size = __data_end - __data_start;

	.bss : {
		__bss_start = .;
		*(.bss)
		*(COMMON)
		__bss_end = .;
	} > RAM
	__bss_size = __bss_end - __bss_start;

	.ssos : {
		__ssosram_start = .;
		. = . + __ssosram_size;
		__ssosram_end = .;
	} > SSOSRAM
}
