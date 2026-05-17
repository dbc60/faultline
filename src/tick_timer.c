/**
 * @file tick_timer.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-03-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/timer.h>                // for TickTimer, elapsed_tick_seconds
#include <faultline/fl_abbreviated_types.h> // for u64

void start_ticks(TickTimer *t) {
    t->start = __rdtsc();
    t->stop  = t->start;
}

void stop_ticks(TickTimer *t) {
    t->stop = __rdtsc();
}

u64 elapsed_ticks(TickTimer *t) {
    return t->stop - t->start;
}

double elapsed_tick_seconds(TickTimer *t) {
    return (double)(t->stop - t->start) / 1000000000LL;
}
