#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Test framework for SSOS - designed for LOCAL_MODE testing
// Lightweight framework focusing on fast feedback and regression detection

// Global test statistics
extern int total_tests;
extern int failed_tests;
extern const char* current_test_name;

// Test definition macros
#define TEST(name) \
    void test_##name(void); \
    void test_##name(void)

// Test execution macro
#define RUN_TEST(name) \
    do { \
        current_test_name = #name; \
        total_tests++; \
        printf("Running test: %s... ", #name); \
        fflush(stdout); \
        test_##name(); \
        printf("PASS\n"); \
    } while(0)

// Assertion macros
#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Expected ", __FILE__, __LINE__); \
            _print_value((uintptr_t)b); \
            printf(", got "); \
            _print_value((uintptr_t)a); \
            printf("\n"); \
            failed_tests++; \
            return; \
        } \
    } while(0)

#define ASSERT_NEQ(a, b) \
    do { \
        if ((a) == (b)) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Expected ", __FILE__, __LINE__); \
            _print_value((uintptr_t)a); \
            printf(" != "); \
            _print_value((uintptr_t)b); \
            printf("\n"); \
            failed_tests++; \
            return; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Expected non-NULL pointer\n", __FILE__, __LINE__); \
            failed_tests++; \
            return; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Expected NULL pointer, got %p\n", __FILE__, __LINE__, (void*)(ptr)); \
            failed_tests++; \
            return; \
        } \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Expected true condition\n", __FILE__, __LINE__); \
            failed_tests++; \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Expected false condition\n", __FILE__, __LINE__); \
            failed_tests++; \
            return; \
        } \
    } while(0)

// Memory-specific assertions
#define ASSERT_ALIGNED_4K(ptr) \
    do { \
        uint32_t addr = (uint32_t)(ptr); \
        if (addr & 0xFFF) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Address 0x%08x not 4K-aligned\n", __FILE__, __LINE__, addr); \
            failed_tests++; \
            return; \
        } \
    } while(0)

#define ASSERT_IN_RANGE(val, min, max) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Value ", __FILE__, __LINE__); \
            _print_value((uintptr_t)val); \
            printf(" not in range ["); \
            _print_value((uintptr_t)min); \
            printf(", "); \
            _print_value((uintptr_t)max); \
            printf("]\n"); \
            failed_tests++; \
            return; \
        } \
    } while(0)

// Test utilities
void test_framework_init(void);
void test_framework_report(void);
int test_framework_exit_code(void);

// Mock/stub support
void reset_mocks(void);

// Helper function for printing values of different types
static inline void _print_value(uintptr_t val) {
    // Check if this looks like a pointer (high bit set or specific address ranges)
    // For native testing, valid pointers are usually in higher address ranges
    if ((val > 0x10000 && val < 0xFFFFFFFFFFFFFFFFULL) || val == 0) {
        printf("%p", (void*)val);
    } else {
        // Treat as regular integer
        printf("%d", (int)val);
    }
}