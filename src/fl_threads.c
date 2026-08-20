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
 * Windows: one FLS (Fiber Local Storage) slot holds a per-thread value array.
 * FLS is TLS plus a destructor callback, which is what tss_create's dtor
 * argument needs; TlsAlloc has no such hook. The FLS callback runs the
 * registered destructors at thread exit. POSIX: pthread keys map 1:1. */
#if FL_THREADS_USE_SHIM

#if defined(_WIN32) || defined(WIN32)

#define FL_TSS_MAX 16

/* A key is a slot index in its low bits and the generation the slot was issued
 * under in the rest. Masking the index out of the key means it is always in
 * range, and carrying the generation in the key keeps the validity check off
 * the global generation array: tss_get compares against the generation stored
 * beside the value, which shares its cache line. */
#define FL_TSS_INDEX_BITS 4
#define FL_TSS_INDEX_MASK ((tss_t)(FL_TSS_MAX - 1))
#define FL_TSS_SLOT(key)  ((tss_t)((key) & FL_TSS_INDEX_MASK))
#define FL_TSS_GEN(key)   ((LONG)((key) >> FL_TSS_INDEX_BITS))

/* Slot bookkeeping. A key is an index into these arrays.
 *
 * fl_tss_used_ is the allocator: a slot is claimed with a compare-exchange, so
 * tss_delete can hand it back and a later tss_create can take it again. A
 * counter that only moved forward would burn one slot per create/destroy cycle
 * and stop issuing keys once FL_TSS_MAX cycles had gone by. A NULL destructor is
 * a legitimate argument to tss_create, so occupancy cannot be read off
 * fl_tss_dtors_ and needs its own array.
 *
 * fl_tss_gen_ retires the values other threads still hold for a slot. Those
 * value arrays are private to their own threads and cannot be reached from
 * here, so tss_delete bumps the slot's generation instead: a stored value
 * carries the generation of the key it was written for, and one written under
 * an earlier generation reads back as absent. Without it, a slot reissued to a
 * second key would offer the first key's values to the second key's destructor.
 * It also makes a stale key inert rather than dangerous -- it addresses a slot
 * that no longer answers to it.
 */
static tss_dtor_t     fl_tss_dtors_[FL_TSS_MAX];
static volatile LONG  fl_tss_used_[FL_TSS_MAX];
static volatile LONG  fl_tss_gen_[FL_TSS_MAX];
static volatile LONG  fl_tss_live_;
static volatile DWORD fl_tss_fls_ = FLS_OUT_OF_INDEXES;

/** A per-thread value, tagged with the generation of the slot it was stored under. */
typedef struct FLTssValue {
    void *val;
    LONG  gen;
} FLTssValue;

static VOID WINAPI fl_tss_thread_exit_(PVOID data) {
    FLTssValue *values = (FLTssValue *)data;
    if (values == NULL) {
        return;
    }
    for (LONG i = 0; i < FL_TSS_MAX; i++) {
        if (fl_tss_dtors_[i] != NULL && values[i].val != NULL
            && values[i].gen == fl_tss_gen_[i]) {
            fl_tss_dtors_[i](values[i].val);
        }
    }
    free(values);
}

/** @brief Claim a free slot. @return its index, or FL_TSS_MAX if all are in use. */
static LONG fl_tss_claim_slot_(void) {
    LONG index = FL_TSS_MAX;

    for (LONG i = 0; i < FL_TSS_MAX; i++) {
        if (InterlockedCompareExchange(&fl_tss_used_[i], 1, 0) == 0) {
            index = i;
            break;
        }
    }

    return index;
}

int tss_create(tss_t *key, tss_dtor_t dtor) {
    LONG index = fl_tss_claim_slot_();
    if (index >= FL_TSS_MAX) {
        return thrd_error;
    }

    /* Counted before the FLS slot is touched. tss_delete releases that slot once
     * the live count reaches zero, and this claim keeps it from reaching zero
     * while the allocation below is in flight. */
    InterlockedIncrement(&fl_tss_live_);
    fl_tss_dtors_[index] = dtor;

    if (fl_tss_fls_ == FLS_OUT_OF_INDEXES) {
        DWORD fls = FlsAlloc(fl_tss_thread_exit_);
        if (fls == FLS_OUT_OF_INDEXES) {
            fl_tss_dtors_[index] = NULL;
            InterlockedDecrement(&fl_tss_live_);
            InterlockedExchange(&fl_tss_used_[index], 0);
            return thrd_error;
        }
        /* Two threads arriving together each allocate a slot; the one that loses
         * the exchange returns its own rather than leaking it. */
        if (InterlockedCompareExchange((volatile LONG *)&fl_tss_fls_, (LONG)fls,
                                       (LONG)FLS_OUT_OF_INDEXES)
            != (LONG)FLS_OUT_OF_INDEXES) {
            FlsFree(fls);
        }
    }

    *key = (tss_t)index | ((tss_t)fl_tss_gen_[index] << FL_TSS_INDEX_BITS);
    return thrd_success;
}

void *tss_get(tss_t key) {
    void *val = NULL;

    if (fl_tss_fls_ != FLS_OUT_OF_INDEXES) {
        FLTssValue *values = (FLTssValue *)FlsGetValue(fl_tss_fls_);
        if (values != NULL && values[FL_TSS_SLOT(key)].gen == FL_TSS_GEN(key)) {
            val = values[FL_TSS_SLOT(key)].val;
        }
    }

    return val;
}

int tss_set(tss_t key, void *val) {
    if (fl_tss_fls_ == FLS_OUT_OF_INDEXES) {
        return thrd_error;
    }

    FLTssValue *values = (FLTssValue *)FlsGetValue(fl_tss_fls_);
    if (values == NULL) {
        values = (FLTssValue *)calloc(FL_TSS_MAX, sizeof *values);
        if (values == NULL || !FlsSetValue(fl_tss_fls_, values)) {
            free(values);
            return thrd_error;
        }
    }

    values[FL_TSS_SLOT(key)].val = val;
    values[FL_TSS_SLOT(key)].gen = FL_TSS_GEN(key);
    return thrd_success;
}

void tss_delete(tss_t key) {
    tss_t slot = FL_TSS_SLOT(key);

    /* Only the key the slot was issued to may release it. A stale key -- one
     * already deleted, or one naming a slot since reissued -- fails this check,
     * so a repeated delete is a no-op rather than a second decrement. A live
     * count driven below zero never reaches zero again, which would leave the
     * FLS slot below permanently held. */
    if (fl_tss_gen_[slot] != FL_TSS_GEN(key)) {
        return;
    }
    if (InterlockedCompareExchange(&fl_tss_used_[slot], 0, 1) != 1) {
        return;
    }

    fl_tss_dtors_[slot] = NULL;
    InterlockedIncrement(&fl_tss_gen_[slot]);

    if (InterlockedDecrement(&fl_tss_live_) != 0) {
        return;
    }
    /* The FLS callback is an address inside whichever module compiled this
     * shim. A module that is unloaded before the process exits would leave the
     * system holding a dangling callback, so the slot is released as soon as
     * the last key goes away. FlsFree runs the callback for every fiber that
     * still holds a value array, which frees those arrays here rather than at
     * shutdown; every destructor is already NULL by this point, so the callback
     * only reclaims the arrays. */
    DWORD fls = (DWORD)InterlockedExchange((volatile LONG *)&fl_tss_fls_,
                                           (LONG)FLS_OUT_OF_INDEXES);
    if (fls != FLS_OUT_OF_INDEXES) {
        FlsFree(fls);
    }
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
