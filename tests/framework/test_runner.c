#include "ssos_test.h"
#include <stdlib.h>

// Global test statistics
int total_tests = 0;
int failed_tests = 0;
const char* current_test_name = NULL;

void test_framework_init(void) {
    total_tests = 0;
    failed_tests = 0;
    current_test_name = NULL;
    printf("SSOS Unit Test Framework\n");
    printf("========================\n\n");
}

void test_framework_report(void) {
    printf("\nTest Results:\n");
    printf("-------------\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", total_tests - failed_tests);
    printf("Failed: %d\n", failed_tests);

    if (failed_tests == 0) {
        printf("\n[PASS] All tests PASSED!\n");
    } else {
        printf("\n[FAIL] %d test(s) FAILED\n", failed_tests);
    }
}

int test_framework_exit_code(void) {
    return (failed_tests == 0) ? 0 : 1;
}

void reset_mocks(void) {
    // Reset any mock state here
    // For now, this is a placeholder for future mock implementations
}

// Test runner main function
int main() {
    test_framework_init();

    // Test function declarations - these will be defined in unit test files
    extern void run_memory_tests(void);
    extern void run_scheduler_tests(void);
    extern void run_layer_tests(void);
    extern void run_error_tests(void);
    extern void run_performance_tests(void);
    extern void run_kernel_tests(void);

    // Run all test suites
    printf("Running Memory Tests...\n");
    run_memory_tests();

    printf("\nRunning Scheduler Tests...\n");
    run_scheduler_tests();

    printf("\nRunning Layer Tests...\n");
    run_layer_tests();

    printf("\nRunning Error Tests...\n");
    run_error_tests();

    printf("\nRunning Performance Tests...\n");
    run_performance_tests();

    printf("\nRunning Kernel Tests...\n");
    run_kernel_tests();

    test_framework_report();
    return test_framework_exit_code();
}