#ifndef FL_TIMER_SERVICE_H_
#define FL_TIMER_SERVICE_H_

/**
 * @file fl_timer_service.h
 * @author Douglas Cuthbertson
 * @brief Monotonic timing service provided by the platform layer.
 * @version 0.1
 * @date 2026-06-18
 *
 * The core measures durations through this service. A timestamp is an
 * opaque monotonic sample: the caller captures two samples and asks the platform for the
 * seconds between.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_abbreviated_types.h> // u64
#include <faultline/fl_macros.h>            // FL_STR
#include <stddef.h>                         // size_t

#if defined(__cplusplus)
extern "C" {
#endif

// Opaque monotonic sample. On Windows this holds a QueryPerformanceCounter tick;
// the caller never interprets it, only passes it back to elapsed_seconds().
typedef u64 FLTimestamp;

#define FL_TIMER_NOW_FN(name) FLTimestamp name(void)
typedef FL_TIMER_NOW_FN(fl_timer_now_fn);

#define FL_TIMER_ELAPSED_FN(name) double name(FLTimestamp start, FLTimestamp end)
typedef FL_TIMER_ELAPSED_FN(fl_timer_elapsed_fn);

typedef struct FLTimerService {
    fl_timer_now_fn     *now;             // capture a monotonic sample
    fl_timer_elapsed_fn *elapsed_seconds; // seconds between two samples
} FLTimerService;

// Symmetry with the other services, so a consumer that wants timing can be injected
// the same clock the core uses.
#define FLA_SET_TIMER_SERVICE_FN(name) void name(FLTimerService *const svc, size_t size)
typedef FLA_SET_TIMER_SERVICE_FN(fla_set_timer_service_fn);
#define FLA_SET_TIMER_SERVICE_STR FL_STR(fla_set_timer_service)

#if defined(__cplusplus)
}
#endif

#endif // FL_TIMER_SERVICE_H_
