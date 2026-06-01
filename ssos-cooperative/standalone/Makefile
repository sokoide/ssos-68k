ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TARGET=	standalone.x
SRCS=	main.c \
		../os/kernel/scheduler.c \
		../os/mem/buddy.c \
		../os/mem/slab.c
ASRCS=	../os/kernel/interrupts.s

OBJDIR=obj

SRC_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SRCS)))
ASRC_OBJS=$(patsubst %.s,$(OBJDIR)/%.o,$(notdir $(ASRCS)))
OBJS=$(ASRC_OBJS) $(SRC_OBJS)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJDUMP=m68k-xelf-objdump
READELF=m68k-xelf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../include -I../os/kernel -I../os/mem
LIB=-L$(XELF_BASE)/m68k-elf/lib

CCFLAGS=$(INCLUDE) -O2 -g -DLOCAL_MODE
LDFLAGS=$(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc

.PHONY: all clean install

all: standalone

standalone: $(TARGET)
	cp $(TARGET) $(HOME)/tmp

$(TARGET): $(OBJS)
	$(CC) $^ -o $(TARGET) -Xlinker "$(LDFLAGS)"

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

clean:
	rm -rf $(TARGET) $(OBJDIR) *.elf *.o *.lst
