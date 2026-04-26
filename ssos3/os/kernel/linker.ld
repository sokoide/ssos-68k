ENTRY(entry)

__ssosram_size = 0x00AB0000;

MEMORY
{
	ROM (rx):		ORIGIN = 0x00010000, LENGTH = 192K
	RAM (rwx):		ORIGIN = 0x00040000, LENGTH = 64K
	SSOSRAM (rwx):	ORIGIN = 0x00150000, LENGTH = 10944K
}

SECTIONS
{
	_start = .;

	.text : {
		__text_start = .;
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
