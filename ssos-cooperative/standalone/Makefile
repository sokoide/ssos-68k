ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TARGET=	ssos_cop.x
SRCS=	main.c \
		../os/kernel/scheduler.c \
		../os/mem/buddy.c \
		../os/mem/slab.c \
		../os/gfx/vram.c \
		../os/win/window.c
ASRCS=	../os/kernel/interrupts.s

OBJDIR=obj

SRC_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SRCS)))
ASRC_OBJS=$(patsubst %.s,$(OBJDIR)/%.o,$(notdir $(ASRCS)))
OBJS=$(ASRC_OBJS) $(SRC_OBJS)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJDUMP=m68k-elf-objdump
READELF=m68k-elf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../include -I../os/kernel -I../os/mem -I../os/gfx -I../os/win
LIB=-L$(XELF_BASE)/m68k-elf/lib

CCFLAGS=$(INCLUDE) -O2 -g -DLOCAL_MODE
LDFLAGS=$(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc

.PHONY: all clean install dump readelf

all: standalone

standalone: $(TARGET)
	cp $(TARGET) $(HOME)/tmp

$(TARGET): $(OBJS)
	$(CC) $^ -o $(TARGET) -Xlinker "$(LDFLAGS)"

dump: $(TARGET)
	$(OBJDUMP) -d -S -C $(TARGET).elf

readelf: $(TARGET)
	$(READELF) -a $(TARGET).elf

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

$(OBJDIR)/%.o: ../os/kernel/%.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

$(OBJDIR)/%.o: ../os/kernel/%.s | $(OBJDIR)
	$(AS) --strip-local-absolute $< -o $@ -m68000 --register-prefix-optional --traditional-format -I../include -I../os/kernel

$(OBJDIR)/%.o: ../os/mem/%.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

$(OBJDIR)/%.o: ../os/gfx/%.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

$(OBJDIR)/%.o: ../os/win/%.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) $(OBJDIR) *.elf *.o *.lst
