/**
 * @file exception_smoke_test.c
 * @author Douglas Cuthbertson
 * @brief Smoke test that the imported `exception` package compiles and runs as a
 *        single FL_EMBEDDED binary.
 *
 * The in-driver suites already cover the exception engine's functional surface;
 * this test deliberately stays minimal. Its purpose is to prove the
 * exception_service package is self-sufficient as a single binary: that
 * FL_TRY / FL_THROW / FL_CATCH / FL_RETHROW link and behave correctly with no
 * driver and no service injection.
 *
 * It is built /DFL_BUILD_DRIVER, so fl_try.h selects the platform-side macros
 * (flp_push/pop/throw over flp_exception_service.c's own TLS stack), which are
 * self-contained. The application-side (fla_*) service is the opposite case: it
 * is a set of abort stubs awaiting injection and is not runnable standalone.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty. Exit code: 0 = pass, 1 = failure.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include "fl_selftest.h"

#include <faultline/fl_try.h> // FL_TRY/CATCH/THROW/RETHROW (platform side under FL_BUILD_DRIVER)
#include <faultline/fl_exception_service.h>
#include <faultline/fl_exception_types.h> // FLExceptionReason

#include <stdbool.h>

static FLExceptionReason smoke_reason = "smoke exception";

int main(void) {
    fprintf(stdout, "exception_smoke_test (imported package)\n");

    /* Throw and catch by reason. */
    bool caught = false;
    FL_TRY {
        FL_THROW(smoke_reason);
    }
    FL_CATCH(smoke_reason) {
        caught = true;
    }
    FL_END_TRY;
    CHECK(caught);

    /* Rethrow from an inner catch-all, caught by an outer handler. */
    bool outer = false;
    FL_TRY {
        FL_TRY {
            FL_THROW(smoke_reason);
        }
        FL_CATCH_ALL {
            FL_RETHROW;
        }
        FL_END_TRY;
    }
    FL_CATCH(smoke_reason) {
        outer = true;
    }
    FL_END_TRY;
    CHECK(outer);

    if (g_failures == 0) {
        fprintf(stdout, "PASS (2 checks)\n");
        return 0;
    }
    fprintf(stderr, "FAIL (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
    return 1;
}
