#include "ss_errors.h"

#include <string.h>

#include "kernel.h"

// Global error tracking variables
SsErrorContext ss_last_error = {0};
uint32_t ss_error_count = 0;

// Set an error with full context
void ss_set_error(SsError error_code, SsErrorSeverity severity,
                  const char* function, const char* file, uint32_t line,
                  const char* description) {
    ss_last_error.error_code = error_code;
    ss_last_error.severity = severity;
    ss_last_error.function_name = function;
    ss_last_error.file_name = file;
    ss_last_error.line_number = line;
    ss_last_error.timestamp = ss_timerd_counter;  // Use system timer
    ss_last_error.description = description;

    ss_error_count++;

    // In debug mode, we could add logging here
    // For now, just track the error
}

// Get the last error code
SsError ss_get_last_error(void) {
    return ss_last_error.error_code;
}

// Convert error code to string for debugging
const char* ss_error_to_string(SsError error) {
    switch (error) {
    case SS_SUCCESS:
        return "Success";
    case SS_ERROR_NULL_PTR:
        return "NULL pointer error";
    case SS_ERROR_INVALID_PARAM:
        return "Invalid parameter";
    case SS_ERROR_OUT_OF_BOUNDS:
        return "Out of bounds";
    case SS_ERROR_INVALID_ID:
        return "Invalid ID";
    case SS_ERROR_OUT_OF_MEMORY:
        return "Out of memory";
    case SS_ERROR_OUT_OF_RESOURCES:
        return "Out of resources";
    case SS_ERROR_RESOURCE_BUSY:
        return "Resource busy";
    case SS_ERROR_INVALID_STATE:
        return "Invalid state";
    case SS_ERROR_NOT_INITIALIZED:
        return "Not initialized";
    case SS_ERROR_ALREADY_INITIALIZED:
        return "Already initialized";
    case SS_ERROR_SYSTEM_ERROR:
        return "System error";
    case SS_ERROR_HARDWARE_ERROR:
        return "Hardware error";
    case SS_ERROR_TIMEOUT:
        return "Timeout error";
    default:
        return "Unknown error";
    }
}