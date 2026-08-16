#ifndef FL_THREADS_H_
#define FL_THREADS_H_

/**
 * @file fl_threads.h
 * @author Douglas Cuthbertson
 * @brief C11 threads portability header.
 * @version 0.1
 * @date 2026-02-19
 *
 * Exposes the C11 <threads.h> mutex and thread API regardless of toolchain. When
 * the toolchain provides <threads.h> this includes it directly; when it does not
 * (e.g., MinGW with the MSVCRT runtime), a compatibility shim declared here and
 * implemented in src/fl_threads.c supplies the same names. The shim is partial: it
 * covers only the commonly used mutex and thread subset, not all of <threads.h>.
 *
 * Selection is by capability, not age: the shim activates whenever <threads.h> is
 * absent (detected via __has_include / __STDC_NO_THREADS__), which includes current
 * toolchains that simply omit C11 threads. See FL_THREADS_USE_SHIM below.
 *
 * Shim backends:
 *   Windows  (_WIN32) : CRITICAL_SECTION + CreateThread
 *   POSIX    (Unix / macOS / FreeBSD) : pthread_mutex_t + pthread_create
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

/*
 * FL_THREADS_USE_SHIM is 1 when this header supplies the C11 names itself, and 0 when it
 * forwards to the toolchain's <threads.h>. This header and fl_threads.c both test it, so
 * the two cannot disagree about which side they are on.
 *
 * Prefer __has_include over __STDC_NO_THREADS__ alone: some MinGW configurations
 * omit threads.h without defining __STDC_NO_THREADS__. The branches are kept
 * separate so a toolchain lacking __has_include never has to parse an
 * __has_include expression.
 *
 * FL_THREADS_USE_SHIM's value is decided only by detecting whether <threads.h>
 * exists. There is deliberately no macro to force the shim on where it does exist.
 *
 * The shim defines the same function names the C runtime defines -- mtx_init,
 * thrd_create, and the rest -- so on a toolchain that ships <threads.h>, the shim
 * and that runtime can each satisfy those names at link time. A program that
 * includes this header with the shim selected but does not link fl_threads.c takes
 * its declarations from the shim (mtx_t is a CRITICAL_SECTION, 40 bytes; thrd_t is
 * a HANDLE, 8) and its definitions from the C runtime (32 and 16 bytes). The
 * linker reports nothing, and the disagreement corrupts memory at run time.
 *
 * A macro that forced the shim on would therefore need a link-time guard: some
 * symbol that fails the build when fl_threads.c is absent from the link. Until
 * such a guard exists, no forcing macro is offered.
 */
#if defined(__STDC_NO_THREADS__) || !defined(__has_include)
#define FL_THREADS_USE_SHIM 1
#elif __has_include(<threads.h>)
#define FL_THREADS_USE_SHIM 0
#else
#define FL_THREADS_USE_SHIM 1
#endif

#if !FL_THREADS_USE_SHIM
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

#include <time.h> // struct timespec (for thrd_sleep)

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

/* --- tss_t (C11 subset) ---
 * Thread-specific storage with a per-key destructor that runs at thread exit.
 * Windows backend: fibre-local storage (FlsAlloc), whose callback fires on
 * thread exit even for threads created with CreateThread. POSIX backend:
 * pthread_key_create. */
typedef void (*tss_dtor_t)(void *);

#if defined(_WIN32) || defined(WIN32)
typedef unsigned long tss_t;
#else
typedef pthread_key_t tss_t;
#endif

int   tss_create(tss_t *key, tss_dtor_t dtor);
void *tss_get(tss_t key);
int   tss_set(tss_t key, void *val);
void  tss_delete(tss_t key);

/* --- thrd_sleep (C11) ---
 * Provided by <threads.h> on conforming platforms; declared here only for the
 * shim. Follows the C11 contract: 0 on success, -1 if interrupted by a
 * signal, a value < -1 on any other error. NB: unlike the old win_timer.c
 * nanosleep, this does NOT throw -- a portability primitive must match the
 * stdlib thrd_sleep it substitutes for. */
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);

#if defined(__cplusplus)
}
#endif

#endif /* compatibility shim */

#endif /* FL_THREADS_H_ */
