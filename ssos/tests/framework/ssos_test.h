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
            printf("  %s:%d: Expected %d, got %d\n", __FILE__, __LINE__, (int)(b), (int)(a)); \
            failed_tests++; \
            return; \
        } \
    } while(0)

#define ASSERT_NEQ(a, b) \
    do { \
        if ((a) == (b)) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Expected %d != %d\n", __FILE__, __LINE__, (int)(a), (int)(b)); \
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

#define ASSERT_STREQ(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            printf("FAIL\n"); \
            printf("  %s:%d: Expected strings to be equal:\n", __FILE__, __LINE__); \
            printf("    Expected: \"%s\"\n", (b)); \
            printf("    Got:      \"%s\"\n", (a)); \
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
            printf("  %s:%d: Value %d not in range [%d, %d]\n", __FILE__, __LINE__, (int)(val), (int)(min), (int)(max)); \
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