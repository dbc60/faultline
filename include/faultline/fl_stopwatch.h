#ifndef FL_STOPWATCH_H_
#define FL_STOPWATCH_H_

/**
 * @file fl_stopwatch.h
 * @author Douglas Cuthbertson
 * @brief A start/stop stopwatch composed over FLTimerService.
 * @version 0.1
 * @date 2026-06-18
 *
 * Pure composition: two timestamps plus the service that produced them. It
 * names no platform type, so it lives in the portable layer and works wherever
 * a timer service is injected. start/stop/elapsed are written once here, not
 * re-implemented per platform -- a port supplies only now()/elapsed_seconds().
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_timer_service.h> // FLTimerService, FLTimestamp

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct FLStopwatch {
    FLTimerService const *timer; // injected clock; bound at make()
    FLTimestamp           start;
    FLTimestamp           stop;
} FLStopwatch;

// Bind a stopwatch to a clock. Initialize inline:
//     FLStopwatch sw = fl_stopwatch_make(fctx->timer);
static inline FLStopwatch fl_stopwatch_make(FLTimerService const *timer) {
    FLStopwatch sw = {.timer = timer, .start = 0, .stop = 0};
    return sw;
}

static inline void fl_stopwatch_start(FLStopwatch *sw) {
    sw->start = sw->timer->now();
    sw->stop  = sw->start; // 0 elapsed (not garbage) if stop() is never called
}

static inline void fl_stopwatch_stop(FLStopwatch *sw) {
    sw->stop = sw->timer->now();
}

static inline double fl_stopwatch_elapsed_seconds(FLStopwatch const *sw) {
    return sw->timer->elapsed_seconds(sw->start, sw->stop);
}

// Live read: seconds since the last start(), without recording a stop.
static inline double fl_stopwatch_peek_seconds(FLStopwatch const *sw) {
    return sw->timer->elapsed_seconds(sw->start, sw->timer->now());
}

#if defined(__cplusplus)
}
#endif

#endif // FL_STOPWATCH_H_
