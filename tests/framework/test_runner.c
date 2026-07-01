/* test_runner.c - SSOS native unit test entry point.
 *
 * Pulls in every test suite (one run_*_tests() per unit/test_*.c) and reports
 * the aggregate result via exit code so `make test` can pass/fail in CI.
 * Suites are added here as they are implemented. */

#include "ssos_test.h"

/* Per-suite entry points (defined in unit/test_*.c) */
void run_numfmt_tests(void);
void run_mem_tests(void);
void run_scheduler_tests(void);
void run_window_tests(void);
void run_ipc_tests(void);

/* Global statistics, referenced by the ASSERT_* macros in ssos_test.h */
int total_tests = 0;
int failed_tests = 0;
const char* current_test_name = NULL;

void test_framework_init(void) {
    printf("=== SSOS Native Unit Tests ===\n");
}

void test_framework_report(void) {
    int passed = total_tests - failed_tests;
    printf("\n--- Results: %d passed, %d failed, %d total ---\n",
           passed, failed_tests, total_tests);
}

int test_framework_exit_code(void) {
    return failed_tests ? 1 : 0;
}

int main(void) {
    test_framework_init();

    run_numfmt_tests();
    run_mem_tests();
    run_scheduler_tests();
    run_window_tests();
    run_ipc_tests();

    test_framework_report();
    return test_framework_exit_code();
}
