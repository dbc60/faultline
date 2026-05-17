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
#include <faultline/fl_macros.h> // FL_UNUSED, FL_SPEC_EXPORT
#include <faultline/fl_try.h>    // FL_THROW

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
#define FL_TEST(NAME, TEST) static void TEST(void)

/**
 * @brief Define a test case with setup and cleanup functions.
 *
 * @param NAME The name of the test case as a string.
 * @param TEST The test function to run.
 * @param SETUP The setup function to run before the test.
 * @param CLEANUP The cleanup function to run after the test.
 */
#define FL_TEST_SETUP_CLEANUP(NAME, TEST, SETUP, CLEANUP) static void TEST(void)

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
#define FL_TYPE_TEST_SETUP_CLEANUP(NAME, TYPE, TEST, SETUP, CLEANUP) \
    static void TEST(TYPE *t)

#define FL_TYPE_TEST(NAME, TYPE, TEST) static void TEST(TYPE *t)

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
#define FL_VOID_TEST_SETUP_CLEANUP(NAME, TYPE, TEST, SETUP, CLEANUP) \
    static void TEST(void)

// helper macro for defining test suites
#define FL_PTR(X) (&(X).tc)

#define FL_TEST_SUITE_NAME(SUITE) SUITE##_ts

#define FL_TEST_SUITE(NAME, SUITE)                                       \
    static FLTestSuite SUITE##_ts                                        \
        = {.name       = NAME,                                           \
           .count      = sizeof SUITE##_cases / sizeof SUITE##_cases[0], \
           .test_cases = SUITE##_cases}

// Define suite with auto count
#define FL_GET_TEST_SUITE(NAME, SUITE)                                   \
    static FLTestSuite SUITE##_ts                                        \
        = {.name       = NAME,                                           \
           .count      = sizeof SUITE##_cases / sizeof SUITE##_cases[0], \
           .test_cases = SUITE##_cases};                                 \
    FL_SPEC_EXPORT FLTestSuite *fl_get_test_suite(void) {                \
        return &SUITE##_ts;                                              \
    }

// a macro to define a common field for test-case structs to embed a FLTestCase.
#define FL_EMBED_CASE FLTestCase fltc

// Auto-register test cases in a suite
#define FL_SUITE_BEGIN(NAME)      static FLTestCase *NAME##_cases[] = {
#define FL_SUITE_ADD(TC)          &TC##_case,
#define FL_SUITE_ADD_EMBEDDED(TC) &TC##_case.tc,
#define FL_SUITE_END              }

//////////////////////////////////////////////////////////////////
/////////////////// DEFINE TEST FUNCTIONS ////////////////////////
//////////////////////////////////////////////////////////////////
#define FL_TEST_UNUSED

// A test function runs a test case. If the test fails, it should throw an exception that
// describes the reason for the failure.
#define FL_TEST_FN(name) void name(void)
typedef FL_TEST_FN(fl_test_fn);

// A setup function is intended to initialize a test case by acquiring any necessary
// resources. If it fails to acquire those resources, it should throw an exception that
// describes the reason for the failure.
#define FL_SETUP_FN(name) void name(void)
typedef FL_SETUP_FN(fl_setup_fn);

// Cleanup is intended to release any resources acquired by the setup function.
#define FL_CLEANUP_FN(name) void name(void)
typedef FL_CLEANUP_FN(fl_cleanup_fn);

/**
 * @brief A test case has a name, a test function, an optional setup function, and an
 * optional cleanup function. The setup and cleanup function pointers must always be
 * non-NULL. Use fl_default_setup and fl_default_cleanup when no custom function is
 * needed.
 */
struct FLTestCase {
    char          *name;    ///< the name of the test case
    fl_test_fn    *test;    ///< the function that defines the test case
    fl_setup_fn   *setup;   ///< setup function. Use fl_default_setup if none needed
    fl_cleanup_fn *cleanup; ///< cleanup function. Use fl_default_cleanup if none needed
};

/**
 * @brief Default setup function (no-op). Use when no setup is needed.
 */
static inline FL_SETUP_FN(fl_default_setup) {
}

/**
 * @brief Default test function. Throws an exception to flag a missing test.
 */
static inline FL_TEST_FN(fl_default_test) {
    FL_THROW("no test function defined");
}

/**
 * @brief Default cleanup function (no-op). Use when no cleanup is needed.
 */
static inline FL_CLEANUP_FN(fl_default_cleanup) {
}

/**
 * @brief A test suite has a name and an array of test cases.
 */
struct FLTestSuite {
    char        *name;       ///< the name of the test suite
    size_t       count;      ///< the number of test cases in the test suite
    FLTestCase **test_cases; ///< an array of test cases.
};

/**
 * @brief fl_get_test_suite_fn is the signature of a function that retrieves the address
 * of a test suite.
 */
#define FL_GET_TEST_SUITE_FN(name) FLTestSuite *name(void)
typedef FL_GET_TEST_SUITE_FN(fl_get_test_suite_fn);

/**
 * @brief The function that returns the address of a FLTestSuite instance.
 */
extern FL_SPEC_EXPORT FL_GET_TEST_SUITE_FN(fl_get_test_suite);

#if defined(__cplusplus)
}
#endif

#endif // FL_TEST_H_
