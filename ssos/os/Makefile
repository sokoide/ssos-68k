TARGET=SSOS.X
SRCS= \
	kernel/premain.c \
	kernel/dma.c \
	kernel/kernel.c \
	kernel/memory.c \
	kernel/task_manager.c \
	kernel/vram.c \
	main/ssosmain.c \
	main/ssoswindows.c \
	window/layer.c \
	util/printf.c
ASRCS= \
	kernel/entry.s \
	kernel/interrupts.s

OBJS=$(ASRCS:.s=.o) $(SRCS:.c=.o)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJCOPY=m68k-xelf-objcopy
OBJDUMP=m68k-xelf-objdump
READELF=m68k-xelf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I./util -I./kernel -I./window -I./main
LIB=-L$(XELF_BASE)/m68k-elf/lib -L$(XELF_BASE)/lib/gcc/m68k-elf/13.3.0

CCFLAGS=$(INCLUDE) -g -O2 -Wa,-adhlns="$@.lst"
#CCFLAGS=$(INCLUDE) -g -Wa,-adhlns="$@.lst"
# CCFLAGS=$(INCLUDE) -g -O2 -Wa,-adhlns="$@.lst" -fomit-frame-pointer -fno-stack-protector
LDFLAGS=$(LIB) -lx68kiocs -Tlinker.ld -lnosys -nostdlib -lc -lm -lgcc
# LDFLAGS=$(LIB) -lx68kiocs -Tlinker.ld -lnosys -nostdlib -lgcc
ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include -I./kernel

.PHONY:
	all clean dump readelf compiledb

all: $(TARGET).bin

$(TARGET).bin: $(TARGET)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

$(TARGET): $(OBJS) linker.ld
	$(LD) --strip-all $(OBJS) -o $(TARGET).elf $(LDFLAGS) -e entry
	elf2x68k.py -s -o $(TARGET) $(TARGET).elf

%.o: %.s
	$(AS) --strip-local-absolute $^ -o $@ $(ASFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) $(TARGET).bin $(TARGET).elf $(OBJS)

dump: $(TARGET)
	$(OBJDUMP) -D $(TARGET).elf

readelf: $(TARGET)
	$(READELF) -a $(TARGET).elf

compiledb: $(SRCS)
	compiledb make
