ENTRY(boot)

MEMORY
{
	RAM (rwx) : ORIGIN = 0x00002000, LENGTH = 1K
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
	} > RAM
	__test_size = __text_end - __text_start;

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
}
