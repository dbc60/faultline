#ifndef FL_TIMER_H_
#define FL_TIMER_H_

/**
 * @file fl_timer.h
 * @brief Unified timing macros that work in both platform and application code.
 *
 * This header is the single definition site for FL_NOW / FL_ELAPSED. It selects
 * the timer-service backend -- platform (flp_timer_now / flp_timer_elapsed_seconds)
 * or application (g_fla_timer_service) -- by whether FL_PLATFORM_BUILD is defined,
 * then defines the timing macros once over that backend. A translation unit gets
 * exactly one backend, and the call-site names never carry a platform/application
 * prefix: the side is fixed per TU by the build, not per call.
 *
 * For start/stop/peek semantics, bind an FLStopwatch (fl_stopwatch.h) to a service
 * pointer directly -- the host holds one as platform->timer.
 */
#include <faultline/fl_timer_service.h> // FLTimestamp, FLTimerService

#if defined(FL_PLATFORM_BUILD)
#include <flp_timer_service.h> // IWYU pragma: export -- flp_timer_now, flp_timer_elapsed_seconds
#define FL_NOW()               flp_timer_now()
#define FL_ELAPSED(start, end) flp_timer_elapsed_seconds((start), (end))
#else                                    // application / DLL build
#include <faultline/fla_timer_service.h> // IWYU pragma: export -- g_fla_timer_service
#define FL_NOW()               (g_fla_timer_service.now())
#define FL_ELAPSED(start, end) (g_fla_timer_service.elapsed_seconds((start), (end)))
#endif // FL_PLATFORM_BUILD

#endif // FL_TIMER_H_
