/**
 * @file timer_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the timer library.
 * @version 0.1
 * @date 2026-02-21
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception.c" // exception reasons, push/pop/throw
#include "fl_threads.c"   // thrd_sleep
#include "tick_timer.c"   // start_ticks, stop_ticks, elapsed_ticks
#include "time_timer.c"   // start, stop, elapsed_time
#include "win_timer.c" // get_100ns_intervals_between_epochs (check_epoch); QueryPerformanceCounter
#include "flp_timer_service.c" // flp_timer_now, flp_timer_elapsed_seconds, flp_init_timer_service
#include "fla_timer_service.c" // g_fla_timer_service, fla_set_timer_service

#include <faultline/fl_stopwatch.h> // FLStopwatch, FLTimerService, FLTimestamp
#include <faultline/fl_timer.h>     // FL_NOW, FL_ELAPSED (consumer-side selector)
#include <faultline/fl_test.h> // FL_TYPE_TEST_SETUP_CLEANUP, FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h> // FL_THROW_DETAILS

#include <faultline/fl_abbreviated_types.h> // u32, u64, i64
#include <faultline/fl_macros.h>            // FL_CONTAINER_OF

#include <time.h> // struct timespec

// The platform provider under test, surfaced as a service for the stopwatch and
// the contract tests below -- the same QPC clock the host installs at startup.
static FLTimerService const provider = {
    .now             = flp_timer_now,
    .elapsed_seconds = flp_timer_elapsed_seconds,
};

typedef struct TimeTest {
    FLTestCase tc;
    u32        sleep;
    double     epsilon;
} TimeTest;

static FL_SETUP_FN(setup_time_test) {
    TimeTest *tt = FL_CONTAINER_OF(tc, TimeTest, tc);
    tt->sleep    = NANOSECONDS_PER_SECOND / 10;
    tt->epsilon
        = 0.020; // 20ms slack: the OS bounds the floor of a sleep, not the ceiling
}

FL_TYPE_TEST_SETUP_CLEANUP("Time Timer", TimeTest, test_time_timer, setup_time_test,
                           fl_default_cleanup) {
    TimeTimer       timer;
    double const    sleep_duration = (double)t->sleep;
    struct timespec req            = {0, (long)t->sleep};

    start(&timer);
    int sleep_result = thrd_sleep(&req, NULL);
    if (sleep_result != 0) {
        FL_THROW_DETAILS(fl_test_exception, "thrd_sleep failed with %d", sleep_result);
    }
    stop(&timer);

    time_t elapsed = elapsed_time(&timer);
    if (elapsed > 1) {
        double const duration = sleep_duration / NANOSECONDS_PER_SECOND;
        FL_THROW_DETAILS(fl_test_exception, "elapsed time is %lld, duration %f", elapsed,
                         duration);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Stopwatch", TimeTest, test_stopwatch, setup_time_test,
                           fl_default_cleanup) {
    FLStopwatch     sw  = fl_stopwatch_make(&provider);
    struct timespec req = {0, (long)t->sleep};

    fl_stopwatch_start(&sw);
    int sleep_result = thrd_sleep(&req, NULL);
    if (sleep_result != 0) {
        FL_THROW_DETAILS(fl_test_exception, "thrd_sleep failed with %d", sleep_result);
    }
    fl_stopwatch_stop(&sw);

    double actual = fl_stopwatch_elapsed_seconds(&sw);
    if (actual <= 0.0) { // time must have advanced (replaces the elapsed-ticks check)
        FL_THROW_DETAILS(fl_test_exception, "elapsed seconds is %f", actual);
    }

    double expected    = (double)t->sleep / NANOSECONDS_PER_SECOND;
    double upper_bound = expected + t->epsilon;
    if (actual > upper_bound) {
        FL_THROW_DETAILS(fl_test_exception, "expected %f, actual %f, upper bound %f",
                         expected, actual, upper_bound);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Tick Timer", TimeTest, test_tick_timer, setup_time_test,
                           fl_default_cleanup) {
    TickTimer       timer;
    struct timespec req = {0, (long)t->sleep};

    start_ticks(&timer);
    int sleep_result = thrd_sleep(&req, NULL);
    if (sleep_result != 0) {
        FL_THROW_DETAILS(fl_test_exception, "thrd_sleep failed with %d", sleep_result);
    }
    stop_ticks(&timer);

    u64 elapsed = elapsed_ticks(&timer);
    if (elapsed < 1) {
        FL_THROW_DETAILS(fl_test_exception, "elapsed ticks is %llu", elapsed);
    }
}

FL_TEST("Epoch Interval", check_epoch) {
    u64 const expected = EPOCH_INTERVAL;
    u64       actual   = get_100ns_intervals_between_epochs();
    if (actual != expected) {
        FL_THROW_DETAILS("WAT?!", "expected %llu, actual %llu", expected, actual);
    }
}

// --- Platform timer provider (flp_timer_now / flp_timer_elapsed_seconds) ------

FL_TEST("Timer Now Monotonic", test_timer_now_monotonic) {
    FLTimestamp first  = flp_timer_now();
    FLTimestamp second = flp_timer_now();
    if (second < first) {
        FL_THROW_DETAILS(fl_test_exception,
                         "now() went backwards: first %llu, second %llu", (u64)first,
                         (u64)second);
    }
}

FL_TEST("Timer Elapsed Zero", test_timer_elapsed_zero) {
    FLTimestamp t       = flp_timer_now();
    double      elapsed = flp_timer_elapsed_seconds(t, t);
    if (elapsed != 0.0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "elapsed over identical samples is %f, expected 0", elapsed);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Timer Elapsed Over Sleep", TimeTest, test_timer_elapsed,
                           setup_time_test, fl_default_cleanup) {
    struct timespec req = {0, (long)t->sleep};

    FLTimestamp start        = flp_timer_now();
    int         sleep_result = thrd_sleep(&req, NULL);
    if (sleep_result != 0) {
        FL_THROW_DETAILS(fl_test_exception, "thrd_sleep failed with %d", sleep_result);
    }
    FLTimestamp end = flp_timer_now();

    double actual = flp_timer_elapsed_seconds(start, end);
    if (actual <= 0.0) { // the clock must have advanced across the sleep
        FL_THROW_DETAILS(fl_test_exception, "elapsed seconds is %f, expected > 0",
                         actual);
    }

    double expected    = (double)t->sleep / NANOSECONDS_PER_SECOND;
    double upper_bound = expected + t->epsilon;
    if (actual > upper_bound) {
        FL_THROW_DETAILS(fl_test_exception, "expected %f, actual %f, upper bound %f",
                         expected, actual, upper_bound);
    }
}

// Verify cross-boundary injection through the real driver path. When the driver
// loads this DLL it resolves the exported fla_set_timer_service via GetProcAddress
// and installs the *host's* timer provider into our g_fla_timer_service before any
// test runs. This test does NOT self-inject; it relies entirely on that wiring, so
// a pass proves the symbol was actually exported and the service crossed the DLL
// boundary. Were the export missing -- or the driver to skip it --
// g_fla_timer_service would still hold the default abort stubs.
//
// The host installs its own flp_timer_now, whose address differs from this DLL's
// copy, so we do not assert pointer identity against the local provider; we assert
// the abort stubs were replaced (they are static in the included fla_timer_service.c)
// and that the injected clock behaves.
FL_TEST("Timer Service Cross-Boundary Injection", test_timer_cross_boundary) {
    if (g_fla_timer_service.now == default_now
        || g_fla_timer_service.elapsed_seconds == default_elapsed_seconds) {
        FL_THROW_DETAILS(
            fl_test_exception,
            "driver did not inject the timer service across the DLL boundary - "
            "g_fla_timer_service still holds the default abort stubs");
    }

    FLTimestamp start = FL_NOW();
    FLTimestamp end   = FL_NOW();
    if (end < start) {
        FL_THROW_DETAILS(fl_test_exception,
                         "injected FL_NOW went backwards: first %llu, second %llu",
                         (u64)start, (u64)end);
    }
    double secs = FL_ELAPSED(start, end);
    if (secs < 0.0) {
        FL_THROW_DETAILS(fl_test_exception, "FL_ELAPSED returned negative: %f", secs);
    }
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(test_time_timer)
FL_SUITE_ADD_EMBEDDED(test_stopwatch)
FL_SUITE_ADD_EMBEDDED(test_tick_timer)
FL_SUITE_ADD_EMBEDDED(test_timer_elapsed)
FL_SUITE_ADD(test_timer_now_monotonic)
FL_SUITE_ADD(test_timer_elapsed_zero)
FL_SUITE_ADD(test_timer_cross_boundary)
FL_SUITE_ADD(check_epoch)
FL_SUITE_END;

FL_GET_TEST_SUITE("Timer", ts);
