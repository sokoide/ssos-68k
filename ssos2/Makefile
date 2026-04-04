# SSOS2 - Responsive GUI for X68000
# Build: source ~/.elf2x68k && make
# Run:   Copy ~/tmp/ssos2.x to XEiJ emulator

TARGET  = ssos2.x
DESTDIR = ~/tmp

CC = m68k-xelf-gcc
AS = m68k-xelf-as

XELF_BASE ?= /opt/homebrew/Cellar/elf2x68k/20260331

INCLUDE = -I$(XELF_BASE)/m68k-elf/include
LIB     = -L$(XELF_BASE)/m68k-elf/lib \
          -L$(shell dirname $(shell m68k-xelf-gcc -print-libgcc-file-name))

CFLAGS  = $(INCLUDE) -O2 -g
ASFLAGS = --strip-local-absolute -m68000 --register-prefix-optional

OBJS = main.o gui.o timer.o task.o dma.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ -Xlinker "$(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc"
	cp $@ $(DESTDIR)/

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

timer.o: timer.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
