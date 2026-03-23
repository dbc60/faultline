#ifndef FL_THREADS_H_
#define FL_THREADS_H_

/**
 * @file fl_threads.h
 * @author Douglas Cuthbertson
 * @brief C11 threads portability header.
 * @version 0.1
 * @date 2026-02-19
 *
 * Uses <threads.h> directly when the platform provides it. When it does not
 * (e.g., MinGW with MSVCRT runtime), a minimal polyfill is declared here and
 * implemented in src/fl_threads.c. The polyfill covers only the mutex and
 * thread subset used by FaultLine.
 *
 * Supported platforms for the polyfill:
 *   Windows  (_WIN32) : CRITICAL_SECTION + CreateThread
 *   POSIX    (Unix / macOS / FreeBSD) : pthread_mutex_t + pthread_create
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

/*
 * Prefer __has_include over __STDC_NO_THREADS__ alone: some MinGW
 * configurations omit threads.h without defining __STDC_NO_THREADS__.
 */
#if !defined(__STDC_NO_THREADS__) && defined(__has_include) && __has_include(<threads.h>)
#include <threads.h>
#else

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <minwinbase.h> // for CRITICAL_SECTION
#include <minwindef.h>  // for HANDLE
#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <pthread.h>
#else
#error \
    "fl_threads.h: unsupported platform - no <threads.h> and no known mutex/thread API"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/* --- Result codes (C11 subset) --- */
enum {
    thrd_success  = 0,
    thrd_error    = 1,
    thrd_busy     = 2,
    thrd_timedout = 3,
    thrd_nomem    = 4,
};

/* --- Mutex type flags (C11 subset) --- */
enum {
    mtx_plain     = 0,
    mtx_recursive = 1,
    mtx_timed     = 2,
};

/* --- mtx_t --- */
#if defined(_WIN32) || defined(WIN32)
typedef CRITICAL_SECTION mtx_t;
#else
typedef pthread_mutex_t mtx_t;
#endif

int  mtx_init(mtx_t *mutex, int type);
int  mtx_lock(mtx_t *mutex);
int  mtx_unlock(mtx_t *mutex);
void mtx_destroy(mtx_t *mutex);

/* --- thrd_t --- */
typedef int (*thrd_start_t)(void *);

#if defined(_WIN32) || defined(WIN32)
typedef HANDLE thrd_t;
#else
typedef pthread_t thrd_t;
#endif

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
int thrd_join(thrd_t thr, int *res);

#if defined(__cplusplus)
}
#endif

#endif /* polyfill */

#endif /* FL_THREADS_H_ */
