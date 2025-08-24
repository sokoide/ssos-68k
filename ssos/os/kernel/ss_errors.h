#pragma once

#include <stdint.h>

// Enhanced error handling framework for SSOS

// Error severity levels
typedef enum {
    SS_SEVERITY_INFO = 0,
    SS_SEVERITY_WARNING = 1,
    SS_SEVERITY_ERROR = 2,
    SS_SEVERITY_CRITICAL = 3
} SsErrorSeverity;

// Extended error codes with better categorization
typedef enum {
    // Success
    SS_SUCCESS = 0,

    // Parameter errors
    SS_ERROR_NULL_PTR = -1,
    SS_ERROR_INVALID_PARAM = -2,
    SS_ERROR_OUT_OF_BOUNDS = -3,
    SS_ERROR_INVALID_ID = -4,

    // Resource errors
    SS_ERROR_OUT_OF_MEMORY = -5,
    SS_ERROR_OUT_OF_RESOURCES = -6,
    SS_ERROR_RESOURCE_BUSY = -7,

    // State errors
    SS_ERROR_INVALID_STATE = -8,
    SS_ERROR_NOT_INITIALIZED = -9,
    SS_ERROR_ALREADY_INITIALIZED = -10,

    // System errors
    SS_ERROR_SYSTEM_ERROR = -11,
    SS_ERROR_HARDWARE_ERROR = -12,
    SS_ERROR_TIMEOUT = -13,

    // Compatibility (uT-Kernel error codes)
    SS_E_OK = 0,
    SS_E_PAR = -17,
    SS_E_ID = -18,
    SS_E_LIMIT = -34,
    SS_E_OBJ = -41
} SsError;

// Error context structure for debugging
typedef struct {
    SsError error_code;
    SsErrorSeverity severity;
    const char* function_name;
    const char* file_name;
    uint32_t line_number;
    uint32_t timestamp;
    const char* description;
} SsErrorContext;

// Global error tracking
extern SsErrorContext ss_last_error;
extern uint32_t ss_error_count;

// Error handling functions
void ss_set_error(SsError error_code, SsErrorSeverity severity,
                  const char* function, const char* file, uint32_t line,
                  const char* description);

SsError ss_get_last_error(void);
const char* ss_error_to_string(SsError error);

// Assertion system for debugging
#ifdef SS_DEBUG
#define SS_ASSERT(condition, error_code, description) \
    if (!(condition)) { \
        ss_set_error(error_code, SS_SEVERITY_CRITICAL, \
                    __func__, __FILE__, __LINE__, description); \
        return error_code; \
    }

#define SS_ASSERT_VOID(condition, error_code, description) \
    if (!(condition)) { \
        ss_set_error(error_code, SS_SEVERITY_CRITICAL, \
                    __func__, __FILE__, __LINE__, description); \
        return; \
    }
#else
#define SS_ASSERT(condition, error_code, description) (void)0
#define SS_ASSERT_VOID(condition, error_code, description) (void)0
#endif

// Input validation macros
#define SS_CHECK_NULL(ptr) \
    if ((ptr) == NULL) { \
        ss_set_error(SS_ERROR_NULL_PTR, SS_SEVERITY_ERROR, \
                    __func__, __FILE__, __LINE__, "NULL pointer parameter"); \
        return SS_ERROR_NULL_PTR; \
    }

#define SS_CHECK_RANGE(value, min, max) \
    if ((value) < (min) || (value) > (max)) { \
        ss_set_error(SS_ERROR_OUT_OF_BOUNDS, SS_SEVERITY_ERROR, \
                    __func__, __FILE__, __LINE__, "Parameter out of bounds"); \
        return SS_ERROR_OUT_OF_BOUNDS; \
    }

#define SS_CHECK_ID(id, max_id) \
    if ((id) <= 0 || (id) > (max_id)) { \
        ss_set_error(SS_ERROR_INVALID_ID, SS_SEVERITY_ERROR, \
                    __func__, __FILE__, __LINE__, "Invalid ID parameter"); \
        return SS_ERROR_INVALID_ID; \
    }

// Legacy compatibility
#define E_OK SS_E_OK
#define E_PAR SS_E_PAR
#define E_ID SS_E_ID
#define E_LIMIT SS_E_LIMIT
#define E_OBJ SS_E_OBJ