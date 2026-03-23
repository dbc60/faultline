/**
 * @file timer_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the timer library.
 * @version 0.1
 * @date 2026-02-21
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception_service.c"  // fl_test_exception and other exception reasons
#include "fla_exception_service.c" // app-side TLS exception service
#include "tick_timer.c"            // start_ticks, stop_ticks, elapsed_ticks
#include "time_timer.c"            // start, stop, elapsed_time
#include "win_timer.c" // start_win, stop_win, elapsed_win_ticks, elapsed_win_seconds

#include <faultline/fl_test.h> // FL_TYPE_TEST_SETUP_CLEANUP, FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h>  // FL_THROW_DETAILS

#include <faultline/fl_abbreviated_types.h> // u32, u64, i64
#include <faultline/fl_macros.h>            // FL_CONTAINER_OF

#include <time.h> // struct timespec

typedef struct TimeTest {
    FLTestCase tc;
    u32        sleep;
    double     epsilon;
} TimeTest;

static FL_SETUP_FN(setup_time_test) {
    TimeTest *tt = FL_CONTAINER_OF(tc, TimeTest, tc);
    tt->sleep    = NANOSECONDS_PER_SECOND / 10;
    tt->epsilon  = 0.001;
}

FL_TYPE_TEST_SETUP_CLEANUP("Time Timer", TimeTest, test_time_timer, setup_time_test,
                           fl_default_cleanup) {
    TimeTimer       timer;
    double const    sleep_duration = (double)t->sleep;
    struct timespec req            = {0, (long)t->sleep};

    start(&timer);
    int sleep_result = nanosleep(&req, NULL);
    if (sleep_result != 0) {
        FL_THROW_DETAILS(fl_test_exception, "nanosleep failed with %d", sleep_result);
    }
    stop(&timer);

    time_t elapsed = elapsed_time(&timer);
    if (elapsed > 1) {
        double const duration = sleep_duration / NANOSECONDS_PER_SECOND;
        FL_THROW_DETAILS(fl_test_exception, "elapsed time is %lld, duration %f", elapsed,
                         duration);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Windows Timer", TimeTest, test_win_timer, setup_time_test,
                           fl_default_cleanup) {
    WinTimer        timer;
    struct timespec req = {0, (long)t->sleep};

    start_win(&timer);
    int sleep_result = nanosleep(&req, NULL);
    if (sleep_result != 0) {
        FL_THROW_DETAILS(fl_test_exception, "nanosleep failed with %d", sleep_result);
    }
    stop_win(&timer);

    i64 elapsed = elapsed_win_ticks(&timer);
    if (elapsed < 1) {
        FL_THROW_DETAILS(fl_test_exception, "elapsed ticks is %lld", elapsed);
    }

    double       expected       = (double)t->sleep / NANOSECONDS_PER_SECOND;
    double       actual         = elapsed_win_seconds(&timer);
    double const sleep_duration = (double)t->sleep;
    double       upper_bound    = sleep_duration / NANOSECONDS_PER_SECOND + t->epsilon;
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
    int sleep_result = nanosleep(&req, NULL);
    if (sleep_result != 0) {
        FL_THROW_DETAILS(fl_test_exception, "nanosleep failed with %d", sleep_result);
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

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(test_time_timer)
FL_SUITE_ADD_EMBEDDED(test_win_timer)
FL_SUITE_ADD_EMBEDDED(test_tick_timer)
FL_SUITE_ADD(check_epoch)
FL_SUITE_END;

FL_GET_TEST_SUITE("Timer", ts);
