TARGET=	local.x
SRCS=	main.c \
		../ssos/os/kernel/main.c \
		../ssos/os/kernel/crtc.c \
		../ssos/os/kernel/kernel.c \
		../ssos/os/kernel/vram.c \
		../ssos/os/util/printf.c
ASRCS= \
		../ssos/os/kernel/interrupts.s

HEADERS=$(wildcard *.h)
OBJS=$(ASRCS:.s=.o) $(SRCS:.c=.o)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJDUMP=m68k-xelf-objdump
OBJDUMP=m68k-xelf-objdump
READELF=m68k-xelf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../ssos/os/util
LIB=-L$(XELF_BASE)/m68k-elf/lib

CCFLAGS=$(INCLUDE) -O2 -g -DLOCAL_MODE
LDFLAGS=$(LIB) -lx68kiocs

.PHONY:
	clean install dump compiledb

install: $(TARGET)
	cp $(TARGET) $(HOME)/tmp

$(TARGET): $(OBJS)
	$(CC) $^ -o $(TARGET) -Xlinker "$(LDFLAGS)"

%.o: %.s
	$(AS) --strip-local-absolute $^ -o $@ -m68000 -I../include

%.o: %.c
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) *.elf *.o *.lst ../ssos/kernel/*.o ../ssos/util/*.o

dump: $(TARGET)
	$(OBJDUMP) -D $(TARGET).elf

compiledb: $(SRCS)
	compiledb make
