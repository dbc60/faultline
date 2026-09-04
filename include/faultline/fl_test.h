#ifndef FL_TEST_H_
#define FL_TEST_H_

/**
 * @file fl_test.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Fault Injection Testing Framework - Public API
 * @version 0.2.0
 * @date 2024-12-31
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * FaultLine extends the BUT (Basic Unit Test) framework with fault injection
 * capabilities, allowing systematic testing of error handling code paths.
 */
#include <faultline/fl_macros.h>       // FL_UNUSED, FL_SPEC_EXPORT
#include <faultline/fl_try.h>          // FL_TRY, FL_CATCH, FL_CATCH_ALL, FL_THROW
#include <faultline/fl_abi.h>          // FLA_GET_ABI_FN, fl_fill_abi_info
#include <faultline/fl_case_outcome.h> // FLCaseOutcome, FL_RUN_CASE_FN, fl_case_outcome_*
#include <faultline/fl_exception_service.h> // fl_expected_failure
#include <faultline/fl_timer.h>             // FL_NOW, FL_ELAPSED

#include <string.h> // strcmp

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Check if an exception is unexpected (not but_expected_failure).
 *
 * Uses strcmp() because the exception library is statically linked to both
 * executables (test drivers) and shared libraries (test suites). This means
 * the address of an exception thrown by a shared library and caught by a
 * test driver won't be the same, but their string values will be.
 *
 * @param e The exception reason to check.
 * @return Non-zero if exception is unexpected, zero if it's but_expected_failure.
 */
#define FL_UNEXPECTED_EXCEPTION(e) strcmp((e), fl_expected_failure)

// Forward declaration to suppress "warning C4115: 'FLTestCase': named type
// definition in parentheses"
typedef struct FLTestCase  FLTestCase;
typedef struct FLTestSuite FLTestSuite;

//////////////////////////////////////////////////////////////////
////////////////// MACROS TO DEFINE TEST CASES ///////////////////
//////////////////////////////////////////////////////////////////
/**
 * @brief Define a test case with a name and a test function.
 *
 * @param NAME The name of the test case as a string.
 * @param TEST The test function to run.
 */
#define FL_TEST(NAME, TEST)                      \
    static void TEST(void);                      \
    static void TEST##_wrapper(FLTestCase *tc) { \
        FL_UNUSED(tc);                           \
        TEST();                                  \
    }                                            \
    FLTestCase TEST##_case = {                   \
        .name    = NAME,                         \
        .test    = TEST##_wrapper,               \
        .setup   = fl_default_setup,             \
        .cleanup = fl_default_cleanup,           \
    };                                           \
    static void TEST(void)

/**
 * @brief Define a test case with setup and cleanup functions.
 *
 * @param NAME The name of the test case as a string.
 * @param TEST The test function to run.
 * @param SETUP The setup function to run before the test.
 * @param CLEANUP The cleanup function to run after the test.
 */
#define FL_TEST_SETUP_CLEANUP(NAME, TEST, SETUP, CLEANUP) \
    static void TEST(void);                               \
    static void TEST##_wrapper(FLTestCase *fltc) {        \
        FL_UNUSED(fltc);                                  \
        TEST();                                           \
    }                                                     \
    FLTestCase TEST##_case = {                            \
        .name    = (NAME),                                \
        .test    = TEST##_wrapper,                        \
        .setup   = (SETUP),                               \
        .cleanup = (CLEANUP),                             \
    };                                                    \
    static void TEST(void)

/**
 * @brief Macro to mark the test-case parameter in the test function as unused.
 */
#define FL_UNUSED_TYPE_ARG FL_UNUSED(t)

/**
 * @brief Define a test case derived from FLTestCase with setup and cleanup functions.
 *
 * @param NAME The name of the test case as a string.
 * @param TYPE The type of the test case.
 * @param TEST The test function to run.
 * @param SETUP The setup function to run before the test.
 * @param CLEANUP The cleanup function to run after the test.
 */
#define FL_TYPE_TEST_SETUP_CLEANUP(NAME, TYPE, TEST, SETUP, CLEANUP)                  \
    static void TEST(TYPE *t);                                                        \
    static void TEST##_wrapper(FLTestCase *fltc) {                                    \
        TYPE *t = FL_CONTAINER_OF(fltc, TYPE, tc);                                    \
        TEST(t);                                                                      \
    }                                                                                 \
    TYPE TEST##_case = {                                                              \
        .tc                                                                           \
        = {.name = NAME, .test = TEST##_wrapper, .setup = SETUP, .cleanup = CLEANUP}, \
    };                                                                                \
    static void TEST(TYPE *t)

#define FL_TYPE_TEST(NAME, TYPE, TEST)             \
    static void TEST(TYPE *t);                     \
    static void TEST##_wrapper(FLTestCase *fltc) { \
        TYPE *t = FL_CONTAINER_OF(fltc, TYPE, tc); \
        TEST(t);                                   \
    }                                              \
    TYPE TEST##_case = {                           \
        .tc = {.name    = NAME,                    \
               .test    = TEST##_wrapper,          \
               .setup   = fl_default_setup,        \
               .cleanup = fl_default_cleanup},     \
    };                                             \
    static void TEST(TYPE *t)

/**
 * @brief Sometimes setup and cleanup deal with side effects that the test doesn't use
 * directly, so this macro defines a test that takes no arguments, while allowing setup
 * and cleanup to accept a pointer to a struct with an embedded FLTestCase.
 *
 * @param NAME The name of the test case
 * @param TYPE a type (struct) with an embedded FLTestCase
 * @param TEST The test function to run.
 * @param SETUP The setup function to run before the test.
 * @param CLEANUP The cleanup function to run after the test.
 */
#define FL_VOID_TEST_SETUP_CLEANUP(NAME, TYPE, TEST, SETUP, CLEANUP)                  \
    static void TEST(void);                                                           \
    static void TEST##_wrapper(FLTestCase *fltc) {                                    \
        FL_UNUSED(fltc);                                                              \
        TEST();                                                                       \
    }                                                                                 \
    TYPE TEST##_case = {                                                              \
        .tc                                                                           \
        = {.name = NAME, .test = TEST##_wrapper, .setup = SETUP, .cleanup = CLEANUP}, \
    };                                                                                \
    static void TEST(void)

// helper macro for defining test suites
#define FL_PTR(X) (&(X).tc)

#define FL_TEST_SUITE_NAME(SUITE) SUITE##_ts

#define FL_TEST_SUITE(NAME, SUITE)                                       \
    static FLTestSuite SUITE##_ts                                        \
        = {.name       = NAME,                                           \
           .count      = sizeof SUITE##_cases / sizeof SUITE##_cases[0], \
           .test_cases = SUITE##_cases}

// Define suite with auto count.
//
// Also exports fla_get_abi, so every suite reports the toolchain with which it was built
// so the driver can detect an incompatible by name and report an error instead of
// crashing.
//
// And exports fl_run_case, which runs one case with its exceptions contained inside this
// module. That export is what lets a driver built as C run this suite whether the suite
// itself was built as C or C++: the try/catch below is compiled by the suite's own
// compiler, so it is setjmp here and a real catch there, and either way what crosses the
// boundary is an FLCaseOutcome rather than an unwind.
#define FL_GET_TEST_SUITE(NAME, SUITE)                                   \
    static FLTestSuite SUITE##_ts                                        \
        = {.name       = NAME,                                           \
           .count      = sizeof SUITE##_cases / sizeof SUITE##_cases[0], \
           .test_cases = SUITE##_cases};                                 \
    FL_SPEC_EXPORT FLTestSuite *fl_get_test_suite(void) {                \
        return &SUITE##_ts;                                              \
    }                                                                    \
    FL_SPEC_EXPORT FLA_GET_ABI_FN(fla_get_abi) {                         \
        fl_fill_abi_info(out);                                           \
    }                                                                    \
    FL_SPEC_EXPORT FL_RUN_CASE_FN(fl_run_case) {                         \
        return fl_run_case_impl(&SUITE##_ts, index, out, out_size);      \
    }

// a macro to define a common field for test-case structs to embed a FLTestCase.
#define FL_EMBED_CASE FLTestCase fltc

// Auto-register test cases in a suite
#define FL_SUITE_BEGIN(NAME)      static FLTestCase *NAME##_cases[] = {
#define FL_SUITE_ADD(TC)          &TC##_case,
#define FL_SUITE_ADD_EMBEDDED(TC) &TC##_case.tc,
#define FL_SUITE_END              }

// Forward-declare an externally-linked test-case object so a suite defined in a
// separate translation unit can reference it (non-unity builds). FL_TEST_DECL
// matches the FL_TEST / FL_TEST_SETUP_CLEANUP family (plain FLTestCase);
// FL_TYPE_TEST_DECL matches the FL_TYPE_TEST / *_SETUP_CLEANUP / FL_VOID_TEST
// family (a TYPE that embeds an FLTestCase named tc).
#define FL_TEST_DECL(TEST)            extern FLTestCase TEST##_case
#define FL_TYPE_TEST_DECL(TYPE, TEST) extern TYPE TEST##_case

//////////////////////////////////////////////////////////////////
/////////////////// DEFINE TEST FUNCTIONS ////////////////////////
//////////////////////////////////////////////////////////////////
#define FL_TEST_UNUSED FL_UNUSED(tc)

// A test function runs a test case. If the test fails, it should throw an exception that
// describes the reason for the failure.
#define FL_TEST_FN(name) void name(FLTestCase *tc)
typedef FL_TEST_FN(fl_test_fn);

// A setup function is intended to initialize a test case by acquiring any necessary
// resources. If it fails to acquire those resources, it should throw an exception that
// describes the reason for the failure.
#define FL_SETUP_FN(name) void name(FLTestCase *tc)
typedef FL_SETUP_FN(fl_setup_fn);

// Cleanup is intended to release any resources acquired by the setup function.
#define FL_CLEANUP_FN(name) void name(FLTestCase *tc)
typedef FL_CLEANUP_FN(fl_cleanup_fn);

/**
 * @brief A test case has a name, a test function, an optional setup function, and an
 * optional cleanup function. The setup and cleanup function pointers must always be
 * non-NULL. Use fl_default_setup and fl_default_cleanup when no custom function is
 * needed.
 */
struct FLTestCase {
    char const    *name;    ///< the name of the test case
    fl_test_fn    *test;    ///< the function that defines the test case
    fl_setup_fn   *setup;   ///< setup function. Use fl_default_setup if none needed
    fl_cleanup_fn *cleanup; ///< cleanup function. Use fl_default_cleanup if none needed
};

/**
 * @brief Default setup function (no-op). Use when no setup is needed.
 */
static inline FL_SETUP_FN(fl_default_setup) {
    FL_UNUSED(tc);
}

/**
 * @brief Default test function. Throws an exception to flag a missing test.
 */
static inline FL_TEST_FN(fl_default_test) {
    FL_UNUSED(tc);
    FL_THROW("no test function defined");
}

/**
 * @brief Default cleanup function (no-op). Use when no cleanup is needed.
 */
static inline FL_CLEANUP_FN(fl_default_cleanup) {
    FL_UNUSED(tc);
}

/**
 * @brief A test suite has a name and an array of test cases.
 */
struct FLTestSuite {
    char const  *name;       ///< the name of the test suite
    size_t       count;      ///< the number of test cases in the test suite
    FLTestCase **test_cases; ///< an array of test cases.
};

/**
 * @brief fl_get_test_suite_fn is the signature of a function that retrieves the address
 * of a test suite.
 */
#define FL_GET_TEST_SUITE_FN(name) FLTestSuite *name(void)
typedef FL_GET_TEST_SUITE_FN(fl_get_test_suite_fn);
#define FL_GET_TEST_SUITE_STR FL_STR(fl_get_test_suite)

/**
 * @brief The function that returns the address of a FLTestSuite instance.
 */
extern FL_SPEC_EXPORT FL_GET_TEST_SUITE_FN(fl_get_test_suite);

/**
 * @brief Run one test case, containing every exception it throws.
 *
 * FL_GET_TEST_SUITE's fl_run_case export is a one-line call to this function. The body
 * lives here rather than in the macro so it reads as ordinary C, and stays a static
 * inline so each module compiles its own copy against its own exception backend --
 * the same reason fl_fill_abi_info is one (fl_abi.h).
 *
 * Each phase gets its own FL_TRY, so an exception is contained to the phase that threw
 * it. A single FL_TRY spanning all three phases would behave differently: an
 * fl_expected_failure thrown by setup would skip the test body rather than let it run.
 *
 * Only the body is timed. start is read before FL_TRY rather than inside it because a
 * local written between setjmp and longjmp has an indeterminate value afterwards; read
 * before, it survives the throw. The cost of the setjmp itself lands inside the
 * measurement, which is a fixed few nanoseconds.
 *
 * @return FL_RUN_CASE_OK when a case ran, a failing one included. Otherwise the value
 * naming which argument was invalid: index, or out and out_size. Neither reports a
 * test failure.
 */
static inline FL_MAYBE_UNUSED FLRunCaseResult fl_run_case_impl(FLTestSuite   *ts,
                                                               size_t         index,
                                                               FLCaseOutcome *out,
                                                               size_t         out_size) {
    if (out == NULL || out_size < sizeof(FLCaseOutcome)) {
        return FL_RUN_CASE_BAD_OUTCOME;
    }

    fl_case_outcome_clear(out);

    // Not a test failure: no case exists at this index, so none ran. Reporting it as a
    // failed case would record a caller's bug as a bug in the suite.
    if (index >= ts->count) {
        return FL_RUN_CASE_NO_SUCH_CASE;
    }

    FLTestCase *tc = ts->test_cases[index];

    if (tc->setup != NULL) {
        FL_TRY {
            tc->setup(tc);
        }
        FL_CATCH(fl_expected_failure) {
            fl_case_note_expected(out);
        }
        FL_CATCH_ALL {
            fl_case_outcome_fail(out, FL_CASE_UNEXPECTED_FAILURE, FL_SETUP_FAILURE,
                                 FL_REASON, FL_DETAILS, FL_FILE, FL_LINE);
        }
        FL_END_TRY;

        // Setup acquired nothing, so there is nothing to exercise and nothing to
        // release. The case ran, and its outcome is the setup failure.
        if (out->status == FL_CASE_UNEXPECTED_FAILURE) {
            return FL_RUN_CASE_OK;
        }
    }

    FLTimestamp start = FL_NOW();

    FL_TRY {
        tc->test(tc);
    }
    FL_CATCH(fl_expected_failure) {
        fl_case_note_expected(out);
    }
    FL_CATCH_ALL {
        fl_case_outcome_fail(out, FL_CASE_UNEXPECTED_FAILURE, FL_TEST_FAILURE, FL_REASON,
                             FL_DETAILS, FL_FILE, FL_LINE);
    }
    FL_END_TRY;

    out->elapsed_seconds = FL_ELAPSED(start, FL_NOW());

    if (tc->cleanup != NULL) {
        FL_TRY {
            tc->cleanup(tc);
        }
        FL_CATCH(fl_expected_failure) {
            fl_case_note_expected(out);
        }
        FL_CATCH_ALL {
            // A cleanup failure does not overwrite one the body already reported.
            if (out->status != FL_CASE_UNEXPECTED_FAILURE) {
                fl_case_outcome_fail(out, FL_CASE_UNEXPECTED_FAILURE, FL_CLEANUP_FAILURE,
                                     FL_REASON, FL_DETAILS, FL_FILE, FL_LINE);
            }
        }
        FL_END_TRY;
    }

    return FL_RUN_CASE_OK;
}

#if defined(__cplusplus)
}
#endif

#endif // FL_TEST_H_
