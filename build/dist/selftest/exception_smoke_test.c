/**
 * @file exception_smoke_test.c
 * @author Douglas Cuthbertson
 * @brief Smoke test that the imported `exceptions` package compiles and runs as a
 *        single FL_EMBEDDED binary.
 *
 * The in-driver suites already cover the exception implementation's functional
 * surface; this test deliberately stays minimal. Its purpose is to prove the
 * exceptions package is self-sufficient as a single binary: that
 * FL_TRY / FL_THROW / FL_CATCH / FL_RETHROW link and behave correctly with no
 * driver and nothing injected.
 *
 * fl_exception.c carries the whole implementation, so the binary needs no other
 * source: fl_push/fl_pop/fl_throw run over its own thread-local stack.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty. Exit code: 0 = pass, 1 = failure.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include "fl_selftest.h"

#include <faultline/fl_try.h> // FL_TRY/CATCH/THROW/RETHROW
#include <faultline/fl_exception.h>
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
