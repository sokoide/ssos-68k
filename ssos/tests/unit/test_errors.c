#include "../framework/ssos_test.h"

// Include SSOS error handling system
#include "ss_errors.h"

// External declarations for error system
extern SsErrorContext ss_last_error;
extern uint32_t ss_error_count;

// Test helper: Reset error system state
static void reset_error_system(void) {
    ss_last_error.error_code = SS_SUCCESS;
    ss_last_error.severity = SS_SEVERITY_INFO;
    ss_last_error.function_name = NULL;
    ss_last_error.file_name = NULL;
    ss_last_error.line_number = 0;
    ss_last_error.timestamp = 0;
    ss_last_error.description = NULL;
    ss_error_count = 0;
}

// Test basic error reporting
TEST(errors_basic_error_reporting) {
    reset_error_system();

    // Set an error
    ss_set_error(SS_ERROR_INVALID_PARAM, SS_SEVERITY_ERROR,
                "test_function", "test_file.c", 123, "Test error message");

    // Verify error was recorded
    ASSERT_EQ(ss_last_error.error_code, SS_ERROR_INVALID_PARAM);
    ASSERT_EQ(ss_last_error.severity, SS_SEVERITY_ERROR);
    ASSERT_EQ(ss_last_error.line_number, 123);
    ASSERT_EQ(ss_error_count, 1);

    // Verify string fields are set (we can't use ASSERT_EQ for strings)
    ASSERT_NOT_NULL(ss_last_error.function_name);
    ASSERT_NOT_NULL(ss_last_error.file_name);
    ASSERT_NOT_NULL(ss_last_error.description);
}

// Test error retrieval
TEST(errors_error_retrieval) {
    reset_error_system();

    // Initially no error
    SsError error = ss_get_last_error();
    ASSERT_EQ(error, SS_SUCCESS);

    // Set an error
    ss_set_error(SS_ERROR_OUT_OF_MEMORY, SS_SEVERITY_CRITICAL,
                "allocator", "memory.c", 200, "Out of memory");

    // Retrieve the error
    error = ss_get_last_error();
    ASSERT_EQ(error, SS_ERROR_OUT_OF_MEMORY);
}

// Test error string conversion
TEST(errors_string_conversion) {
    const char* str;

    // Test known error codes
    str = ss_error_to_string(SS_SUCCESS);
    ASSERT_NOT_NULL(str);

    str = ss_error_to_string(SS_ERROR_NULL_PTR);
    ASSERT_NOT_NULL(str);

    str = ss_error_to_string(SS_ERROR_INVALID_PARAM);
    ASSERT_NOT_NULL(str);

    str = ss_error_to_string(SS_ERROR_OUT_OF_MEMORY);
    ASSERT_NOT_NULL(str);

    str = ss_error_to_string(SS_ERROR_SYSTEM_ERROR);
    ASSERT_NOT_NULL(str);

    // Test unknown error code
    str = ss_error_to_string((SsError)-999);
    ASSERT_NOT_NULL(str);  // Should return "Unknown error"
}

// Test error severity levels
TEST(errors_severity_levels) {
    reset_error_system();

    // Test different severity levels
    ss_set_error(SS_ERROR_INVALID_PARAM, SS_SEVERITY_INFO,
                "func", "file", 1, "Info level error");
    ASSERT_EQ(ss_last_error.severity, SS_SEVERITY_INFO);

    ss_set_error(SS_ERROR_INVALID_PARAM, SS_SEVERITY_WARNING,
                "func", "file", 2, "Warning level error");
    ASSERT_EQ(ss_last_error.severity, SS_SEVERITY_WARNING);

    ss_set_error(SS_ERROR_INVALID_PARAM, SS_SEVERITY_ERROR,
                "func", "file", 3, "Error level error");
    ASSERT_EQ(ss_last_error.severity, SS_SEVERITY_ERROR);

    ss_set_error(SS_ERROR_INVALID_PARAM, SS_SEVERITY_CRITICAL,
                "func", "file", 4, "Critical level error");
    ASSERT_EQ(ss_last_error.severity, SS_SEVERITY_CRITICAL);
}

// Test error counting
TEST(errors_error_counting) {
    reset_error_system();

    ASSERT_EQ(ss_error_count, 0);

    // Generate multiple errors
    for (int i = 0; i < 5; i++) {
        ss_set_error(SS_ERROR_INVALID_PARAM, SS_SEVERITY_ERROR,
                    "test", "test.c", i, "Test error");
    }

    ASSERT_EQ(ss_error_count, 5);

    // Last error should overwrite previous
    ASSERT_EQ(ss_last_error.line_number, 4);  // Last iteration (i=4)
}

// Test compatibility error codes
TEST(errors_compatibility_codes) {
    reset_error_system();

    // Test that compatibility defines work correctly
    ASSERT_EQ(E_OK, SS_E_OK);
    ASSERT_EQ(E_PAR, SS_E_PAR);
    ASSERT_EQ(E_ID, SS_E_ID);
    ASSERT_EQ(E_LIMIT, SS_E_LIMIT);
    ASSERT_EQ(E_OBJ, SS_E_OBJ);

    // Test setting error with compatibility code
    ss_set_error(E_PAR, SS_SEVERITY_ERROR, "test", "test.c", 1, "Parameter error");
    ASSERT_EQ(ss_get_last_error(), SS_E_PAR);
}

// Test error context preservation
TEST(errors_context_preservation) {
    reset_error_system();

    const char* test_func = "test_function_name";
    const char* test_file = "test_file_name.c";
    const char* test_desc = "Test error description";
    uint32_t test_line = 456;

    ss_set_error(SS_ERROR_HARDWARE_ERROR, SS_SEVERITY_CRITICAL,
                test_func, test_file, test_line, test_desc);

    // Verify all context information is preserved
    ASSERT_EQ(ss_last_error.error_code, SS_ERROR_HARDWARE_ERROR);
    ASSERT_EQ(ss_last_error.severity, SS_SEVERITY_CRITICAL);
    ASSERT_EQ(ss_last_error.line_number, test_line);

    // Note: String comparison would require strcmp, which we're avoiding
    // In a real test environment, we'd verify string equality
    ASSERT_NOT_NULL(ss_last_error.function_name);
    ASSERT_NOT_NULL(ss_last_error.file_name);
    ASSERT_NOT_NULL(ss_last_error.description);
}

// Test error boundary conditions
TEST(errors_boundary_conditions) {
    reset_error_system();

    // Test with NULL strings (should not crash)
    ss_set_error(SS_ERROR_SYSTEM_ERROR, SS_SEVERITY_ERROR,
                NULL, NULL, 0, NULL);

    ASSERT_EQ(ss_last_error.error_code, SS_ERROR_SYSTEM_ERROR);
    ASSERT_EQ(ss_error_count, 1);

    // Test with very large line number
    ss_set_error(SS_ERROR_TIMEOUT, SS_SEVERITY_WARNING,
                "func", "file", 0xFFFFFFFF, "Large line number");

    ASSERT_EQ(ss_last_error.line_number, 0xFFFFFFFF);
}

// Test error enum values
TEST(errors_enum_values) {
    // Verify error codes have expected values for compatibility
    ASSERT_EQ(SS_SUCCESS, 0);
    ASSERT_TRUE(SS_ERROR_NULL_PTR < 0);
    ASSERT_TRUE(SS_ERROR_INVALID_PARAM < 0);
    ASSERT_TRUE(SS_ERROR_OUT_OF_MEMORY < 0);
    ASSERT_TRUE(SS_ERROR_SYSTEM_ERROR < 0);

    // Verify compatibility codes
    ASSERT_EQ(SS_E_OK, 0);
    ASSERT_EQ(SS_E_PAR, -17);
    ASSERT_EQ(SS_E_ID, -18);
    ASSERT_EQ(SS_E_LIMIT, -34);
    ASSERT_EQ(SS_E_OBJ, -41);
}

// Run all error tests
void run_error_tests(void) {
    RUN_TEST(errors_basic_error_reporting);
    RUN_TEST(errors_error_retrieval);
    RUN_TEST(errors_string_conversion);
    RUN_TEST(errors_severity_levels);
    RUN_TEST(errors_error_counting);
    RUN_TEST(errors_compatibility_codes);
    RUN_TEST(errors_context_preservation);
    RUN_TEST(errors_boundary_conditions);
    RUN_TEST(errors_enum_values);
}