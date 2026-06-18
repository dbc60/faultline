/**
 * @file fl_threads.c
 * @author Douglas Cuthbertson
 * @brief Polyfill implementations for C11 <threads.h> mutex and thread functions.
 * @version 0.1
 * @date 2026-02-19
 *
 * Compiled only when <threads.h> is unavailable on the target platform.
 * Windows:  CRITICAL_SECTION for mutexes, CreateThread for threads.
 * POSIX:    pthread_mutex_t for mutexes, pthread_create for threads.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_threads.h>

#if defined(__STDC_NO_THREADS__) || !defined(__has_include) || !__has_include(<threads.h>)

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <handleapi.h>         // for CloseHandle
#include <processthreadsapi.h> // for CreateThread, GetExitCodeThread
#include <stdlib.h>            // for NULL, free, malloc
#include <synchapi.h>          // for DeleteCriticalSection, EnterCriticalS...
#include <time.h>              // for struct timespec
#include <winbase.h>           // for INFINITE, CreateWaitableTimerEx, SetWaitableTimer

/* --- Windows mutex --- */

int mtx_init(mtx_t *mutex, int type) {
    (void)type; /* CRITICAL_SECTION supports both plain and recursive use */
    InitializeCriticalSection(mutex);
    return thrd_success;
}

int mtx_lock(mtx_t *mutex) {
    EnterCriticalSection(mutex);
    return thrd_success;
}

int mtx_unlock(mtx_t *mutex) {
    LeaveCriticalSection(mutex);
    return thrd_success;
}

void mtx_destroy(mtx_t *mutex) {
    DeleteCriticalSection(mutex);
}

/* --- Windows threads --- */

typedef struct {
    thrd_start_t func;
    void        *arg;
} fl_thrd_param_;

static DWORD WINAPI fl_thrd_wrapper_(LPVOID param) {
    fl_thrd_param_ *p    = (fl_thrd_param_ *)param;
    thrd_start_t    func = p->func;
    void           *arg  = p->arg;
    free(p);
    return (DWORD)func(arg);
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    fl_thrd_param_ *p = (fl_thrd_param_ *)malloc(sizeof *p);
    if (p == NULL) {
        return thrd_nomem;
    }
    p->func = func;
    p->arg  = arg;
    *thr    = CreateThread(NULL, 0, fl_thrd_wrapper_, p, 0, NULL);
    if (*thr == NULL) {
        free(p);
        return thrd_error;
    }
    return thrd_success;
}

int thrd_join(thrd_t thr, int *res) {
    WaitForSingleObject(thr, INFINITE);
    if (res != NULL) {
        DWORD exit_code;
        GetExitCodeThread(thr, &exit_code);
        *res = (int)exit_code;
    }
    CloseHandle(thr);
    return thrd_success;
}

/* --- Windows thrd_sleep (lifted from win_timer.c's nanosleep) --- */

#define FL_NS_PER_SEC          1000000000LL
#define FL_TIMER_RESOLUTION_NS 100 /* Windows waitable timers tick in 100ns units */

int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    LONGLONG intervals
        = (LONGLONG)duration->tv_sec * (FL_NS_PER_SEC / FL_TIMER_RESOLUTION_NS)
          + (LONGLONG)duration->tv_nsec / FL_TIMER_RESOLUTION_NS;

    HANDLE h = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                     TIMER_ALL_ACCESS);
    if (h == NULL) {
        return -2; /* error other than interruption */
    }

    LARGE_INTEGER due;
    due.QuadPart = -intervals; /* negative => relative time */
    if (!SetWaitableTimer(h, &due, 0, NULL, NULL, FALSE)) {
        CloseHandle(h);
        return -2;
    }

    (void)WaitForSingleObject(h, INFINITE); /* not interruptible here */
    CloseHandle(h);

    if (remaining != NULL) { /* uninterruptible wait => nothing remains */
        remaining->tv_sec  = 0;
        remaining->tv_nsec = 0;
    }
    return 0;
}

#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)

#include <stdint.h> /* intptr_t */
#include <time.h>   /* struct timespec, nanosleep */

/* --- POSIX mutex --- */

int mtx_init(mtx_t *mutex, int type) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (type & mtx_recursive) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    int rc = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return rc == 0 ? thrd_success : thrd_error;
}

int mtx_lock(mtx_t *mutex) {
    return pthread_mutex_lock(mutex) == 0 ? thrd_success : thrd_error;
}

int mtx_unlock(mtx_t *mutex) {
    return pthread_mutex_unlock(mutex) == 0 ? thrd_success : thrd_error;
}

void mtx_destroy(mtx_t *mutex) {
    pthread_mutex_destroy(mutex);
}

/* --- POSIX threads --- */

typedef struct {
    thrd_start_t func;
    void        *arg;
} fl_thrd_param_;

static void *fl_thrd_wrapper_(void *param) {
    fl_thrd_param_ *p    = (fl_thrd_param_ *)param;
    thrd_start_t    func = p->func;
    void           *arg  = p->arg;
    free(p);
    return (void *)(intptr_t)func(arg);
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    fl_thrd_param_ *p = (fl_thrd_param_ *)malloc(sizeof *p);
    if (p == NULL) {
        return thrd_nomem;
    }
    p->func = func;
    p->arg  = arg;
    if (pthread_create(thr, NULL, fl_thrd_wrapper_, p) != 0) {
        free(p);
        return thrd_error;
    }
    return thrd_success;
}

int thrd_join(thrd_t thr, int *res) {
    void *retval = NULL;
    if (pthread_join(thr, res != NULL ? &retval : NULL) != 0) {
        return thrd_error;
    }
    if (res != NULL) {
        *res = (int)(intptr_t)retval;
    }
    return thrd_success;
}

/* --- POSIX thrd_sleep --- */

int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    return nanosleep(duration, remaining) == 0 ? 0 : -1;
}

#endif /* platform */

#endif /* polyfill needed */
