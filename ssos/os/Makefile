ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

# Threading model: cooperative (explicit yield) or preemptive (Timer D ISR).
SCHED ?= cooperative
ifeq ($(SCHED),cooperative)
else ifeq ($(SCHED),preemptive)
else
$(error Invalid SCHED='$(SCHED)'. Use: make SCHED=cooperative|preemptive)
endif

# Per-variant sources live under kernel/$(SCHED)/; shared kernel code stays at kernel/.
KDIR=kernel/$(SCHED)

TARGET=SSOS.X
SRCS= \
	kernel/premain.c \
	$(KDIR)/scheduler.c \
	kernel/work_queue.c \
	util/numfmt.c \
	mem/buddy.c \
	mem/slab.c \
	gfx/vram.c \
	win/window.c \
	ipc/message.c \
	app/main.c
ASRCS= \
	kernel/entry.s \
	$(KDIR)/interrupts.s

OBJS=$(ASRCS:.s=.o) $(SRCS:.c=.o)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJCOPY=m68k-xelf-objcopy
OBJDUMP=m68k-elf-objdump
READELF=m68k-elf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../include -I./kernel -I./$(KDIR) -I./mem -I./app -I./gfx -I./win -I./ipc -I./util
LIB=-L$(XELF_BASE)/m68k-elf/lib -L$(shell dirname $(shell m68k-xelf-gcc -print-libgcc-file-name))

CCFLAGS=$(INCLUDE) -g -O2 -Wa,-adhlns="$@.lst"
LDFLAGS=$(LIB) -lx68kiocs -Tkernel/linker.ld -nostdlib -lc -lm -lgcc -lnosys
ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include -I./kernel

.PHONY: all clean dump readelf

all: $(TARGET).bin

$(TARGET).bin: $(TARGET)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

$(TARGET): $(OBJS) kernel/linker.ld
	$(LD) --strip-all $(OBJS) -o $(TARGET).elf $(LDFLAGS) -e entry

dump: $(TARGET).elf
	$(OBJDUMP) -d -S -C $(TARGET).elf

readelf: $(TARGET).elf
	$(READELF) -a $(TARGET).elf

%.o: %.s
	$(AS) --strip-local-absolute $^ -o $@ $(ASFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) $(TARGET).bin $(TARGET).elf $(OBJS)
	rm -f kernel/*.o kernel/*.lst kernel/cooperative/*.o kernel/preemptive/*.o
