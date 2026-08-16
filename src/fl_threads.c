/**
 * @file fl_threads.c
 * @author Douglas Cuthbertson
 * @brief Compatibility-shim implementations for the C11 <threads.h> mutex and thread
 * subset.
 * @version 0.1
 * @date 2026-02-19
 *
 * Compiled when <threads.h> is unavailable on the target platform, which
 * fl_threads.h resolves into FL_THREADS_USE_SHIM. Otherwise this file is an
 * empty translation unit.
 * Windows:  CRITICAL_SECTION for mutexes, CreateThread for threads.
 * POSIX:    pthread_mutex_t for mutexes, pthread_create for threads.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_threads.h>

#if FL_THREADS_USE_SHIM

#include <stddef.h> // for NULL

/* --- Thread-parameter slots (shared by both backends) ---------------------
 * CreateThread and pthread_create each carry exactly one pointer into the new
 * thread, so the (func, arg) pair must live somewhere that outlives the
 * creator's stack frame. These slots supply that storage, which keeps the
 * thread shim free of any allocator: its only dependency stays the OS thread
 * API itself.
 *
 * A new thread copies the pair into locals and releases its slot as its first
 * action, so a slot is held only for the window between the create call and
 * the new thread's first instructions. The pool therefore bounds concurrent
 * pending starts, not live threads. Exhausting it yields thrd_nomem, the same
 * code the C11 contract already reserves for "could not allocate what the
 * thread needed"; raise FL_THRD_PARAM_SLOTS if a caller can outrun it.
 */
#ifndef FL_THRD_PARAM_SLOTS
#define FL_THRD_PARAM_SLOTS 64
#endif

/* The shim cannot assume <stdatomic.h> is enabled, so each backend claims a
 * slot with the test-and-set primitive it already has. */
#if defined(_WIN32) || defined(WIN32)
typedef volatile LONG fl_thrd_slot_;
#define FL_THRD_SLOT_CLAIM(S)   (InterlockedExchange(&(S), 1) == 0)
#define FL_THRD_SLOT_RELEASE(S) ((void)InterlockedExchange(&(S), 0))
#else
typedef int volatile fl_thrd_slot_;
#define FL_THRD_SLOT_CLAIM(S)   (__atomic_exchange_n(&(S), 1, __ATOMIC_ACQUIRE) == 0)
#define FL_THRD_SLOT_RELEASE(S) __atomic_store_n(&(S), 0, __ATOMIC_RELEASE)
#endif

typedef struct {
    thrd_start_t func;
    void        *arg;
} fl_thrd_param_;

static fl_thrd_param_ fl_thrd_params_[FL_THRD_PARAM_SLOTS];
static fl_thrd_slot_  fl_thrd_slot_used_[FL_THRD_PARAM_SLOTS];

/* Returns a claimed slot holding the pair, or NULL when every slot is in use.
 * The create call that publishes the slot pointer to the new thread is itself
 * the synchronization edge, so the pair needs no barrier of its own. */
static fl_thrd_param_ *fl_thrd_param_claim_(thrd_start_t func, void *arg) {
    fl_thrd_param_ *slot = NULL;
    for (int i = 0; i < FL_THRD_PARAM_SLOTS && slot == NULL; i++) {
        if (FL_THRD_SLOT_CLAIM(fl_thrd_slot_used_[i])) {
            fl_thrd_params_[i].func = func;
            fl_thrd_params_[i].arg  = arg;
            slot                    = &fl_thrd_params_[i];
        }
    }
    return slot;
}

static void fl_thrd_param_release_(fl_thrd_param_ *p) {
    FL_THRD_SLOT_RELEASE(fl_thrd_slot_used_[p - fl_thrd_params_]);
}

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <handleapi.h>         // for CloseHandle
#include <processthreadsapi.h> // for CreateThread, GetExitCodeThread
#include <stdlib.h>            // for calloc, free (tss shim below)
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

static DWORD WINAPI fl_thrd_wrapper_(LPVOID param) {
    fl_thrd_param_ *p    = (fl_thrd_param_ *)param;
    thrd_start_t    func = p->func;
    void           *arg  = p->arg;
    fl_thrd_param_release_(p);
    return (DWORD)func(arg);
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    fl_thrd_param_ *p = fl_thrd_param_claim_(func, arg);
    if (p == NULL) {
        return thrd_nomem;
    }
    *thr = CreateThread(NULL, 0, fl_thrd_wrapper_, p, 0, NULL);
    if (*thr == NULL) {
        fl_thrd_param_release_(p);
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

static void *fl_thrd_wrapper_(void *param) {
    fl_thrd_param_ *p    = (fl_thrd_param_ *)param;
    thrd_start_t    func = p->func;
    void           *arg  = p->arg;
    fl_thrd_param_release_(p);
    return (void *)(intptr_t)func(arg);
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    fl_thrd_param_ *p = fl_thrd_param_claim_(func, arg);
    if (p == NULL) {
        return thrd_nomem;
    }
    if (pthread_create(thr, NULL, fl_thrd_wrapper_, p) != 0) {
        fl_thrd_param_release_(p);
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

#endif /* compatibility shim needed */

/* --- tss shim ---------------------------------------------------------- */
/* Selected by FL_THREADS_USE_SHIM, like the rest of the shim.
 * Windows: one FLS slot holds a per-thread value array; the FLS callback runs
 * the registered destructors at thread exit. POSIX: pthread keys map 1:1. */
#if FL_THREADS_USE_SHIM

#if defined(_WIN32) || defined(WIN32)

#define FL_TSS_MAX 16

static tss_dtor_t    fl_tss_dtors_[FL_TSS_MAX];
static volatile LONG fl_tss_count_;
static DWORD         fl_tss_fls_ = FLS_OUT_OF_INDEXES;

static VOID WINAPI fl_tss_thread_exit_(PVOID data) {
    void **values = (void **)data;
    if (values != NULL) {
        for (LONG i = 0; i < fl_tss_count_; i++) {
            if (fl_tss_dtors_[i] != NULL && values[i] != NULL) {
                fl_tss_dtors_[i](values[i]);
            }
        }
        free(values);
    }
}

int tss_create(tss_t *key, tss_dtor_t dtor) {
    LONG index = InterlockedIncrement(&fl_tss_count_) - 1;
    if (index >= FL_TSS_MAX) {
        return thrd_error;
    }
    if (fl_tss_fls_ == FLS_OUT_OF_INDEXES) {
        DWORD fls = FlsAlloc(fl_tss_thread_exit_);
        if (fls == FLS_OUT_OF_INDEXES) {
            return thrd_error;
        }
        fl_tss_fls_ = fls;
    }
    fl_tss_dtors_[index] = dtor;
    *key                 = (tss_t)index;
    return thrd_success;
}

void *tss_get(tss_t key) {
    void **values = (void **)FlsGetValue(fl_tss_fls_);
    return values != NULL ? values[key] : NULL;
}

int tss_set(tss_t key, void *val) {
    void **values = (void **)FlsGetValue(fl_tss_fls_);
    if (values == NULL) {
        values = (void **)calloc(FL_TSS_MAX, sizeof *values);
        if (values == NULL || !FlsSetValue(fl_tss_fls_, values)) {
            free(values);
            return thrd_error;
        }
    }
    values[key] = val;
    return thrd_success;
}

void tss_delete(tss_t key) {
    fl_tss_dtors_[key] = NULL;
}

#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)

int tss_create(tss_t *key, tss_dtor_t dtor) {
    return pthread_key_create(key, dtor) == 0 ? thrd_success : thrd_error;
}

void *tss_get(tss_t key) {
    return pthread_getspecific(key);
}

int tss_set(tss_t key, void *val) {
    return pthread_setspecific(key, val) == 0 ? thrd_success : thrd_error;
}

void tss_delete(tss_t key) {
    (void)pthread_key_delete(key);
}

#endif

#endif /* compatibility shim */
