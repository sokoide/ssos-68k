# SSOS Unit Test Makefile
# Compiles tests in LOCAL_MODE for fast execution without emulator

# Default target for cross-compilation
TARGET = test_runner

# Check if cross-compilation toolchain is available (skip check for native builds)
ifeq ($(filter-out native clean debug help,$(MAKECMDGOALS)),)
  # Native-only targets don't need cross-compilation toolchain
else
  ifeq ($(shell which m68k-xelf-gcc),)
    $(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
  endif
endif

CC = m68k-xelf-gcc
AS = m68k-xelf-as

# Include paths
SSOS_ROOT = ..
INCLUDE = -I$(XELF_BASE)/m68k-elf/include \
           -I$(SSOS_ROOT)/ssos/os/kernel \
           -I$(SSOS_ROOT)/ssos/os/window \
           -I$(SSOS_ROOT)/ssos/os/main \
           -I$(SSOS_ROOT)/ssos/os/util \
           -I$(SSOS_ROOT)/ssos/tests/framework

# Library paths
LIB = -L$(XELF_BASE)/m68k-elf/lib

# Compiler flags for testing
CFLAGS = $(INCLUDE) -O2 -g -DLOCAL_MODE -DTESTING
LDFLAGS = $(LIB) -lx68kiocs -nostdlib -lc -lm -lgcc

# Test framework sources
FRAMEWORK_SRCS = $(SSOS_ROOT)/ssos/tests/framework/test_runner.c \
                  $(SSOS_ROOT)/ssos/tests/framework/test_mocks.c

# Unit test sources
UNIT_TEST_SRCS = $(SSOS_ROOT)/ssos/tests/unit/test_quickdraw.c

# SSOS kernel sources (subset for testing)
KERNEL_SRCS = $(SSOS_ROOT)/ssos/os/kernel/memory.c \
                $(SSOS_ROOT)/ssos/os/kernel/task_manager.c \
                $(SSOS_ROOT)/ssos/os/kernel/ss_errors.c \
                $(SSOS_ROOT)/ssos/os/kernel/ss_perf.c \
                $(SSOS_ROOT)/ssos/os/window/layer.c \
                $(SSOS_ROOT)/ssos/os/window/quickdraw.c \
                $(SSOS_ROOT)/ssos/os/util/printf.c

# Assembly sources (excluded for testing - conflicts with mocks)
# ASM_SRCS = $(SSOS_ROOT)/os/kernel/interrupts.s

# All sources
ALL_SRCS = $(FRAMEWORK_SRCS) $(UNIT_TEST_SRCS) $(KERNEL_SRCS)

# Object files
ALL_OBJS = $(ALL_SRCS:.c=.o) $(ASM_SRCS:.s=.o)

# Assembly flags
ASFLAGS = -m68000 --register-prefix-optional --traditional-format \
           -I$(SSOS_ROOT)/ssos/include -I$(SSOS_ROOT)/ssos/os/kernel -I$(SSOS_ROOT)/ssos/tests/framework

.PHONY: all test clean help debug native

all: $(TARGET)

# Native build target for host system execution
native:
	@echo "Building native tests for host system..."
	gcc -I$(SSOS_ROOT)/ssos/os/kernel -I$(SSOS_ROOT)/ssos/os/window -I$(SSOS_ROOT)/ssos/os/main -I$(SSOS_ROOT)/ssos/os/util -I$(SSOS_ROOT)/ssos/tests/framework \
	    -O2 -g -DLOCAL_MODE -DTESTING -DNATIVE_TEST \
	    $(SSOS_ROOT)/ssos/tests/framework/test_runner.c \
	    $(SSOS_ROOT)/ssos/tests/unit/test_quickdraw.c \
	    $(SSOS_ROOT)/ssos/os/window/quickdraw.c \
	    -o test_runner_native -lm
	@echo "Running native tests..."
	./test_runner_native

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
	rm -f $(SSOS_ROOT)/ssos/tests/framework/*.o $(SSOS_ROOT)/ssos/tests/unit/*.o
	rm -f $(SSOS_ROOT)/ssos/os/kernel/*.o
	rm -f $(SSOS_ROOT)/ssos/os/window/*.o
	rm -f $(SSOS_ROOT)/ssos/os/util/*.o

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