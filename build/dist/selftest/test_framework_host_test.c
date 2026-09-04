/**
 * @file test_framework_host_test.c
 * @author Douglas Cuthbertson
 * @brief The host half of the `test_framework` package self-test.
 * @date 2026-08-16
 *
 * Proves the imported test_framework package is sufficient to build both sides of
 * the boundary it describes. This binary is the host: it loads the suite module
 * built from test_framework_suite.c, reads the module's build identity, injects the
 * services the module needs, enumerates the suite, and runs its cases through the
 * module's fl_run_case export -- the sequence a real driver performs, reduced to what
 * the package itself ships.
 *
 * The value of this boundary is that no exception crosses back into this file and this
 * host installs no throw hook of its own. Instead,  fl_run_case wraps each case in an
 * FL_TRY compiled into the module, so a throw is caught there and reported as an
 * FLCaseOutcome.
 *
 * It is built /DFL_PLATFORM_BUILD, so its own FL_ASSERT_* are compiled from the
 * implementation in flp_exception_service.c.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty.
 *
 * Usage: test_framework_host_test <suite-dll>
 * Exit code: 0 = pass, 1 = failure.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include "fl_selftest.h"

#include <faultline/fl_abi.h> // FLAbiInfo, fl_fill_abi_info, fl_abi_check, FLA_GET_ABI_STR
#include <faultline/fl_test.h> // FLTestSuite, fl_get_test_suite_fn, FL_GET_TEST_SUITE_STR
#include <faultline/fl_case_outcome.h> // FLCaseOutcome, fl_run_case_fn, FL_RUN_CASE_STR
#include <faultline/fl_timer_service.h> // fla_set_timer_service_fn, FLA_SET_TIMER_SERVICE_STR
#include <flp_timer_service.h>          // flp_init_timer_service

#include <stddef.h> // size_t
#include <string.h> // strcmp

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // LoadLibraryA, GetProcAddress, FreeLibrary, HMODULE

/** The module's step counter, resolved by name; see test_framework_suite.c. */
typedef int(tf_selftest_steps_fn)(void);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <suite-dll>\n", argv[0]);
        return 1;
    }

    fprintf(stdout, "test_framework_host_test (imported package)\n");

    HMODULE suite = LoadLibraryA(argv[1]);
    if (suite == NULL) {
        fprintf(stderr, "FAIL LoadLibraryA(\"%s\") error=%lu\n", argv[1],
                GetLastError());
        return 1;
    }

    SECTION("build identity");
    fla_get_abi_fn *get_abi = (fla_get_abi_fn *)GetProcAddress(suite, FLA_GET_ABI_STR);
    CHECK(get_abi != NULL);
    if (get_abi != NULL) {
        FLAbiInfo host = {0};
        FLAbiInfo mod  = {0};
        fl_fill_abi_info(&host);
        get_abi(&mod);
        CHECK(mod.magic == FL_ABI_MAGIC);
        CHECK(mod.struct_size == host.struct_size);
        CHECK(fl_abi_check(&host, &mod) == FL_ABI_OK);
    }

    SECTION("service injection");
    /* fl_run_case uses the module's timer service to time the test body, so a host that
     * doesn't inject a timer service leaves the module pointing to fla_timer_service.c's
     * abort stubs. */
    fla_set_timer_service_fn *fla_set_timer
        = (fla_set_timer_service_fn *)GetProcAddress(suite, FLA_SET_TIMER_SERVICE_STR);
    CHECK(fla_set_timer != NULL);
    if (fla_set_timer != NULL) {
        flp_init_timer_service(fla_set_timer);
    }

    SECTION("suite enumeration");
    fl_get_test_suite_fn *get_suite
        = (fl_get_test_suite_fn *)GetProcAddress(suite, FL_GET_TEST_SUITE_STR);
    CHECK(get_suite != NULL);

    fl_run_case_fn *run_case = (fl_run_case_fn *)GetProcAddress(suite, FL_RUN_CASE_STR);
    CHECK(run_case != NULL);

    FLTestSuite *ts = (get_suite != NULL) ? get_suite() : NULL;
    CHECK(ts != NULL);
    if (ts != NULL && run_case != NULL) {
        CHECK(strcmp(ts->name, "test_framework_selftest") == 0);
        CHECK(ts->count == 3);

        SECTION("running the cases");
        int expected_failures = 0;
        for (size_t i = 0; i < ts->count; ++i) {
            CHECK(ts->test_cases[i] != NULL && ts->test_cases[i]->name != NULL);

            FLCaseOutcome   out;
            FLRunCaseResult ran = run_case(i, &out, sizeof out);
            CHECK(ran == FL_RUN_CASE_OK);
            CHECK(out.status != FL_CASE_UNEXPECTED_FAILURE);
            if (out.status == FL_CASE_UNEXPECTED_FAILURE) {
                fprintf(stderr, "  %s: %s (%s:%d)\n", ts->test_cases[i]->name,
                        out.reason ? out.reason : "?", out.file ? out.file : "?",
                        out.line);
            }
            if (out.status == FL_CASE_EXPECTED_FAILURE) {
                expected_failures++;
            }
        }
        CHECK(expected_failures == 1);

        tf_selftest_steps_fn *steps
            = (tf_selftest_steps_fn *)GetProcAddress(suite, "tf_selftest_steps");
        CHECK(steps != NULL);
        if (steps != NULL) {
            CHECK(steps() == 5);
        }
    }

    FreeLibrary(suite);

    if (g_failures == 0) {
        fprintf(stdout, "PASS\n");
        return 0;
    }
    fprintf(stderr, "FAIL (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
    return 1;
}
