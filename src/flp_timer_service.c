/**
 * @file flp_timer_service.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-06-18
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_timer_service.h> // FLTimerService
#include <faultline/fl_try.h> // FL_ASSERT_REASON_IMPL used by FL_ASSERT_NOT_NULL
#include <faultline/fl_exception_assert.h> // FL_ASSERT_NOT_NULL
#include <faultline/fl_exception_types.h>  // FLExceptionReason
#include <flp_timer_service.h> // FLP_INIT_TIMER_SERVICE_FN, flp_init_timer_service

#include <errhandlingapi.h> // GetLastError
#include <minwindef.h>      // DWORD
#include <profileapi.h>     // QueryPerformanceCounter, QueryPerformanceFrequency

static FLExceptionReason flp_timer_query_frequency_failure
    = "failed to query performance counter frequency";

static double flp_seconds_per_tick(void) {
    static double s = 0.0; // frequency is fixed at boot; query once
    if (s == 0.0) {
        LARGE_INTEGER f;
        if (!QueryPerformanceFrequency(&f) || f.QuadPart == 0) {
            DWORD error = GetLastError();
            FL_THROW_DETAILS(flp_timer_query_frequency_failure, "error %lu", error);
        }
        s = 1.0 / (double)f.QuadPart;
    }
    return s;
}

FL_TIMER_NOW_FN(flp_timer_now) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (FLTimestamp)t.QuadPart;
}

FL_TIMER_ELAPSED_FN(flp_timer_elapsed_seconds) {
    return (double)((i64)end - (i64)start) * flp_seconds_per_tick();
}

static FLTimerService g_timer_service = {
    .now             = flp_timer_now,
    .elapsed_seconds = flp_timer_elapsed_seconds,
};

FLTimerService const *flp_timer_service(void) {
    return &g_timer_service;
}

FLP_INIT_TIMER_SERVICE_FN(flp_init_timer_service) {
    FL_ASSERT_NOT_NULL(fla_set);
    fla_set(&g_timer_service, sizeof g_timer_service);
}
