#ifndef FL_CONTEXT_H_
#define FL_CONTEXT_H_

/**
 * @file faultline_context.h
 * @author Douglas Cuthbertson
 * @brief context definitions
 * @version 0.2.0
 * @date 2025-01-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fault_injector.h>              // for FaultInjector
#include <faultline/fl_result_codes.h>             // for FLResultCode
#include <faultline/fl_test_summary.h>             // for FLTestSummary, fault...
#include <faultline/fl_types.h>                    // for FLFailureType, Fault...
#include <faultline/fl_exception_service_assert.h> // for FL_ASSERT_NOT_NULL
#include <faultline/fl_exception_types.h>          // for FLExceptionReason
#include <faultline/fl_test.h>                     // for FLTestSuite
#include <stdbool.h>                               // for bool, true, false
#include <string.h>                                // for size_t, memset
#include <time.h>                                  // for time_t, time
#include <faultline/timer.h>                       // for WinTimer
#include <faultline/arena.h>                       // for Arena
#include <faultline/buffer.h>                      // for buffer_clear, DEFINE_TYPED_...
#include <faultline/fault_site.h>                  // for fault_site_buffer_count
#include <faultline/fl_abbreviated_types.h>        // for i64, u32

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct Arena Arena;

/**
 * @brief Define WinTimerBuffer, a typed buffer object. This macro defines the following
 * functions:
 *
 *  init_clock_timer_buffer(WinTimerBuffer *buf, size_t capacity, void *mem)
 *  new_clock_timer_buffer(size_t capacity)
 *  destroy_clock_timer_buffer(WinTimerBuffer *buf)
 *  clock_timer_buffer_put(WinTimerBuffer *buf, WinTimer const *item)
 *  clock_timer_buffer_get(WinTimerBuffer *buf, size_t index)
 *  clock_timer_buffer_count(WinTimerBuffer *buf)
 *  clock_timer_buffer_clear(WinTimerBuffer *buf)
 *  clock_timer_buffer_clear_and_release(WinTimerBuffer *buf)
 *  clock_timer_buffer_allocate_next_free_slot(WinTimerBuffer *buf)
 *  clock_timer_buffer_copy(WinTimerBuffer *dst, WinTimerBuffer *src)
 */
DEFINE_TYPED_BUFFER(WinTimer, clock_timer)

typedef struct ElapsedTime {
    i64    index;   ///< index of the test case
    double seconds; ///< elapsed time in seconds
} ElapsedTime;

static inline void elapsed_time_init(ElapsedTime *elapsed, i64 index, double seconds) {
    elapsed->index   = index;
    elapsed->seconds = seconds;
}

/**
 * @brief Define ElapsedTimeBuffer, a typed buffer object. This macro defines the
 * following functions:
 *
 *  init_elapsed_time_buffer(ElapsedTimeBuffer *buf, size_t capacity, void *mem)
 *  new_elapsed_time_buffer(size_t capacity)
 *  destroy_elapsed_time_buffer(ElapsedTimeBuffer *buf)
 *  elapsed_time_buffer_put(ElapsedTimeBuffer *buf, ElapsedTime const *item)
 *  elapsed_time_buffer_get(ElapsedTimeBuffer *buf, size_t index)
 *  elapsed_time_buffer_count(ElapsedTimeBuffer *buf)
 *  elapsed_time_buffer_clear(ElapsedTimeBuffer *buf)
 *  elapsed_time_buffer_clear_and_release(ElapsedTimeBuffer *buf)
 *  elapsed_time_buffer_allocate_next_free_slot(ElapsedTimeBuffer *buf)
 *  elapsed_time_buffer_copy(ElapsedTimeBuffer *dst, ElapsedTimeBuffer *src)
 */
DEFINE_TYPED_BUFFER(ElapsedTime, elapsed_time)

/**
 * @brief Fault injection testing context.
 *
 * This structure holds the state for running fault injection tests, including
 * the current test case, results, and fault injector state.
 */
typedef struct {
    Arena       *arena;
    FLTestSuite *ts;                      ///< the test suite under test
    size_t       count;                   ///< the number of test cases in the suite
    size_t       index;                   ///< index of the current test case
    size_t       tests_run;               ///< number of tests run
    size_t       tests_passed;            ///< number of tests that passed
    size_t       tests_passed_with_leaks; ///< number of tests that passed with leaks
    size_t       tests_failed;            ///< number of tests that ran and failed
    size_t       setups_failed;           ///< number of tests that failed setup
    size_t       cleanups_failed;         ///< tests that failed cleanup
    time_t       run_start_time;          ///< UTC time when testing starts
    ElapsedTimeBuffer
        elapsed_times;            ///< a buffer of elapsed times, one for each test case
    FLTestSummaryBuffer results;  ///< a resizable buffer of test results
    FaultInjector      *injector; ///< the FaultInjector for the test suite
    FaultSiteBuffer     fault_sites; ///< the FaultSites collected for each test case
    bool                initialized; ///< whether the context has been initialized
} FLContext;

typedef struct TestResult TestResult;

/**
 * @brief define a signature for a function that initializes a FLContext with a
 * test suite, an exception handler, and an arena for memory management and fault
 * injection.
 *
 */
#define FL_INITIALIZE(name) void name(FLContext *fctx, FLTestSuite *ts, Arena *arena)
typedef FL_INITIALIZE(faultline_initialize_fn);

/**
 * @brief initialize a test context with the given test suite.
 *
 * @param fctx the address of a test context to be initialized.
 * @param handler an optional exception handler to be set for the context.
 */
static inline FL_INITIALIZE(faultline_initialize) {
    memset(fctx, 0, sizeof *fctx);

    fctx->arena = arena;
    fctx->ts    = ts;
    init_faultline_test_summary_buffer(&fctx->results, fctx->arena, 0);
    init_elapsed_time_buffer(&fctx->elapsed_times, fctx->arena, 0);
    init_fault_site_buffer(&fctx->fault_sites, fctx->arena, 0);
    fctx->initialized = true;
}

#define FL_IS_VALID(name) bool name(FLContext const *fctx)
typedef FL_IS_VALID(faultline_is_valid_fn);

/**
 * @brief determine if the context has been properly initialized by calls to
 * faultline_initialize and faultline_begin.
 *
 * @param fctx a test context
 * @return true if the test context is valid and false otherwise.
 */
static inline FL_IS_VALID(faultline_is_valid) {
    bool valid = false;

    if (fctx) {
        if (fctx->initialized && fctx->index < fctx->count) {
            valid = true;
        }
    }

    return valid;
}

#define FL_BEGIN(name) void name(FLContext *fctx, FLTestSuite *const test_suite)
typedef FL_BEGIN(faultline_begin_fn);

/**
 * @brief faultline_begin assigns a test suite to a new test context.
 *
 * @param fctx a test context.
 * @param bts a test suite.
 */
static inline FL_BEGIN(faultline_begin) {
    fctx->ts    = test_suite;
    fctx->count = test_suite->count;
    time(&fctx->run_start_time);
}

#define FL_END(name) void name(FLContext *fctx)
typedef FL_END(faultline_end_fn);

/**
 * @brief Clear test results and reset all counters to prepare for the next test suite.
 *
 * This function resets all test execution counters and clears result buffers, allowing
 * the context to be reused for subsequent test suite runs without accumulating state.
 *
 * @param fctx a test context
 */
static inline FL_END(faultline_end) {
    // Clear result buffers
    buffer_clear(&fctx->results);
    buffer_clear(&fctx->elapsed_times);
    buffer_clear(&fctx->fault_sites);

    // Reset all test counters
    fctx->index                   = 0;
    fctx->tests_run               = 0;
    fctx->tests_passed            = 0;
    fctx->tests_passed_with_leaks = 0;
    fctx->tests_failed            = 0;
    fctx->setups_failed           = 0;
    fctx->cleanups_failed         = 0;
    fctx->run_start_time          = 0;

    // Note: ts and count are intentionally NOT reset here, as they may be
    // queried after faultline_end() is called for reporting purposes
}

#define FL_ADD_FAULT(name)                                                      \
    void name(FLContext *fctx, size_t index, FLResultCode code, void *resource, \
              FLExceptionReason reason, char const *details, char const *file,  \
              int line)
typedef FL_ADD_FAULT(faultline_add_fault_fn);

/**
 * @brief add a fault to the result buffer.
 *
 * @param fctx a test context.
 * @param index the index of the fault in the test case.
 * @param code the result code for the fault.
 * @param resource an optional resource to associate with the fault
 * @param reason a string representing the reason for the fault.
 * @param file the file in which the exception was thrown.
 * @param line the line on which the exception was thrown.
 */
static inline FL_ADD_FAULT(faultline_add_fault) {
    FL_ASSERT_NOT_NULL(reason);
    FLTestSummary *summary
        = faultline_test_summary_buffer_allocate_next_free_slot(&fctx->results);
    init_faultline_test_summary(summary, fctx->arena, (u32)index, code, reason, details);
    faultline_test_summary_add_fault(summary, index, code, resource, reason, details,
                                     file, line);
}

#define FL_ADD_TEST_FAILURE(name)                               \
    void name(FLContext *fctx, size_t index, FLResultCode code, \
              FLExceptionReason reason, char const *details)
typedef FL_ADD_TEST_FAILURE(faultline_add_test_failure_fn);

static inline FL_ADD_TEST_FAILURE(faultline_add_test_failure) {
    FLTestSummary *summary
        = faultline_test_summary_buffer_allocate_next_free_slot(&fctx->results);
    init_faultline_test_summary(summary, fctx->arena, (u32)index, code, reason, details);
}

#define FL_NEXT(name) void name(FLContext *fctx)
typedef FL_NEXT(faultline_next_fn);

/**
 * @brief faultline_next is part of the iterator interface. If there is another test case
 * in the test suite, the context is updated to hold that test case as the current one.
 * If there are no more test cases, the context is unchanged.
 *
 * @param fctx a test context.
 */
static inline FL_NEXT(faultline_next) {
    if (fctx->index < fctx->count) {
        fctx->index++;
    }
}

#define FL_HAS_MORE(name) bool name(FLContext const *fctx)
typedef FL_HAS_MORE(faultline_has_more_fn);

/**
 * @brief faultline_has_more is part of the iterator interface. It determines if there is
 * another test case in the test suite,
 *
 * @param fctx a test context.
 *
 * @return true if there are more test cases after the current one, and false otherwise.
 */
static inline FL_HAS_MORE(faultline_has_more) {
    return fctx->index < fctx->count;
}

#define FL_GET_CASE_NAME(name) char const *name(FLContext const *fctx)
typedef FL_GET_CASE_NAME(faultline_get_case_name_fn);

/**
 * @brief faultline_get_case_name retrieves the name of the current test case.
 *
 * @param fctx a test context.
 * @param bts a test suite.
 *
 * @return the name of the current test case.
 * @throw but_internal_error if index is out of range
 */
extern FL_GET_CASE_NAME(faultline_get_case_name);

#define FL_GET_RESULT_CODE(name) FLResultCode name(FLContext *fctx, size_t index)
typedef FL_GET_RESULT_CODE(faultline_get_result_code_fn);

/**
 * @brief retrieve the result code from a result context.
 *
 * @param fctx a test context.
 * @param index the index of a result context.
 * @return a test-result code.
 */
extern FL_GET_RESULT_CODE(faultline_get_result_code);

#define FL_GET_INDEX(name) size_t name(FLContext const *fctx)
typedef FL_GET_INDEX(faultline_get_index_fn);

/**
 * @brief retrieve the index of the current test case.
 *
 * @param fctx a test context.
 * @return the zero-based index of the test case.
 */
static inline FL_GET_INDEX(faultline_get_index) {
    return fctx->index;
}

#define FL_GET_RUN_COUNT(name) size_t name(FLContext const *fctx)
typedef FL_GET_RUN_COUNT(faultline_get_run_count_fn);

/**
 * @brief retrieve the number of test cases executed.
 *
 * @param fctx a test context.
 * @return the number of test cases executed.
 */
static inline FL_GET_RUN_COUNT(faultline_get_run_count) {
    return fctx->tests_run;
}

#define FL_GET_PASS_COUNT(name) size_t name(FLContext const *fctx)
typedef FL_GET_PASS_COUNT(faultline_get_pass_count_fn);

/**
 * @brief retrieve the number of tests that passed.
 *
 * tests_failed is now incremented at most once per test case, so undeflow of
 * (fctx->count - failures) is unlikely, but the check (failures >= fctx->count) ensures
 * the pass count will always report a reasonable value.
 *
 * @param fctx a test context.
 * @return  the number of tests that passed.
 */
static inline FL_GET_PASS_COUNT(faultline_get_pass_count) {
    size_t failures = fctx->tests_failed + fctx->setups_failed + fctx->cleanups_failed;
    return failures >= fctx->count ? 0 : fctx->count - failures;
}

#define FL_GET_FAIL_COUNT(name) size_t name(FLContext const *fctx)
typedef FL_GET_FAIL_COUNT(faultline_get_fail_count_fn);

/**
 * @brief retrieve the number of tests that failed.
 *
 * @param fctx a test context
 * @return the number of tests that failed.
 */
static inline FL_GET_FAIL_COUNT(faultline_get_fail_count) {
    return fctx->tests_failed;
}

#define FL_GET_SETUP_FAIL_COUNT(name) size_t name(FLContext const *fctx)
typedef FL_GET_SETUP_FAIL_COUNT(faultline_get_setup_fail_count_fn);

/**
 * @brief retrieve the number of tests where the setup failed.
 *
 * @param fctx a test context
 * @return the number of tests where the setup failed.
 */
static inline FL_GET_SETUP_FAIL_COUNT(faultline_get_setup_fail_count) {
    return fctx->setups_failed;
}

#define FL_GET_CLEANUP_FAIL_COUNT(name) size_t name(FLContext const *fctx)
typedef FL_GET_CLEANUP_FAIL_COUNT(faultline_get_cleanup_fail_count_fn);

/**
 * @brief retrieve the number of tests that failed in the cleanup phase.
 *
 * @param fctx a test context.
 * @return the number of tests that failed in the cleanup phase.
 */
static inline FL_GET_CLEANUP_FAIL_COUNT(faultline_get_cleanup_fail_count) {
    return fctx->cleanups_failed;
}

#define FL_GET_RESULTS_COUNT(name) size_t name(FLContext const *fctx)
typedef FL_GET_RESULTS_COUNT(faultline_get_results_count_fn);

/**
 * @brief retrieve the number of result contexts.
 *
 * @param fctx a test context.
 * @return the number of result contexts (size_t).
 */
static inline FL_GET_RESULTS_COUNT(faultline_get_results_count) {
    return (size_t)faultline_test_summary_buffer_count(&fctx->results);
}

#define FL_GET_FAULT_SITE_COUNT(name) size_t name(FLContext const *fctx)
typedef FL_GET_FAULT_SITE_COUNT(faultline_get_fault_site_count_fn);

/**
 * @brief Return the number of fault sites encountered in the test suite.
 *
 * @param fctx a test context
 * @return the number of fault sites (size_t)
 */
static inline FL_GET_FAULT_SITE_COUNT(faultline_get_fault_site_count) {
    return fault_site_buffer_count(&fctx->fault_sites);
}

/**
 * @brief Helper function to determine failure type from result code and context
 *
 * @param result_code the FLResultCode from test execution
 * @param is_setup true if failure occurred during setup phase
 * @param is_cleanup true if failure occurred during cleanup phase
 * @return FLFailureType corresponding to the failure
 */
static inline FLFailureType faultline_categorize_failure_type(FLResultCode result_code,
                                                              bool         is_setup,
                                                              bool         is_cleanup) {
    switch (result_code) {
    case FL_PASS:
        return FL_FAILURE_NONE;
    case FL_LEAK:
        return FL_LEAK_FAILURE;
    case FL_INVALID_FREE:
    case FL_DOUBLE_FREE:
        return FL_INVALID_FREE_FAILURE;
    case FL_FAIL:
    case FL_BAD_STATE:
        if (is_setup) {
            return FL_SETUP_FAILURE;
        } else if (is_cleanup) {
            return FL_CLEANUP_FAILURE;
        } else {
            return FL_TEST_FAILURE;
        }
    default:
        return FL_TEST_FAILURE; // Default to test failure for unknown codes
    }
}

/**
 * @brief Helper function to update test summary with phase and failure information
 *
 * @param summary the FLTestSummary to update
 * @param phase the phase during which this result was recorded
 * @param failure_type the type of failure that occurred
 * @param phase_time the time spent in this phase (seconds)
 */
static inline void faultline_update_summary_phase_info(FLTestSummary *summary,
                                                       FLTestPhase    phase,
                                                       FLFailureType  failure_type,
                                                       double         phase_time) {
    // Update phase information if this represents a failure
    if (failure_type != FL_FAILURE_NONE) {
        summary->failure_phase = phase;
        summary->failure_type  = failure_type;
    }

    // Update phase timing
    if (phase == FL_DISCOVERY_PHASE) {
        summary->discovery_time += phase_time;
        if (failure_type != FL_FAILURE_NONE) {
            summary->discovery_failures++;
        }
    } else if (phase == FL_INJECTION_PHASE) {
        summary->injection_time += phase_time;
        if (failure_type != FL_FAILURE_NONE) {
            summary->injection_failures++;
        }
    }
}

void faultline_record_injection_failure(FLTestSummary *summary, i64 fault_index,
                                        FLResultCode  result_code,
                                        FLFailureType failure_type,
                                        TestResult *test_result, double phase_time,
                                        char const *synthetic_reason,
                                        char const *details, char const *fallback_file,
                                        int fallback_line);

/**
 * @brief Validate that a TestResult contains complete failure information when it should
 *
 * This function only validates TestResults that represent actual failures (unexpected
 * exceptions). Successful tests with no exceptions don't need failure details and will
 * return true.
 *
 * This is a pure validation function with no side effects - callers are responsible for
 * any logging or error handling based on the return value.
 *
 * @param test_result the TestResult to validate
 * @return true if test_result is valid (either successful or complete failure info),
 * false if incomplete failure
 */
bool faultline_validate_test_result(TestResult *test_result);

#if defined(__cplusplus)
}
#endif

#endif // FL_CONTEXT_H_
