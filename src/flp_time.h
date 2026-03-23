#ifndef FLP_TIME_H_
#define FLP_TIME_H_

/**
 * @file flp_time.h
 * @author Douglas Cuthbertson
 * @brief A generic timestamp function.
 * @version 0.1
 * @date 2026-03-13
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <time.h>

/**
 * @brief
 *
 * @param timer
 * @param result
 * @return struct tm*
 */
static inline struct tm *fl_gmtime(time_t const *timer, struct tm *result) {
#if defined(_MSC_VER) || defined(__clang__) && defined(_WIN32)
    gmtime_s(result, timer); // Microsoft: (result, time) — parameters reversed vs POSIX
    return result;
#else
    return gmtime_r(timer, result); // POSIX/MinGW: (time, result)
#endif
}

#endif // FLP_TIME_H_
