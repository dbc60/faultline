#ifndef FAULTLINE_TEST_SUMMARY_H_
#define FAULTLINE_TEST_SUMMARY_H_

/**
 * @file faultline_test_summary.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Test Summary - Public API
 * @version 0.2.0
 * @date 2025-07-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This header defines structures and functions for storing and accessing test execution
 * summaries, including failure information, timing data, and fault injection results.
 */
#include <faultline/buffer.h>          // Buffer API
#include <faultline/timer.h>           // WinTimer
#include <faultline/fault.h>           // Fault
#include <faultline/fl_result_codes.h> // FLResultCode
#include <faultline/fl_types.h>        // FLTestPhase, FLFailureType

#include <faultline/fl_exception_types.h>   // FLExceptionReason
#include <faultline/fl_abbreviated_types.h> // u32

#include <stddef.h> // size_t

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief A summary of test case results.
 *
 * The summary includes the overall result code for the test case as well as a collection
 * of results for each injected fault that wasn't handled as expected.
 *
 */
typedef struct FLTestSummary {
    u32               index;  ///< index of the test case
    FLResultCode      code;   ///< result code on the main path
    FLExceptionReason reason; ///< exception thrown on the main path
    char const   *details; ///< any details about the reason why an exception was thrown
    double        elapsed_seconds;  ///< the number of seconds the test case took to run
    i64           faults_exercised; ///< the number of faults exercised
    FaultBuffer   fault_buffer;     ///< the set of faults detected in the test case
    FLTestPhase   failure_phase;    ///< phase during which the primary failure occurred
    FLFailureType failure_type;     ///< type of failure that occurred
    double        discovery_time;   ///< time spent in discovery phase (seconds)
    double        injection_time;   ///< time spent in fault injection phases (seconds)
    u32           discovery_failures; ///< number of failures during discovery phase
    u32           injection_failures; ///< number of failures during injection phases
} FLTestSummary;

/**
 * @brief Define FLTestSummaryBuffer, a typed buffer object. This macro defines
 * the following functions:
 *
 *  init_faultline_test_summary_buffer(FLTestSummaryBuffer *buf, size_t capacity,
 *  new_faultline_test_summary_buffer(size_t capacity)
 *  destroy_faultline_test_summary_buffer(FLTestSummaryBuffer *buf)
 * void *mem) faultline_test_summary_buffer_put(FLTestSummaryBuffer *buf,
 * FLTestSummary const *item)
 *  faultline_test_summary_buffer_get(FLTestSummaryBuffer *buf, size_t index)
 *  faultline_test_summary_buffer_count(FLTestSummaryBuffer *buf)
 *  faultline_test_summary_buffer_clear(FLTestSummaryBuffer *buf)
 *  faultline_test_summary_buffer_clear_and_release(FLTestSummaryBuffer *buf)
 *  faultline_test_summary_buffer_allocate_next_free_slot(FLTestSummaryBuffer
 * *buf) faultline_test_summary_buffer_copy(FLTestSummaryBuffer *dst,
 * FLTestSummaryBuffer *src)
 */
DEFINE_TYPED_BUFFER(FLTestSummary, faultline_test_summary)

/**
 * @brief initialize a new FLTestSummary object
 *
 * @param summary a test summary to be initialized
 * @param index the index of the test case
 * @param status the result code for the test case
 * @param reason the exception thrown on the main path
 * @param details optional details about a thrown exception
 */
static inline void init_faultline_test_summary(FLTestSummary *summary, Arena *arena,
                                               u32 index, FLResultCode status,
                                               FLExceptionReason reason,
                                               char const       *details) {
    summary->index            = index;
    summary->code             = status;
    summary->reason           = reason;
    summary->details          = details ? details : "";
    summary->elapsed_seconds  = 0.0;
    summary->faults_exercised = 0;
    init_fault_buffer(&summary->fault_buffer, arena, 0);
    summary->failure_phase  = FL_DISCOVERY_PHASE; // All tests start in discovery phase
    summary->failure_type   = FL_FAILURE_NONE;    // Assume no failure initially
    summary->discovery_time = 0.0;
    summary->injection_time = 0.0;
    summary->discovery_failures = 0;
    summary->injection_failures = 0;
}

/**
 * @brief add a fault result to the summary
 *
 * @param summary the summary for the current test case
 * @param index the index of the fault
 * @param code the result code for the fault
 * @param reason the exception thrown on the fault
 * @param resource an optional resource to associate with the fault
 * @param file the file in which the exception was thrown
 * @param line the line on which the exception was thrown
 */
static inline void
faultline_test_summary_add_fault(FLTestSummary *summary, i64 index, FLResultCode code,
                                 void const *resource, FLExceptionReason reason,
                                 char const *details, char const *file, int line) {
    Fault *fault = fault_buffer_allocate_next_free_slot(&summary->fault_buffer);
    init_fault(fault, index, code, resource, reason, details, file, line);
}

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_TEST_SUMMARY_H_
