/**
 * @file timer_consumer_test.c
 * @author Douglas Cuthbertson
 * @brief Consumer-side test for the timer service, compiled against an *imported*
 *        timer package layered over its exceptions dependency (not the
 *        repo's src/ tree).
 *
 * Compiled WITHOUT FL_PLATFORM_BUILD, so fl_timer.h resolves FL_NOW / FL_ELAPSED /
 * FL_TIMER_SERVICE to the consumer side (g_fla_timer_service). The test performs
 * the real injection dance -- flp_init_timer_service hands the provider's service
 * to fla_set_timer_service -- then verifies the consumer macros and the
 * FLStopwatch composition read the injected clock.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty (no dllimport on a locally-defined
 * function). Use /DDLL_BUILD instead when compiling into an actual DLL.
 *
 * Exit code: 0 = all passed, 1 = one or more failures.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_selftest.h"

#include <faultline/fl_stopwatch.h>      // FLStopwatch, fl_stopwatch_*
#include <faultline/fl_timer.h>          // FL_NOW, FL_ELAPSED, FL_TIMER_SERVICE
#include <faultline/fl_timer_service.h>  // FLTimestamp
#include <faultline/fla_timer_service.h> // fla_set_timer_service, g_fla_timer_service
#include <flp_timer_service.h> // flp_init_timer_service, flp_timer_now, flp_timer_elapsed_seconds

int main(void) {
    fprintf(stdout, "timer_consumer_test (imported package)\n");

    SECTION("service injection");
    flp_init_timer_service(fla_set_timer_service);
    CHECK(g_fla_timer_service.now == flp_timer_now);
    CHECK(g_fla_timer_service.elapsed_seconds == flp_timer_elapsed_seconds);

    SECTION("now is monotonic");
    {
        FLTimestamp t1 = FL_NOW();
        FLTimestamp t2 = FL_NOW();
        CHECK(t2 >= t1);
        CHECK(FL_ELAPSED(t1, t1) == 0.0);
        CHECK(FL_ELAPSED(t1, t2) >= 0.0);
    }

    SECTION("stopwatch over the injected clock");
    {
        FLStopwatch sw = fl_stopwatch_make(FL_TIMER_SERVICE());

        // A made-but-never-started watch reads 0 elapsed.
        CHECK(fl_stopwatch_elapsed_seconds(&sw) == 0.0);

        fl_stopwatch_start(&sw);
        // Burn enough work that the elapsed time exceeds the counter resolution.
        unsigned volatile spin = 0;
        for (unsigned i = 0; i < 1000000u; i++) {
            spin += i;
        }
        fl_stopwatch_stop(&sw);
        CHECK(fl_stopwatch_elapsed_seconds(&sw) > 0.0);

        // A started-but-not-stopped watch reads 0 elapsed, and peek advances.
        fl_stopwatch_start(&sw);
        CHECK(fl_stopwatch_elapsed_seconds(&sw) == 0.0);
        CHECK(fl_stopwatch_peek_seconds(&sw) >= 0.0);
    }

    if (g_failures == 0) {
        fprintf(stdout, "PASS (timer service checks)\n");
    } else {
        fprintf(stderr, "%d check(s) FAILED.\n", g_failures);
    }

    return g_failures > 0 ? 1 : 0;
}
