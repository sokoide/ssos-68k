ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

# Threading model: cooperative or preemptive. Selects kernel/$(SCHED) sources.
SCHED ?= cooperative
ifeq ($(SCHED),cooperative)
else ifeq ($(SCHED),preemptive)
else
$(error Invalid SCHED='$(SCHED)'. Use: make SCHED=cooperative|preemptive)
endif

# GFX profiling foundation. Set to 1 to enable counters; 0 compiles them out.
SS_PROFILE_GFX ?= 0
ifeq ($(SS_PROFILE_GFX),0)
PROFILE_SUFFIX :=
else
PROFILE_SUFFIX := _profile$(SS_PROFILE_GFX)
endif

# Output name reflects the threading model.
ifeq ($(SCHED),cooperative)
TARGET=ssos_cop$(PROFILE_SUFFIX).x
CDEFINES=-DSS_BUILD_COOPERATIVE
else
TARGET=ssos_pre$(PROFILE_SUFFIX).x
CDEFINES=-DSS_BUILD_PREEMPTIVE
endif

KDIR=../os/kernel/$(SCHED)

SRCS=	main.c \
		../os/app/scene.c \
		../os/kernel/main_task.c \
		../os/kernel/scheduler.c \
		$(KDIR)/wakeups.c \
		../os/mem/buddy.c \
		../os/mem/slab.c \
		../os/gfx/profile.c \
		../os/gfx/palette.c \
		../os/gfx/vram.c \
		../os/win/window.c \
		../os/util/numfmt.c
ASRCS=	$(KDIR)/interrupts.s ../os/kernel/mfp_state.s trap14.s

OBJDIR=build/$(SCHED)/profile-$(SS_PROFILE_GFX)

SRC_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SRCS)))
ASRC_OBJS=$(patsubst %.s,$(OBJDIR)/%.o,$(notdir $(ASRCS)))
OBJS=$(ASRC_OBJS) $(SRC_OBJS)
TARGET_FILE=$(OBJDIR)/$(TARGET)

CC=m68k-xelf-gcc
AS=m68k-xelf-as
LD=m68k-xelf-ld
OBJDUMP=m68k-elf-objdump
READELF=m68k-elf-readelf

INCLUDE=-I$(XELF_BASE)/m68k-elf/include -I../include -I../os/kernel -I$(KDIR) -I../os/mem -I../os/gfx -I../os/win -I../os/util
LIB=-L$(XELF_BASE)/m68k-elf/lib

CCFLAGS=$(INCLUDE) -O2 -g -MMD -MP -DLOCAL_MODE $(CDEFINES) -DSS_PROFILE_GFX=$(SS_PROFILE_GFX)
LDFLAGS=$(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc

.PHONY: all clean install dump readelf

all: standalone

standalone: $(TARGET_FILE)
	cp $(TARGET_FILE) $(HOME)/tmp/$(TARGET)

$(TARGET_FILE): $(OBJS)
	$(CC) $^ -o $@ -Xlinker "$(LDFLAGS)"

dump: $(TARGET_FILE)
	$(OBJDUMP) -d -S -C $(TARGET_FILE)

readelf: $(TARGET_FILE)
	$(READELF) -a $(TARGET_FILE)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

# VPATH lets the generic %.o: %.c / %.s rules find sources across the
# variant kernel dir and the shared subsystem dirs.
VPATH=$(KDIR):../os/app:../os/kernel:../os/mem:../os/gfx:../os/win:../os/util

$(OBJDIR)/scene.o: ../os/app/scene.c | $(OBJDIR)
	$(CC) -c $< -o $@ $(CCFLAGS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) -c $< -o $@ $(CCFLAGS)

$(OBJDIR)/trap14.o: trap14.s | $(OBJDIR)
	$(AS) --strip-local-absolute $< -o $@ -m68000 --register-prefix-optional --traditional-format -I../include

$(OBJDIR)/%.o: %.s | $(OBJDIR)
	$(AS) --strip-local-absolute $< -o $@ -m68000 --register-prefix-optional --traditional-format -I../include -I../os/kernel -I$(KDIR)

-include $(OBJS:.o=.d)

clean:
	rm -rf build
	rm -rf $(TARGET) obj-cooperative obj-preemptive *.elf *.o *.lst
