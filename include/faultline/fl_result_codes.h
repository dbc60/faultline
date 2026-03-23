#ifndef FL_RESULT_CODES_H_
#define FL_RESULT_CODES_H_

/**
 * @file faultline_result_codes.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Test Result Codes - Public API
 * @version 0.2.0
 * @date 2025-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This header defines the result codes returned by FaultLine test execution.
 * Result codes indicate the outcome of a test case, ranging from successful
 * execution to various failure modes including resource leaks and invalid operations.
 */

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief FLResultCode is an enumeration of the possible results of a test case.
 *
 * It is loosely ordered from the best result to the worst result.
 *
 */
typedef enum {
    FL_NOT_RUN,      ///< not executed
    FL_PASS,         ///< no errors detected
    FL_LEAK,         ///< resource-leaked detected
    FL_INVALID_FREE, ///< attempt to release a resource that wasn't allocated
    FL_DOUBLE_FREE,  ///< attempt to release a resource already released
    FL_FAIL,         ///< an assertion or other test condition failed
    FL_BAD_STATE,    ///< an internal error has occurred
} FLResultCode;

static inline char const *faultline_result_code_to_string(FLResultCode code) {
    switch (code) {
    case FL_NOT_RUN:
        return "Not Run";
    case FL_PASS:
        return "Pass";
    case FL_LEAK:
        return "Leak Detected";
    case FL_INVALID_FREE:
        return "Invalid Free";
    case FL_DOUBLE_FREE:
        return "Double Free";
    case FL_FAIL:
        return "Fail";
    case FL_BAD_STATE:
        return "Bad State";
    default:
        return "Unknown Result Code";
    }
}

#if defined(__cplusplus)
}
#endif

#endif // FL_RESULT_CODES_H_
