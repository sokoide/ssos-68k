ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TARGET=BOOT.X
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJCOPY=m68k-xelf-objcopy

ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include

.PHONY: all clean

all: $(TARGET).bin

$(TARGET).bin: $(TARGET)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

$(TARGET): main.o linker.ld
	$(LD) --strip-all main.o -o $(TARGET).elf -Tlinker.ld -e boot

main.o: main.s
	$(AS) --strip-local-absolute $< -o $@ $(ASFLAGS)

clean:
	rm -rf $(TARGET) $(TARGET).bin $(TARGET).elf main.o
