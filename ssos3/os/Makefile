ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TARGET=SSOS3.X
SRCS= \
	kernel/premain.c \
	kernel/scheduler.c \
	kernel/work_queue.c \
	mem/buddy.c \
	mem/slab.c \
	gfx/vram.c \
	win/window.c \
	ipc/message.c \
	app/main.c
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

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../include -I./kernel -I./mem -I./app -I./gfx -I./win -I./ipc
LIB=-L$(XELF_BASE)/m68k-elf/lib -L$(shell dirname $(shell m68k-xelf-gcc -print-libgcc-file-name))

CCFLAGS=$(INCLUDE) -g -O2 -Wa,-adhlns="$@.lst"
LDFLAGS=$(LIB) -lx68kiocs -Tkernel/linker.ld -lnosys -nostdlib -lc -lm -lgcc
ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include -I./kernel

.PHONY: all clean dump readelf

all: $(TARGET).bin

$(TARGET).bin: $(TARGET)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

$(TARGET): $(OBJS) kernel/linker.ld
	$(LD) --strip-all $(OBJS) -o $(TARGET).elf $(LDFLAGS) -e entry

%.o: %.s
	$(AS) --strip-local-absolute $^ -o $@ $(ASFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) $(TARGET).bin $(TARGET).elf $(OBJS)
