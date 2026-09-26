ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TARGET=BOOT.X
SCHED ?= cooperative
BUILD_DIR=build/$(SCHED)
TARGET_ELF=$(BUILD_DIR)/$(TARGET).elf
TARGET_BIN=$(BUILD_DIR)/$(TARGET).bin
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJCOPY=m68k-xelf-objcopy
OBJDUMP=m68k-elf-objdump
READELF=m68k-elf-readelf

ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include

.PHONY: all clean dump readelf

all: $(TARGET_BIN)

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

$(TARGET_ELF): $(BUILD_DIR)/main.o linker.ld
	@mkdir -p $(dir $@)
	$(LD) --strip-all $< -o $@ -Tlinker.ld -e boot

dump: $(TARGET_ELF)
	$(OBJDUMP) -d $(TARGET_ELF)

readelf: $(TARGET_ELF)
	$(READELF) -a $(TARGET_ELF)

$(BUILD_DIR)/main.o: main.s
	@mkdir -p $(dir $@)
	$(AS) --strip-local-absolute $< -o $@ $(ASFLAGS)

clean:
	rm -rf build
	rm -f $(TARGET) $(TARGET).bin $(TARGET).elf main.o
