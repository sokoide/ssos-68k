TARGET=SSOS.X
SRCS=kernel/main.c \
	 kernel/crtc.c \
	 kernel/kernel.c \
	 kernel/syscall.c \
	 kernel/vram.c
OBJS=kernel/entry.o $(SRCS:.c=.o)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJCOPY=m68k-xelf-objcopy
OBJDUMP=m68k-xelf-objdump
READELF=m68k-xelf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include
LIB=-L$(XELF_BASE)/m68k-elf/lib -L$(XELF_BASE)/lib/gcc/m68k-elf/13.3.0

CCFLAGS=$(INCLUDE) -g -O2 -Wa,-adhlns="$@.lst"
# CCFLAGS=$(INCLUDE) -O2
LDFLAGS=$(LIB) -lx68kiocs -Tlinker.ld -lnosys -nostdlib -lc -lm -lgcc

.PHONY:
	clean dump readelf compiledb

$(TARGET).bin: $(TARGET)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

$(TARGET): $(OBJS) linker.ld
	# $(LD) --strip-all $(OBJS) -o $(TARGET).elf $(LDFLAGS) -e entry
	$(LD) $(OBJS) -o $(TARGET).elf $(LDFLAGS) -e entry
	elf2x68k.py -s -o $(TARGET) $(TARGET).elf

%.o: %.s
	$(AS) --strip-local-absolute $^ -o $@ -m68000 -I../../../include

%.o: %.c
	$(CC) -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) *.elf *.o *.bin

dump: $(TARGET)
	$(OBJDUMP) -D $(TARGET).elf


readelf: $(TARGET)
	$(READELF) -a $(TARGET).elf

compiledb: $(SRCS)
	compiledb make
