TARGET=BOOT.X
SRCS=main.s
OBJS=$(SRCS:.s=.o)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJCOPY=m68k-xelf-objcopy
OBJDUMP=m68k-xelf-objdump
READELF=m68k-xelf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include
LIB=-L$(XELF_BASE)/m68k-elf/lib

CCFLAGS=$(INCLUDE) -g -O2
LDFLAGS=$(LIB) -lx68kiocs -nostdlib -Tlinker.ld
ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include

.PHONY:
	all clean dump readelf format

all: $(TARGET).bin

$(TARGET).bin: $(TARGET)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

$(TARGET): $(OBJS) linker.ld
	$(LD) --strip-all $(OBJS) -o $(TARGET).elf $(LDFLAGS) -e boot -q
	elf2x68k.py -s -o $(TARGET) $(TARGET).elf

%.o: %.s
	$(AS) --strip-local-absolute $^ -o $@ $(ASFLAGS)

clean:
	rm -rf $(TARGET) *.elf *.o *.bin

dump: $(TARGET)
	$(OBJDUMP) -D $(TARGET).elf

readelf: $(TARGET)
	$(READELF) -a $(TARGET).elf

format: $(SRCS)
	@echo "pass"