# SSOS Unit Testing Framework

A lightweight testing framework for SSOS that runs in LOCAL_MODE without requiring the X68000 emulator.

## Quick Start

### Native Testing (Recommended for Development)

```bash
cd tests
make -f Makefile.native test
```

### Cross-Compiled Testing (For X68000)

```bash
cd tests
. ~/.elf2x68k
make clean && make
```

## Test Structure

### Framework Components

- `framework/ssos_test.h` - Core testing macros and utilities
- `framework/test_runner.c` - Main test execution engine
- `framework/test_mocks.c` - Mock implementations for SSOS functions

### Test Suites

- `unit/test_memory.c` - Memory allocator unit tests
- `unit/test_scheduler.c` - Task scheduler unit tests  
- `unit/test_layers.c` - Window layer management tests
- `unit/test_errors.c` - Error handling system tests

## Writing Tests

### Basic Test Structure

```c
#include "../framework/ssos_test.h"

TEST(my_test_name) {
    // Test setup
    int value = 42;
    
    // Assertions
    ASSERT_EQ(value, 42);
    ASSERT_TRUE(value > 0);
    ASSERT_NOT_NULL(&value);
}

void run_my_tests(void) {
    RUN_TEST(my_test_name);
}
```

### Available Assertions

- `ASSERT_TRUE(condition)` - Assert condition is true
- `ASSERT_FALSE(condition)` - Assert condition is false
- `ASSERT_EQ(expected, actual)` - Assert values are equal
- `ASSERT_NEQ(expected, actual)` - Assert values are not equal
- `ASSERT_NOT_NULL(ptr)` - Assert pointer is not NULL
- `ASSERT_IN_RANGE(value, min, max)` - Assert value is in range
- `ASSERT_ALIGNED_4K(addr)` - Assert address is 4K-aligned

### Memory Testing Patterns

```c
TEST(memory_allocation_test) {
    setup_memory_system();
    
    uint32_t addr = ss_mem_alloc(1024);
    ASSERT_NEQ(addr, 0);  // Allocation succeeded
    
    teardown_memory_system();
}
```

### Task Scheduler Testing Patterns  

```c
TEST(scheduler_test) {
    reset_scheduler_state();
    ss_mem_init();
    
    int task_id = create_test_task(5);
    ASSERT_TRUE(task_id > 0);
    
    TaskControlBlock* tcb = &tcb_table[task_id - 1];
    ASSERT_EQ(tcb->state, TS_DORMANT);
}
```

## Test Configuration

### Build Modes

- **Native Mode**: `NATIVE_TEST=1` - Runs on host system using mocks
- **Cross-Compiled**: Default - Builds for X68000 using real SSOS code

### Compilation Flags

- `LOCAL_MODE` - Enables local execution without hardware dependencies
- `TESTING` - Enables test-specific code paths
- `NATIVE_TEST` - Enables native compilation with mock implementations

## Mock System

The framework provides mock implementations for:

- Memory allocation (`ss_mem_alloc`, `ss_mem_alloc4k`)
- Task management (`ss_create_task`, `ss_start_task`)
- Layer system (`ss_layer_new`, `ss_layer_set`)
- Error handling (`ss_set_error`, `ss_get_last_error`)
- Hardware abstraction (DMA, interrupts, VRAM)

### Adding New Mocks

Add mock functions to `framework/test_mocks.c`:

```c
#ifdef NATIVE_TEST
// Native mock implementation
int my_function(int param) {
    return param * 2;  // Simple mock behavior
}
#endif
```

## Testing Best Practices

### Test Organization

- Group related tests in the same file
- Use descriptive test names
- Test both success and failure cases
- Include boundary condition tests

### Memory Testing

- Always test allocation success/failure
- Verify memory alignment requirements
- Test fragmentation scenarios
- Check memory statistics consistency

### Scheduler Testing  

- Test task creation, startup, and state transitions
- Verify priority-based scheduling
- Test resource exhaustion scenarios
- Validate queue management

### Layer Testing

- Test layer allocation and configuration
- Verify z-order management
- Test dirty rectangle tracking
- Validate boundary conditions

## Troubleshooting

### Common Issues

1. **Toolchain not found**: Run `. ~/.elf2x68k` before cross-compilation
2. **Link errors**: Ensure all required SSOS functions are mocked
3. **Assertion failures**: Check that test expectations match mock behavior

### Adding Test Coverage

1. Identify SSOS component to test
2. Create test file in `unit/` directory
3. Add mock implementations if needed
4. Update test runner to include new tests
5. Add to appropriate Makefile

## Performance Notes

- Native tests run ~100x faster than emulated tests
- Use native testing for rapid development iteration
- Use cross-compiled tests for final validation
- Mock implementations prioritize speed over accuracy

## Integration with SSOS Build System

The testing framework is designed to:

- Share source code with main SSOS build
- Use conditional compilation for test-specific code
- Maintain compatibility with existing build processes
- Support both development and validation workflows
