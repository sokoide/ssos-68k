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

# GFX profiling foundation. Set to 1 to enable counters; 0 compiles them out.
SS_PROFILE_GFX ?= 0

# Per-variant sources live under kernel/$(SCHED)/; shared kernel code stays at kernel/.
KDIR=kernel/$(SCHED)

TARGET=SSOS.X
SRCS= \
	kernel/premain.c \
	kernel/main_task.c \
	kernel/scheduler.c \
	$(KDIR)/wakeups.c \
	kernel/work_queue.c \
	util/numfmt.c \
	mem/buddy.c \
	mem/slab.c \
	gfx/profile.c \
	gfx/palette.c \
	gfx/vram.c \
	win/window.c \
	ipc/message.c \
	app/main.c \
	app/scene.c
ASRCS= \
	kernel/entry.s \
	kernel/mfp_state.s \
	$(KDIR)/interrupts.s

BUILD_DIR=build/$(SCHED)/profile-$(SS_PROFILE_GFX)
OBJS=$(addprefix $(BUILD_DIR)/,$(ASRCS:.s=.o) $(SRCS:.c=.o))
TARGET_ELF=$(BUILD_DIR)/$(TARGET).elf
TARGET_BIN=$(BUILD_DIR)/$(TARGET).bin

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJCOPY=m68k-xelf-objcopy
OBJDUMP=m68k-elf-objdump
READELF=m68k-elf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../include -I./kernel -I./$(KDIR) -I./mem -I./app -I./gfx -I./win -I./ipc -I./util
LIB=-L$(XELF_BASE)/m68k-elf/lib -L$(shell dirname $(shell m68k-xelf-gcc -print-libgcc-file-name))

CCFLAGS=$(INCLUDE) -g -O2 -MMD -MP -DSS_PROFILE_GFX=$(SS_PROFILE_GFX)
LDFLAGS=$(LIB) -lx68kiocs -Tkernel/linker.ld -nostdlib -lc -lm -lgcc -lnosys
ASFLAGS=-m68000 --register-prefix-optional --traditional-format -I../include -I./kernel

.PHONY: all clean dump readelf

all: $(TARGET_BIN)

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

$(TARGET_ELF): $(OBJS) kernel/linker.ld
	@mkdir -p $(dir $@)
	$(LD) --strip-all $(OBJS) -o $@ $(LDFLAGS) -e entry

dump: $(TARGET_ELF)
	$(OBJDUMP) -d -S -C $(TARGET_ELF)

readelf: $(TARGET_ELF)
	$(READELF) -a $(TARGET_ELF)

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) --strip-local-absolute $< -o $@ $(ASFLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CCFLAGS)

-include $(OBJS:.o=.d)

clean:
	rm -rf build
	rm -f $(TARGET) $(TARGET).bin $(TARGET).elf $(ASRCS:.s=.o) $(SRCS:.c=.o)
	rm -f kernel/*.o kernel/*.lst kernel/cooperative/*.o kernel/preemptive/*.o
