ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TARGET=BOOT.X
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJCOPY=m68k-xelf-objcopy
OBJDUMP=m68k-elf-objdump
READELF=m68k-elf-readelf

ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include

.PHONY: all clean dump readelf

all: $(TARGET).bin

$(TARGET).bin: $(TARGET)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

$(TARGET): main.o linker.ld
	$(LD) --strip-all main.o -o $(TARGET).elf -Tlinker.ld -e boot

dump: $(TARGET).elf
	$(OBJDUMP) -d $(TARGET).elf

readelf: $(TARGET).elf
	$(READELF) -a $(TARGET).elf

main.o: main.s
	$(AS) --strip-local-absolute $< -o $@ $(ASFLAGS)

clean:
	rm -rf $(TARGET) $(TARGET).bin $(TARGET).elf main.o
