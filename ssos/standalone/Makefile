# Check if cross-compilation toolchain is available
ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TARGET=	standalone.x
SRCS=	main.c \
	../os/kernel/kernel.c \
	../os/kernel/vram.c \
	../os/kernel/memory.c \
	../os/kernel/task_manager.c \
	../os/kernel/dma.c \
	../os/kernel/ss_errors.c \
	../os/kernel/ss_perf.c \
	../os/kernel/input.c \
	../os/main/ssosmain.c \
	../os/main/cli.c \
	../os/util/printf.c \
	../os/util/string.c
ASRCS= \
	../os/kernel/interrupts.s \
	../os/kernel/aux.s

# Separate object directory to avoid conflicts with OS build
OBJDIR=obj
HEADERS=$(wildcard *.h)

# Generate object paths with obj/ prefix
SRC_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SRCS)))
ASM_OBJS=$(patsubst %.s,$(OBJDIR)/%.o,$(notdir $(ASRCS)))
OBJS=$(ASM_OBJS) $(SRC_OBJS)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJDUMP=m68k-xelf-objdump
OBJDUMP=m68k-xelf-objdump
READELF=m68k-xelf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../os/util -I../os/kernel -I../os/main
LIB=-L$(XELF_BASE)/m68k-elf/lib

CCFLAGS=$(INCLUDE) -O2 -g -DLOCAL_MODE
# LDFLAGS=$(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc -Tlinker.ld
LDFLAGS=$(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc
ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include -I../os/kernel

.PHONY:
	clean install dump compiledb format

all: $(TARGET)
	cp $(TARGET) $(HOME)/tmp

install: $(TARGET)
	cp $(TARGET) $(HOME)/tmp

$(TARGET): $(OBJS)
	$(CC) $^ -o $(TARGET) -Xlinker "$(LDFLAGS)"

# Create object directory
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Assembly file compilation with directory creation
$(OBJDIR)/%.o: ../os/kernel/%.s | $(OBJDIR)
	$(AS) --strip-local-absolute $< -o $@ $(ASFLAGS)

# Local C file compilation
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

# OS kernel C files
$(OBJDIR)/%.o: ../os/kernel/%.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

# OS main C files
$(OBJDIR)/%.o: ../os/main/%.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

# OS util C files
$(OBJDIR)/%.o: ../os/util/%.c | $(OBJDIR)
	$(CC) -Wa,-adhlns="$@.lst" -c $< -o $@ $(CCFLAGS)

clean:
	rm -rf $(TARGET) $(OBJDIR) *.elf *.o *.lst

dump: $(TARGET)
	$(OBJDUMP) -D $(TARGET).elf

readelf: $(TARGET)
	$(READELF) -a $(TARGET).elf

compiledb: $(SRCS)
	compiledb make

format: $(SRCS)
	@echo "Formatting all .c and .h files..."
	@find . -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo "Formatting complete."