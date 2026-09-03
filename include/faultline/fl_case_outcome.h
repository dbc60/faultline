#ifndef FL_CASE_OUTCOME_H_
#define FL_CASE_OUTCOME_H_

/**
 * @file fl_case_outcome.h
 * @author Douglas Cuthbertson
 * @brief The result one test case reports across the module boundary.
 * @version 0.1
 * @date 2026-09-02
 *
 * A suite catches its own exceptions and reports what happened in an FLCaseOutcome, so
 * the module boundary carries a value rather than an unwind. Each module keeps its
 * unwinding mechanism to itself, which is what lets one driver run both a suite built
 * with the setjmp backend and a suite built with the C++ one. A driver that instead
 * wrapped its calls into the suite in its own FL_TRY would oblige both images to share
 * one mechanism, since it is the driver's jump buffer the suite lands in.
 *
 * details is an array, not a pointer. FL_DETAILS points into the throwing module's
 * per-thread scratch buffer (fl_details_buf, fl_try.h), which the next
 * detail-formatting throw overwrites, so a pointer would oblige the driver to copy the
 * text before its next call into the suite. Copying it here instead means nothing that
 * crosses the boundary outlives the call that produced it.
 *
 * reason and file stay pointers: both are string literals in the suite, valid for as
 * long as it is loaded.
 *
 * elapsed_seconds covers the test body alone: setup and cleanup fall outside the
 * measured region, so the number means the same thing whether or not a case has them.
 * The suite reads the timer service to measure it, so a suite links
 * fla_timer_service.c and its host injects a timer provider.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_exception_types.h> // FLExceptionReason, FL_MAX_DETAILS_LENGTH
#include <faultline/fl_macros.h>          // FL_SPEC_EXPORT, FL_STR, FL_MAYBE_UNUSED
#include <faultline/fl_types.h>           // FLFailureType

#include <stddef.h> // NULL, size_t
#include <stdio.h>  // snprintf

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Whether a test case passed, and if not, which kind of failure it was.
 *
 * A driver reports FL_CASE_EXPECTED_FAILURE as a pass. It is distinguished from
 * FL_CASE_PASS because the suite knows the difference and the information is free;
 * the matching happens inside the suite, where the reason pointer and
 * fl_expected_failure are the same object, so it needs no cross-module strcmp.
 */
typedef enum FLCaseStatus {
    FL_CASE_PASS,
    FL_CASE_EXPECTED_FAILURE,
    FL_CASE_UNEXPECTED_FAILURE,
} FLCaseStatus;

/**
 * @brief Whether the call reached a test case at all.
 *
 * Separate from FLCaseStatus because they answer different questions: this one is
 * about the call, that one about the test. Folding them together would put a value in
 * FLCaseOutcome.status that can never appear there, and would report a driver asking
 * for a case that does not exist as a test case that failed.
 *
 * Shaped after FLAbiVerdict (fl_abi.h), including the string function below, for the
 * same reason: a caller gets a message naming the difference rather than a bare code.
 */
typedef enum FLRunCaseResult {
    FL_RUN_CASE_OK,           ///< a case ran; the outcome is filled in
    FL_RUN_CASE_BAD_OUTCOME,  ///< out is NULL or too small; nothing was written
    FL_RUN_CASE_NO_SUCH_CASE, ///< index is past the end; out is cleared, no case ran
} FLRunCaseResult;

static inline FL_MAYBE_UNUSED char const *fl_run_case_result_str(FLRunCaseResult result) {
    char const *text;
    switch (result) {
    case FL_RUN_CASE_OK:
        text = "ran";
        break;
    case FL_RUN_CASE_BAD_OUTCOME:
        text = "outcome buffer missing or too small";
        break;
    case FL_RUN_CASE_NO_SUCH_CASE:
        text = "no test case at that index";
        break;
    default:
        text = "unrecognized result";
        break;
    }
    return text;
}

/**
 * @brief What one test case reports back to the driver that ran it.
 */
typedef struct FLCaseOutcome {
    FLCaseStatus      status;
    FLFailureType     failure_type; ///< which phase failed, or FL_FAILURE_NONE
    FLExceptionReason reason;       ///< literal in the suite; valid while it is loaded
    char const       *file;         ///< same
    int               line;
    double            elapsed_seconds; ///< the test body only; see the file comment
    char details[FL_MAX_DETAILS_LENGTH]; ///< owned copy; see the file comment
} FLCaseOutcome;

/**
 * @brief Reset an outcome to a passing result with no failure recorded.
 */
static inline FL_MAYBE_UNUSED void fl_case_outcome_clear(FLCaseOutcome *out) {
    out->status          = FL_CASE_PASS;
    out->failure_type    = FL_FAILURE_NONE;
    out->reason          = NULL;
    out->file            = NULL;
    out->line            = 0;
    out->elapsed_seconds = 0.0;
    out->details[0]      = '\0';
}

/**
 * @brief Note that a phase ended in fl_expected_failure.
 *
 * An expected failure is not a failure, but it must not erase one already recorded:
 * a test body that fails and a cleanup that then throws fl_expected_failure has to
 * keep reporting the body's failure.
 */
static inline FL_MAYBE_UNUSED void fl_case_note_expected(FLCaseOutcome *out) {
    if (out->status != FL_CASE_UNEXPECTED_FAILURE) {
        out->status = FL_CASE_EXPECTED_FAILURE;
    }
}

/**
 * @brief Record a failure, copying details out of the caller's scratch buffer.
 */
static inline FL_MAYBE_UNUSED void fl_case_outcome_fail(FLCaseOutcome    *out,
                                                        FLCaseStatus      status,
                                                        FLFailureType     failure_type,
                                                        FLExceptionReason reason,
                                                        char const       *details,
                                                        char const *file, int line) {
    out->status       = status;
    out->failure_type = failure_type;
    out->reason       = reason;
    out->file         = file;
    out->line         = line;

    if (details != NULL) {
        snprintf(out->details, sizeof out->details, "%s", details);
    } else {
        out->details[0] = '\0';
    }
}

/**
 * @brief Run one test case and report what happened.
 *
 * @param index the test case to run, as an index into the suite's array.
 * @param out where to write the result. Cleared before the case runs.
 * @param out_size the caller's sizeof(FLCaseOutcome), so a suite built against a
 * different revision of the struct refuses the call rather than writing past the end
 * of it. Follows the same convention as fla_set_exception_service.
 * @return FL_RUN_CASE_OK when a case ran, a failing one included -- a failing test is
 * a result, not a rejected call. Otherwise the reason no case ran.
 *
 * FL_GET_TEST_SUITE emits the definition, so a suite gains it with no edit.
 *
 * The prototype below is what makes that definition safe to find by name from a C++
 * suite, for the same reason fla_get_abi needs one (fl_abi.h): linkage comes from the
 * first declaration a translation unit sees, so without this one inside an extern "C"
 * block, a C++ compile would give fl_run_case mangled linkage and
 * GetProcAddress(FL_RUN_CASE_STR) would stop finding it.
 */
#define FL_RUN_CASE_FN(name) \
    FLRunCaseResult name(size_t index, FLCaseOutcome *out, size_t out_size)
typedef FL_RUN_CASE_FN(fl_run_case_fn);
extern FL_SPEC_EXPORT fl_run_case_fn fl_run_case;
#define FL_RUN_CASE_STR FL_STR(fl_run_case)

#if defined(__cplusplus)
}
#endif

#endif // FL_CASE_OUTCOME_H_
