TARGET=	local.x
SRCS=	main.c \
		../os/kernel/kernel.c \
		../os/kernel/dma.c \
		../os/kernel/memory.c \
		../os/kernel/task_manager.c \
		../os/kernel/vram.c \
		../os/main/ssosmain.c \
		../os/main/ssoswindows.c \
		../os/window/layer.c \
		../os/util/printf.c
ASRCS= \
		../os/kernel/interrupts.s

HEADERS=$(wildcard *.h)
OBJS=$(ASRCS:.s=.o) $(SRCS:.c=.o)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJDUMP=m68k-xelf-objdump
OBJDUMP=m68k-xelf-objdump
READELF=m68k-xelf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../os/util -I../os/kernel -I../os/window -I../os/main
LIB=-L$(XELF_BASE)/m68k-elf/lib

CCFLAGS=$(INCLUDE) -O2 -g -DLOCAL_MODE
# LDFLAGS=$(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc -Tlinker.ld
LDFLAGS=$(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc

.PHONY:
	clean install dump compiledb

install: $(TARGET)
	cp $(TARGET) $(HOME)/tmp

$(TARGET): $(OBJS)
	$(CC) $^ -o $(TARGET) -Xlinker "$(LDFLAGS)"

%.o: %.s
	$(AS) --strip-local-absolute $^ -o $@ -m68000 -I../include -I../os/kernel

%.o: %.c
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) *.elf *.o *.lst ../os/kernel/*.o ../os/util/*.o

dump: $(TARGET)
	$(OBJDUMP) -D $(TARGET).elf

readelf: $(TARGET)
	$(READELF) -a $(TARGET).elf

compiledb: $(SRCS)
	compiledb make
