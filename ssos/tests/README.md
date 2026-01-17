# SSOS Unit Testing Framework

A lightweight testing framework for SSOS that runs in LOCAL_MODE without requiring the X68000 emulator.

## Quick Start

```bash
# Source the cross-compilation environment
. ~/.elf2x68k

# Build and run all tests
make test

# Or build and run manually
make all
./test_runner
```

## Framework Structure

```text
tests/
├── framework/
│   ├── ssos_test.h        # Assertions and macros
│   ├── test_runner.c      # Main test execution engine
│   └── test_mocks.c       # Hardware mocks
├── unit/                  # Test suites
│   ├── test_memory.c      # Memory manager tests
│   ├── test_scheduler.c   # Task scheduler tests
│   ├── test_layers.c      # Window layer tests
│   └── test_errors.c      # Error handler tests
├── Makefile               # Build system
└── README.md              # This file
```

## Test Categories

### Memory Management Tests

- Basic allocation and deallocation
- 4K-aligned allocation (`ss_mem_alloc4k`)
- Fragmentation handling
- Out-of-memory scenarios
- Memory statistics validation

### Task Scheduler Tests

- Task creation with various priorities
- Priority-based scheduling logic
- Task state transitions (DORMANT → READY)
- Resource exhaustion handling
- Multiple tasks with same priority

### Layer Management Tests

- Layer allocation from pool
- Z-order management
- Dirty rectangle tracking
- Layer configuration and properties
- Memory management for VRAM buffers

### Error Handling Tests

- Error reporting and context tracking
- Severity level handling
- Error code validation
- Compatibility with legacy error codes
- Boundary condition testing

## Writing New Tests

### Basic Test Structure

```c
#include "../framework/ssos_test.h"
#include "your_module.h"

TEST(your_test_name) {
    // Setup
    setup_your_module();

    // Test
    int result = your_function(42);

    // Assert
    ASSERT_EQ(result, expected_value);
}

void run_your_tests(void) {
    RUN_TEST(your_test_name);
}
```

### Available Assertions

- `ASSERT_EQ(a, b)` - Values are equal
- `ASSERT_NEQ(a, b)` - Values are not equal
- `ASSERT_NOT_NULL(ptr)` - Pointer is not NULL
- `ASSERT_NULL(ptr)` - Pointer is NULL
- `ASSERT_TRUE(condition)` - Condition is true
- `ASSERT_FALSE(condition)` - Condition is false
- `ASSERT_ALIGNED_4K(ptr)` - Address is 4K-aligned
- `ASSERT_IN_RANGE(val, min, max)` - Value is in range

## Build Configuration

The test framework compiles with:

- `LOCAL_MODE` - Enables local execution without hardware dependencies
- `TESTING` - Enables testing-specific code paths
- Cross-compilation for m68k-xelf-gcc
- Optimized builds (-O2) for performance

## Benefits

- **Fast Feedback**: Tests run in milliseconds, not minutes
- **No Emulator Required**: 90% coverage without X68000 setup
- **Regression Detection**: Catch bugs before they reach integration
- **CI/CD Ready**: Lightweight enough for automated testing
- **Development Friendly**: Easy to add new tests and modify existing ones

## Troubleshooting

### Toolchain Not Found

```text
Error: Cross-compilation toolchain not found. Please run: . ~/.elf2x68k
```

**Solution**: Source the elf2x68k environment before running make.

### Build Errors

Most build errors are due to missing include paths or function signature mismatches. Check:

1. Include paths in Makefile are correct
2. Function signatures match the actual implementation
3. Required SSOS modules are included in KERNEL_SRCS

### Test Failures

When tests fail, they show the file, line number, and expected vs actual values:

```text
FAIL
  test_memory.c:45: Expected 4096, got 0
```

This helps quickly identify and fix issues in the tested code.
