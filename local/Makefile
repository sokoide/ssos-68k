TARGET=	local.x
SRCS=	main.c \
		../ssos/os/kernel/main.c \
		../ssos/os/kernel/crtc.c \
		../ssos/os/kernel/kernel.c \
		../ssos/os/kernel/vram.c \
		../ssos/os/util/printf.c

HEADERS=$(wildcard *.h)
OBJS=$(SRCS:.c=.o)
CC=m68k-xelf-gcc
OBJDUMP=m68k-xelf-objdump

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

%.o: %.c
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) *.elf *.o *.lst

dump: $(TARGET)
	$(OBJDUMP) -D $(TARGET).elf

compiledb: $(SRCS)
	compiledb make
