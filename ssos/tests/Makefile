# SSOS Unit Test Makefile
# Compiles tests in LOCAL_MODE for fast execution without emulator

# Check if cross-compilation toolchain is available
ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TARGET = test_runner
CC = m68k-xelf-gcc
AS = m68k-xelf-as

# Include paths
SSOS_ROOT = ..
INCLUDE = -I$(XELF_BASE)/m68k-elf/include \
          -I$(SSOS_ROOT)/os/kernel \
          -I$(SSOS_ROOT)/os/window \
          -I$(SSOS_ROOT)/os/main \
          -I$(SSOS_ROOT)/os/util \
          -Iframework

# Library paths
LIB = -L$(XELF_BASE)/m68k-elf/lib

# Compiler flags for testing
CFLAGS = $(INCLUDE) -O2 -g -DLOCAL_MODE -DTESTING
LDFLAGS = $(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc

# Test framework sources
FRAMEWORK_SRCS = framework/test_runner.c

# Unit test sources
UNIT_TEST_SRCS = unit/test_memory.c \
                  unit/test_scheduler.c \
                  unit/test_errors.c
                  # unit/test_layers.c - temporarily disabled due to layer dependency

# SSOS kernel sources (subset for testing)
KERNEL_SRCS = $(SSOS_ROOT)/os/kernel/memory.c \
               $(SSOS_ROOT)/os/kernel/task_manager.c \
               $(SSOS_ROOT)/os/kernel/ss_errors.c \
               $(SSOS_ROOT)/os/kernel/ss_perf.c \
               $(SSOS_ROOT)/os/kernel/stdlib_stubs.c \
               $(SSOS_ROOT)/os/window/quickdraw.c \
               $(SSOS_ROOT)/os/window/quickdraw_monitor.c \
               $(SSOS_ROOT)/os/window/quickdraw_shell.c \
               $(SSOS_ROOT)/os/util/printf.c

# Assembly sources (needed for some kernel functions)
ASM_SRCS = $(SSOS_ROOT)/os/kernel/interrupts.s

# All sources
ALL_SRCS = $(FRAMEWORK_SRCS) $(UNIT_TEST_SRCS) $(KERNEL_SRCS)

# Object files
ALL_OBJS = $(ALL_SRCS:.c=.o) $(ASM_SRCS:.s=.o)

# Assembly flags
ASFLAGS = -m68000 --register-prefix-optional --traditional-format \
          -I$(SSOS_ROOT)/include -I$(SSOS_ROOT)/os/kernel

.PHONY: all test clean help debug

all: $(TARGET)

test: $(TARGET)
	@echo "Running SSOS Unit Tests..."
	@echo "=========================="
	./$(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) $^ -o $(TARGET) -Xlinker "$(LDFLAGS)"

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

%.o: %.s
	$(AS) $< -o $@ $(ASFLAGS)

clean:
	rm -f $(TARGET) $(ALL_OBJS)
	rm -f framework/*.o unit/*.o
	rm -f $(SSOS_ROOT)/os/kernel/*.o
	rm -f $(SSOS_ROOT)/os/window/*.o
	rm -f $(SSOS_ROOT)/os/util/*.o
	rm -f $(SSOS_ROOT)/os/main/*.o

debug:
	@echo "FRAMEWORK_SRCS: $(FRAMEWORK_SRCS)"
	@echo "UNIT_TEST_SRCS: $(UNIT_TEST_SRCS)"
	@echo "KERNEL_SRCS: $(KERNEL_SRCS)"
	@echo "ALL_OBJS: $(ALL_OBJS)"

help:
	@echo "SSOS Unit Test Build System"
	@echo "==========================="
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build test runner"
	@echo "  test     - Build and run all unit tests"
	@echo "  clean    - Remove all build artifacts"
	@echo "  debug    - Show build configuration"
	@echo "  help     - Show this help message"