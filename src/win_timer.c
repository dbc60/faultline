/**
 * @file win_timer.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-01-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <errhandlingapi.h>               // for GetLastError
#include <faultline/fl_exception_types.h> // for FLExceptionReason
#include <faultline/fl_try.h>             // for FL_THROW, FL_THROW_DETAILS
#include <handleapi.h>                    // for CloseHandle
#include <minwinbase.h>                   // for SYSTEMTIME
#include <minwindef.h>                    // for _ULARGE_INTEGER::(anonymous), DWORD
#include <profileapi.h>                   // for QueryPerformanceCounter, QueryPer...
#include <synchapi.h>                     // for SetWaitableTimer, WaitForSingleOb...
#include <time.h>                         // for NULL, timespec
#include <faultline/timer.h>              // for WinTimer, NANOSECONDS_PER_SECOND
#include <timezoneapi.h>                  // for SystemTimeToFileTime
#include <winbase.h>                      // for INFINITE
#include <windows.h>
// IWYU pragma: no_include "fla_exception_service.h"
// IWYU pragma: no_include <flp_exception_service.h>
// IWYU pragma: no_include <pthread_time.h>
#include <faultline/fl_abbreviated_types.h> // for i64, u64

struct timespec;

// convert nanoseconds to 100 ns intervals
#define WIN32_TIMER_RESOLUTION_NS 100

static FLExceptionReason create_timer_failure = "failed to create waitable timer";
static FLExceptionReason set_timer_failure    = "failed to set waitable timer";

void start_win(WinTimer *t) {
    QueryPerformanceCounter(&t->start);
    t->stop = t->start; // in case stop_win is never called, at least this field is
                        // initialized
}

void stop_win(WinTimer *t) {
    QueryPerformanceCounter(&t->stop);
}

i64 elapsed_win_ticks(WinTimer *t) {
    return (t->stop.QuadPart - t->start.QuadPart);
}

double elapsed_win_seconds(WinTimer *t) {
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) {
        FL_THROW(create_timer_failure);
    }
    return (double)(t->stop.QuadPart - t->start.QuadPart) / frequency.QuadPart;
}

/**
 * @brief the execution of the calling thread until at least the time specified in
 * duration.
 *
 * This implementation can't be interrupted, so if rem is not NULL rem's fields will be
 * set to zero.
 *
 * This is a pretty good way to make a thread sleep with sub-millisecond accuracy.
 *
 * @param duration the time in seconds and nanoseconds to sleep.
 * @param rem the remaining time to sleep after this function returns.
 * @return 0
 * @throw create_timer_failure if the timer can't be created, or set_timer_failure if a
 * timer can't be set.
 */
int nanosleep(const struct timespec *duration, struct timespec *rem) {
    HANDLE        h;        // WinTimer handle
    LARGE_INTEGER li;       // Time definition
    LONGLONG      intervals // the number of 100-nanosecond intervals
        = duration->tv_sec * NANOSECONDS_PER_SECOND / WIN32_TIMER_RESOLUTION_NS
          + duration->tv_nsec / WIN32_TIMER_RESOLUTION_NS;

    h = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                              TIMER_ALL_ACCESS);
    if (h == NULL) {
        DWORD error = GetLastError();
        FL_THROW_DETAILS(create_timer_failure, "error %d", error);
    }

    // Set timer properties, negative values represent a relative time
    li.QuadPart = -intervals;
    if (!SetWaitableTimer(h, &li, 0, NULL, NULL, FALSE)) {
        CloseHandle(h);
        DWORD error = GetLastError();
        FL_THROW_DETAILS(set_timer_failure, "error %d", error);
    }

    // Start & wait for the timer
    (void)WaitForSingleObject(h, INFINITE);
    CloseHandle(h);

    if (rem != NULL) {
        rem->tv_sec  = 0;
        rem->tv_nsec = 0;
    }
    return 0; // success
}

#if defined(_MSC_VER)
int gettimeofday(struct timeval *tp, struct timezone *tzp) {
    // EPOCH_INTERVAL is the number of 100 nanosecond intervals from January 1, 1601
    // until 00:00:00 January 1, 1970
    static u64 const epochs_intervals = EPOCH_INTERVAL;

    SYSTEMTIME system_time;
    FILETIME   file_time;
    u64        time;

    FL_UNUSED(tzp);

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((u64)file_time.dwLowDateTime);
    time += ((u64)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long)((time - epochs_intervals) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
    return 0;
}
#endif // _MSC_VER

u64 get_100ns_intervals_between_epochs(void) {
    SYSTEMTIME windows_epoch = {1601, 1, 1, 1, 0, 0, 0, 0}; // Jan 1, 1607 was a Monday
    SYSTEMTIME unix_epoch    = {1970, 1, 4, 1, 0, 0, 0, 0}; // Jan 1, 1970 was a Thursday
    FILETIME   windows_epoch_ft;
    FILETIME   unix_epoch_ft;
    ULARGE_INTEGER nsecs_windows_epoch;
    ULARGE_INTEGER nsecs_unix_epoch;

    SystemTimeToFileTime(&windows_epoch, &windows_epoch_ft);
    SystemTimeToFileTime(&unix_epoch, &unix_epoch_ft);

    nsecs_windows_epoch.LowPart  = windows_epoch_ft.dwLowDateTime;
    nsecs_windows_epoch.HighPart = windows_epoch_ft.dwHighDateTime;
    nsecs_unix_epoch.LowPart     = unix_epoch_ft.dwLowDateTime;
    nsecs_unix_epoch.HighPart    = unix_epoch_ft.dwHighDateTime;
    nsecs_unix_epoch.QuadPart -= nsecs_windows_epoch.QuadPart;

    // The answer is, and always will be 116,444,736,000,000,000.
    return nsecs_unix_epoch.QuadPart;
}
