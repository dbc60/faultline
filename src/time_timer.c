/**
 * @file time_timer.c
 * @author Douglas Cuthbertson
 * @brief a test timer
 * @version 0.1
 * @date 2024-12-20
 *
 * A simple timer that uses the standard C library function time() to take snapshots of
 * the current time.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/timer.h> // TimeTimer

#include <time.h> // time_t, time()

void start(TimeTimer *t) {
    t->start = time(0);
    t->stop  = t->start;
}

void stop(TimeTimer *t) {
    t->stop = time(0);
}

time_t elapsed_time(TimeTimer *t) {
    return t->stop - t->start;
}

double elapsed_time_seconds(TimeTimer *t) {
    return difftime(t->stop, t->start);
}
