/**
 * @file test_framework_suite.c
 * @author Douglas Cuthbertson
 * @brief The loadable module half of the `test_framework` package self-test.
 * @date 2026-08-16
 *
 * This is what the test_framework package exists to build: a suite compiled as a
 * DLL against the package headers alone, which a host loads and drives. It is
 * built /DDLL_BUILD and without FL_PLATFORM_BUILD, so fl_try.h selects the
 * consumer accessor and the exception service arrives by injection -- the mode a
 * real suite is compiled in, and the one a single-binary smoke test would never
 * exercise.
 *
 * FL_GET_TEST_SUITE emits both exports the host resolves: fl_get_test_suite, and
 * fla_get_abi for the build-identity check.
 *
 * tf_selftest_steps reports how far the host actually got. Each case bumps the
 * counter, so the host can distinguish "the suite enumerated" from "the suite
 * ran": one step for the plain case, three for the setup/cleanup case, and one
 * for the throwing case before it throws -- five in total.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include <faultline/fl_test.h>              // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_exception.h> // fl_expected_failure
#include <faultline/fl_macros.h>    // FL_SPEC_EXPORT, FL_UNUSED
#include <faultline/fl_try.h>       // FL_THROW

static int g_steps = 0;

/** @brief Report the number of steps the loaded cases have executed. */
FL_SPEC_EXPORT int tf_selftest_steps(void) {
    return g_steps;
}

FL_TEST("runs", tf_runs) {
    g_steps++;
}

static FL_SETUP_FN(tf_setup) {
    FL_UNUSED(tc);
    g_steps++;
}

static FL_CLEANUP_FN(tf_cleanup) {
    FL_UNUSED(tc);
    g_steps++;
}

FL_TEST_SETUP_CLEANUP("runs with setup and cleanup", tf_runs_wrapped, tf_setup,
                      tf_cleanup) {
    g_steps++;
}

/* The reason constant is linked into this module and into the host separately, so
 * the host must match it with FL_CATCH_STR rather than by pointer identity. */
FL_TEST("throws an expected failure", tf_throws) {
    g_steps++;
    FL_THROW(fl_expected_failure);
}

FL_SUITE_BEGIN(tf)
FL_SUITE_ADD(tf_runs)
FL_SUITE_ADD(tf_runs_wrapped)
FL_SUITE_ADD(tf_throws)
FL_SUITE_END;

FL_GET_TEST_SUITE("test_framework_selftest", tf)
