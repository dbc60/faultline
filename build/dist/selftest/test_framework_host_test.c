/**
 * @file test_framework_host_test.c
 * @author Douglas Cuthbertson
 * @brief The host half of the `test_framework` package self-test.
 * @date 2026-08-16
 *
 * Proves the imported test_framework package is sufficient to build both sides of
 * the boundary it describes. This binary is the host: it loads the suite module
 * built from test_framework_suite.c, reads the module's build identity, injects
 * the exception service, enumerates the suite, and runs its cases -- the sequence
 * a real driver performs, reduced to what the package itself ships.
 *
 * It is built /DFL_PLATFORM_BUILD, so fl_try.h selects the platform-side macros
 * over flp_exception_service.c's own TLS stack, and flp_init_exception_service
 * hands that same stack to the module through the module's fla_set_exception_service
 * export. The module's throw therefore unwinds to an FL_TRY frame in this file,
 * which is the arrangement the exception environment layout in FLAbiInfo exists to
 * protect.
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
#include <faultline/fl_exception_service.h> // fl_expected_failure, FLA_SET_EXCEPTION_SERVICE_STR
#include <faultline/fl_test.h> // FLTestSuite, fl_get_test_suite_fn, FL_GET_TEST_SUITE_STR
#include <faultline/fl_try.h> // FL_TRY/FL_CATCH_STR (platform side under FL_PLATFORM_BUILD)
#include <faultline/fl_timer_service.h> // fla_set_timer_service_fn, FLA_SET_TIMER_SERVICE_STR
#include <flp_exception_service.h> // flp_init_exception_service
#include <flp_timer_service.h>     // flp_init_timer_service

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
        CHECK(mod.crt_id == host.crt_id);
        CHECK(mod.sizeof_exception_env == host.sizeof_exception_env);
        CHECK(fl_abi_check(&host, &mod) == FL_ABI_OK);
    }

    SECTION("service injection");
    fla_set_exception_service_fn *fla_set_exc
        = (fla_set_exception_service_fn *)GetProcAddress(suite,
                                                         FLA_SET_EXCEPTION_SERVICE_STR);
    CHECK(fla_set_exc != NULL);
    if (fla_set_exc != NULL) {
        flp_init_exception_service(fla_set_exc);
    }

    /* fl_run_case times the test body through the module's timer service, so a host
     * that leaves it uninjected leaves the module holding fla_timer_service.c's abort
     * stubs. */
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

    FLTestSuite *ts = (get_suite != NULL) ? get_suite() : NULL;
    CHECK(ts != NULL);
    if (ts != NULL && fla_set_exc != NULL) {
        CHECK(strcmp(ts->name, "test_framework_selftest") == 0);
        CHECK(ts->count == 3);

        SECTION("running the cases");
        int expected_failures = 0;
        for (size_t i = 0; i < ts->count; ++i) {
            FLTestCase *tc = ts->test_cases[i];
            CHECK(tc != NULL);
            if (tc == NULL) {
                continue;
            }
            CHECK(tc->name != NULL);
            CHECK(tc->setup != NULL && tc->test != NULL && tc->cleanup != NULL);

            /* The module throws a reason constant it owns its own copy of, so match
             * it by text rather than by address. */
            FL_TRY {
                tc->setup(tc);
                tc->test(tc);
                tc->cleanup(tc);
            }
            FL_CATCH_STR(fl_expected_failure) {
                expected_failures++;
                tc->cleanup(tc);
            }
            FL_END_TRY;
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
