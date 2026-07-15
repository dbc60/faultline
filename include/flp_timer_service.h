#ifndef FLP_TIMER_SERVICE_H_
#define FLP_TIMER_SERVICE_H_

/**
 * @file flp_timer_service.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-06-21
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_timer_service.h> // FL_TIMER_NOW_FN, FL_TIMER_ELAPSED_FN,
                                        // fla_set_timer_service_fn

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Connect the platform and consumer implementations of the timer service.
 */
#define FLP_INIT_TIMER_SERVICE_FN(name) void name(fla_set_timer_service_fn *fla_set)
typedef FLP_INIT_TIMER_SERVICE_FN(flp_init_timer_service_fn);
FLP_INIT_TIMER_SERVICE_FN(flp_init_timer_service);

/**
 * @brief The platform's timer service instance -- the same one flp_init_timer_service
 * injects into consumers. Lets platform-side code bind an FLStopwatch to the active
 * clock (via FL_TIMER_SERVICE in fl_timer.h) instead of re-composing over the bare
 * functions below.
 */
FLTimerService const *flp_timer_service(void);

/**
 * @brief Platform-side implementation to get the current time as an FLTimestamp object.
 */
FL_TIMER_NOW_FN(flp_timer_now);

/**
 * @brief Platform-side implementation to calculate the time elapsed between @start and
 * @end
 *
 * @param start the start time
 * @param end the end time
 */
FL_TIMER_ELAPSED_FN(flp_timer_elapsed_seconds);

#if defined(__cplusplus)
}
#endif

#endif // FLP_TIMER_SERVICE_H_
