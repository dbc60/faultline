/**
 * @file arena_bench_main.c
 * @author Douglas Cuthbertson
 * @brief Arena allocator benchmark harness.
 * @version 0.1
 * @date 2026-07-19
 *
 * Measures the arena allocator's hot paths so synchronization changes can be
 * evaluated against a recorded baseline. Reports primitive costs (uncontended
 * lock pairs for C11 mtx_t and Win32 SRWLOCK, and an empty FL_TRY frame)
 * alongside arena workloads, so any per-call overhead added to the public
 * entry points can be attributed to its source.
 *
 * Workloads:
 *   - small ping-pong: 64-byte malloc+free (fast node / small bins)
 *   - mixed ping-pong: sizes crossing the 1 KiB large-chunk boundary
 *   - bulk alloc/free in LIFO, FIFO, and interleaved order (merge paths)
 *   - multi-thread: private arena per thread; optionally a shared arena
 *     (--shared, only meaningful when the arena synchronizes internally)
 *
 * Options:
 *   --shared  also run the shared-arena thread workloads (requires a build
 *             with FL_ARENA_SYNCHRONIZED)
 *   --quick   divide iteration counts by 8 (smoke test)
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

// Must be defined before any headers are included so that fl_log.h and fl_try.h
// select the platform implementations of the log and exception services.
#ifndef FL_PLATFORM_BUILD
#define FL_PLATFORM_BUILD
#endif

#include <faultline/fl_log.h> // LOG_* (selects flp_ backend via FL_PLATFORM_BUILD)

#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c" // FL_MALLOC/FL_FREE backing, needed by flp_file_service.c
#include "arena_pool.c"
#include "digital_search_tree.c"
#include "fl_exception_service.c"
#include "fl_threads.c"
#include "flp_exception_service.c"
#include "flp_file_service.c"
#include "flp_log_service.c"    // calls flp_stream_open/write/close
#include "flp_memory_service.c" // FL_MALLOC/FL_FREE backing (flp_malloc/flp_free)
#include "flp_stream_service.c"
#include "lock_os.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"

#include <faultline/arena.h>
#include <faultline/arena_pool.h>
#include <faultline/fl_abbreviated_types.h> // u32, u64
#include <faultline/fl_macros.h>            // FL_ARRAY_COUNT
#include <faultline/fl_threads.h>           // mtx_t, thrd_t
#include <faultline/fl_try.h>               // FL_TRY, FL_CATCH_ALL, FL_END_TRY
#include <faultline/size.h>                 // MEBI

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // QueryPerformanceCounter, SetThreadAffinityMask

static char const *module = "ArenaBench";

#define BM_MALLOC(A, SZ) arena_malloc_throw((A), (SZ), __FILE__, __LINE__)
#define BM_FREE(A, P)    arena_free_throw((A), (P), __FILE__, __LINE__)

enum {
    BM_REPS        = 9,    ///< measured repetitions per single-threaded benchmark
    BM_THREAD_REPS = 5,    ///< measured repetitions per multi-threaded benchmark
    BM_BULK_COUNT  = 4096, ///< live allocations per bulk pass
    BM_MAX_THREADS = 4,
};

/// defeats dead-code elimination in the primitive benchmarks
static size_t volatile g_sink;

/// nanoseconds per QueryPerformanceCounter tick
static double g_qpc_period_ns;

/// logical processor count, set once in main
static u32 g_ncores;

/**
 * @brief Map a worker index to a logical processor.
 *
 * Adjacent logical processors are SMT siblings on this class of hardware, so
 * spread workers across even-numbered processors when there are enough; two
 * workers on one physical core would measure SMT contention, not scaling.
 */
static u32 bm_worker_core(size_t t) {
    u32 core = (u32)(2 * t);
    if (core >= g_ncores) {
        core = (u32)t;
    }
    return core;
}

static u64 bm_now(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (u64)counter.QuadPart;
}

static int bm_cmp_double(void const *a, void const *b) {
    double lhs = *(double const *)a;
    double rhs = *(double const *)b;

    if (lhs < rhs) {
        return -1;
    } else if (lhs > rhs) {
        return 1;
    } else {
        return 0;
    }
}

static void bm_report(char const *name, size_t ops, double *ns_per_op, size_t reps) {
    qsort(ns_per_op, reps, sizeof *ns_per_op, bm_cmp_double);
    printf("%-34s %12zu ops   median %9.2f ns/op   min %9.2f ns/op\n", name, ops,
           ns_per_op[reps / 2], ns_per_op[0]);
    fflush(stdout);
}

/* ------------------------- Primitive benchmarks ------------------------- */

typedef u64(bm_fn)(Arena *arena, size_t iters);

static u64 bm_mtx_pair(Arena *arena, size_t iters) {
    FL_UNUSED(arena);
    mtx_t lock;
    mtx_init(&lock, mtx_plain);

    u64 begin = bm_now();
    for (size_t i = 0; i < iters; i++) {
        mtx_lock(&lock);
        g_sink = i;
        mtx_unlock(&lock);
    }
    u64 elapsed = bm_now() - begin;

    mtx_destroy(&lock);
    return elapsed;
}

static u64 bm_srw_pair(Arena *arena, size_t iters) {
    FL_UNUSED(arena);
    SRWLOCK lock = SRWLOCK_INIT;

    u64 begin = bm_now();
    for (size_t i = 0; i < iters; i++) {
        AcquireSRWLockExclusive(&lock);
        g_sink = i;
        ReleaseSRWLockExclusive(&lock);
    }
    return bm_now() - begin;
}

static tss_t g_bench_tss_key;

static u64 bm_tss_get(Arena *arena, size_t iters) {
    FL_UNUSED(arena);
    u64 begin = bm_now();
    for (size_t i = 0; i < iters; i++) {
        g_sink = (size_t)(uintptr_t)tss_get(g_bench_tss_key);
    }
    return bm_now() - begin;
}

static u64 bm_try_frame(Arena *arena, size_t iters) {
    FL_UNUSED(arena);

    u64 begin = bm_now();
    for (size_t i = 0; i < iters; i++) {
        FL_TRY {
            g_sink = i;
        }
        FL_END_TRY;
    }
    return bm_now() - begin;
}

/* --------------------------- Arena workloads ---------------------------- */

static u64 bm_small_pingpong(Arena *arena, size_t iters) {
    u64 begin = bm_now();
    for (size_t i = 0; i < iters; i++) {
        void *mem    = BM_MALLOC(arena, 64);
        *(char *)mem = (char)i;
        BM_FREE(arena, mem);
    }
    return bm_now() - begin;
}

static size_t const bm_mixed_sizes[] = {16, 48, 96, 256, 512, 1024, 2048, 8192};

static u64 bm_mixed_pingpong(Arena *arena, size_t iters) {
    size_t const count = FL_ARRAY_COUNT(bm_mixed_sizes);

    u64 begin = bm_now();
    for (size_t i = 0; i < iters; i++) {
        void *mem    = BM_MALLOC(arena, bm_mixed_sizes[i % count]);
        *(char *)mem = (char)i;
        BM_FREE(arena, mem);
    }
    return bm_now() - begin;
}

typedef enum BulkOrder {
    BULK_LIFO,
    BULK_FIFO,
    BULK_INTERLEAVED,
} BulkOrder;

static void *g_bulk_ptrs[BM_BULK_COUNT];

static u64 bm_bulk(Arena *arena, size_t passes, BulkOrder order) {
    u64 begin = bm_now();
    for (size_t p = 0; p < passes; p++) {
        for (size_t i = 0; i < BM_BULK_COUNT; i++) {
            g_bulk_ptrs[i] = BM_MALLOC(arena, 128);
        }

        switch (order) {
        case BULK_LIFO:
            for (size_t i = BM_BULK_COUNT; i > 0; i--) {
                BM_FREE(arena, g_bulk_ptrs[i - 1]);
            }
            break;
        case BULK_FIFO:
            for (size_t i = 0; i < BM_BULK_COUNT; i++) {
                BM_FREE(arena, g_bulk_ptrs[i]);
            }
            break;
        case BULK_INTERLEAVED:
            // freeing even indices first leaves free neighbors around every odd
            // chunk, forcing backward/forward merges on the second sweep
            for (size_t i = 0; i < BM_BULK_COUNT; i += 2) {
                BM_FREE(arena, g_bulk_ptrs[i]);
            }
            for (size_t i = 1; i < BM_BULK_COUNT; i += 2) {
                BM_FREE(arena, g_bulk_ptrs[i]);
            }
            break;
        default:
            LOG_ERROR(module, "unknown bulk order %d", (int)order);
            break;
        }
    }
    return bm_now() - begin;
}

static u64 bm_bulk_lifo(Arena *arena, size_t passes) {
    return bm_bulk(arena, passes, BULK_LIFO);
}

static u64 bm_bulk_fifo(Arena *arena, size_t passes) {
    return bm_bulk(arena, passes, BULK_FIFO);
}

static u64 bm_bulk_interleaved(Arena *arena, size_t passes) {
    return bm_bulk(arena, passes, BULK_INTERLEAVED);
}

/* ------------------------ Single-threaded runner ------------------------ */

static u64 bm_run_one(bm_fn *fn, Arena *arena, size_t iters) {
    u64  ticks  = 0;
    bool failed = false;

    FL_TRY {
        ticks = fn(arena, iters);
    }
    FL_CATCH_ALL {
        failed = true;
        fprintf(stderr, "benchmark threw: %s\n", FL_REASON);
        LOG_ERROR(module, "benchmark threw: %s", FL_REASON);
    }
    FL_END_TRY;

    if (failed) {
        exit(1);
    }
    return ticks;
}

static void bm_run(char const *name, bm_fn *fn, Arena *arena, size_t iters,
                   size_t ops_per_iter) {
    double results[BM_REPS];
    size_t ops = iters * ops_per_iter;

    (void)bm_run_one(fn, arena, iters); // warmup
    for (size_t r = 0; r < BM_REPS; r++) {
        u64 ticks  = bm_run_one(fn, arena, iters);
        results[r] = ((double)ticks * g_qpc_period_ns) / (double)ops;
    }
    bm_report(name, ops, results, BM_REPS);
}

/* ------------------------- Multi-threaded runner ------------------------ */

typedef struct ThreadBenchArg {
    Arena      *shared_arena; ///< NULL: the worker creates a private arena
    ArenaPool  *pool;         ///< when set, allocate through the pool instead
    size_t      iters;
    u32         core;
    atomic_int *ready;
    atomic_int *go;
} ThreadBenchArg;

static int bm_thread_worker(void *arg) {
    ThreadBenchArg *tba    = (ThreadBenchArg *)arg;
    bool            failed = false;

    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << tba->core);

    Arena *own   = NULL;
    Arena *arena = tba->shared_arena;
    if (arena == NULL && tba->pool == NULL) {
        own   = new_arena(MEBI(4), 0);
        arena = own;
    }

    atomic_fetch_add(tba->ready, 1);
    while (atomic_load(tba->go) == 0) {
        YieldProcessor();
    }

    FL_TRY {
        if (tba->pool != NULL) {
            for (size_t i = 0; i < tba->iters; i++) {
                void *mem    = ARENA_POOL_MALLOC_THROW(tba->pool, 64);
                *(char *)mem = (char)i;
                ARENA_POOL_FREE_THROW(tba->pool, mem);
            }
        } else {
            for (size_t i = 0; i < tba->iters; i++) {
                void *mem    = BM_MALLOC(arena, 64);
                *(char *)mem = (char)i;
                BM_FREE(arena, mem);
            }
        }
    }
    FL_CATCH_ALL {
        failed = true;
        LOG_ERROR(module, "worker threw: %s", FL_REASON);
    }
    FL_END_TRY;

    if (own != NULL) {
        release_arena(&own);
    }
    return failed ? 1 : 0;
}

static double bm_threads_once(Arena *shared, ArenaPool *pool, size_t nthreads,
                              size_t iters) {
    thrd_t         threads[BM_MAX_THREADS];
    ThreadBenchArg args[BM_MAX_THREADS];
    atomic_int     ready  = 0;
    atomic_int     go     = 0;
    bool           failed = false;

    for (size_t t = 0; t < nthreads; t++) {
        args[t].shared_arena = shared;
        args[t].pool         = pool;
        args[t].iters        = iters;
        args[t].core         = bm_worker_core(t);
        args[t].ready        = &ready;
        args[t].go           = &go;
        if (thrd_create(&threads[t], bm_thread_worker, &args[t]) != thrd_success) {
            fprintf(stderr, "failed to create worker thread %zu\n", t);
            exit(1);
        }
    }

    while ((size_t)atomic_load(&ready) < nthreads) {
        YieldProcessor();
    }

    u64 begin = bm_now();
    atomic_store(&go, 1);
    for (size_t t = 0; t < nthreads; t++) {
        int res = 0;
        thrd_join(threads[t], &res);
        if (res != 0) {
            failed = true;
        }
    }
    u64 elapsed = bm_now() - begin;

    if (failed) {
        fprintf(stderr, "a worker thread failed; aborting\n");
        exit(1);
    }

    size_t ops = 2 * iters * nthreads;
    return ((double)elapsed * g_qpc_period_ns) / (double)ops;
}

static void bm_run_threads(char const *name, Arena *shared, ArenaPool *pool,
                           size_t nthreads, size_t iters) {
    double results[BM_THREAD_REPS];

    (void)bm_threads_once(shared, pool, nthreads, iters / 4); // warmup
    for (size_t r = 0; r < BM_THREAD_REPS; r++) {
        results[r] = bm_threads_once(shared, pool, nthreads, iters);
    }
    bm_report(name, 2 * iters * nthreads, results, BM_THREAD_REPS);
}

/* --------------------- Producer-consumer (remote free) ------------------ */

enum {
    BM_XFER_RING = 256
};

typedef struct XferShared {
    ArenaPool      *pool;
    size_t          iters;
    atomic_int     *ready;
    atomic_int     *go;
    _Atomic(void *) ring[BM_XFER_RING];
} XferShared;

typedef struct XferArg {
    XferShared *sh;
    u32         core;
    bool        producer;
} XferArg;

static int bm_xfer_worker(void *arg) {
    XferArg    *xa     = (XferArg *)arg;
    XferShared *sh     = xa->sh;
    bool        failed = false;

    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << xa->core);
    atomic_fetch_add(sh->ready, 1);
    while (atomic_load(sh->go) == 0) {
        YieldProcessor();
    }

    FL_TRY {
        if (xa->producer) {
            for (size_t i = 0; i < sh->iters; i++) {
                void *mem             = ARENA_POOL_MALLOC_THROW(sh->pool, 64);
                *(char *)mem          = (char)i;
                _Atomic(void *) *slot = &sh->ring[i % BM_XFER_RING];
                while (atomic_load_explicit(slot, memory_order_relaxed) != NULL) {
                    YieldProcessor();
                }
                atomic_store_explicit(slot, mem, memory_order_release);
            }
        } else {
            for (size_t i = 0; i < sh->iters; i++) {
                _Atomic(void *) *slot = &sh->ring[i % BM_XFER_RING];
                void            *mem;
                while ((mem = atomic_load_explicit(slot, memory_order_acquire))
                       == NULL) {
                    YieldProcessor();
                }
                atomic_store_explicit(slot, NULL, memory_order_release);
                ARENA_POOL_FREE_THROW(sh->pool, mem);
            }
        }
    }
    FL_CATCH_ALL {
        failed = true;
        LOG_ERROR(module, "xfer worker threw: %s", FL_REASON);
    }
    FL_END_TRY;

    return failed ? 1 : 0;
}

static double bm_xfer_once(ArenaPool *pool, size_t iters) {
    XferShared sh;
    XferArg    args[2];
    thrd_t     threads[2];
    atomic_int ready  = 0;
    atomic_int go     = 0;
    bool       failed = false;

    sh.pool  = pool;
    sh.iters = iters;
    sh.ready = &ready;
    sh.go    = &go;
    for (size_t i = 0; i < BM_XFER_RING; i++) {
        atomic_store_explicit(&sh.ring[i], NULL, memory_order_relaxed);
    }

    for (size_t t = 0; t < 2; t++) {
        args[t].sh       = &sh;
        args[t].core     = bm_worker_core(t);
        args[t].producer = t == 0;
        if (thrd_create(&threads[t], bm_xfer_worker, &args[t]) != thrd_success) {
            fprintf(stderr, "failed to create xfer thread %zu\n", t);
            exit(1);
        }
    }

    while (atomic_load(&ready) < 2) {
        YieldProcessor();
    }
    u64 begin = bm_now();
    atomic_store(&go, 1);
    for (size_t t = 0; t < 2; t++) {
        int res = 0;
        thrd_join(threads[t], &res);
        if (res != 0) {
            failed = true;
        }
    }
    u64 elapsed = bm_now() - begin;

    if (failed) {
        fprintf(stderr, "an xfer thread failed; aborting\n");
        exit(1);
    }

    size_t ops = 2 * iters;
    return ((double)elapsed * g_qpc_period_ns) / (double)ops;
}

static void bm_run_xfer(char const *name, ArenaPool *pool, size_t iters) {
    double results[BM_THREAD_REPS];

    (void)bm_xfer_once(pool, iters / 4); // warmup
    for (size_t r = 0; r < BM_THREAD_REPS; r++) {
        results[r] = bm_xfer_once(pool, iters);
    }
    bm_report(name, 2 * iters, results, BM_THREAD_REPS);
}

/* -------------------------------- main ---------------------------------- */

int main(int argc, char **argv) {
    bool shared = false;
    bool quick  = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--shared") == 0) {
            shared = true;
        } else if (strcmp(argv[i], "--quick") == 0) {
            quick = true;
        } else {
            fprintf(stderr, "Usage: %s [--shared] [--quick]\n", argv[0]);
            return 1;
        }
    }

#ifndef FL_ARENA_SYNCHRONIZED
    if (shared) {
        fprintf(stderr, "--shared requires a build with FL_ARENA_SYNCHRONIZED; skipping "
                        "shared-arena workloads\n");
        shared = false;
    }
#endif

    flp_log_init_custom(LOG_LEVEL_WARN, "arena_bench.log");

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    g_qpc_period_ns = 1e9 / (double)freq.QuadPart;

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    g_ncores   = (u32)si.dwNumberOfProcessors;
    u32 ncores = g_ncores;

    // Keep the timed thread off the worker cores (workers spread across
    // even-numbered logical processors).
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << (ncores - 1));

#if defined(DEBUG) || defined(_DEBUG)
    char const *build_type = "debug";
#else
    char const *build_type = "release";
#endif
#ifdef FL_ARENA_SYNCHRONIZED
    char const *sync_mode = "sync available";
#else
    char const *sync_mode = "sync compiled out";
#endif
    printf("arena benchmark: %s build, %s, %zu-bit, %lu cores, QPC %.0f Hz%s%s\n\n",
           build_type, sync_mode, sizeof(void *) * 8, (unsigned long)ncores,
           1e9 / g_qpc_period_ns, shared ? ", shared" : "", quick ? ", quick" : "");

    size_t scale = quick ? 8 : 1;
    Arena *arena = new_arena(MEBI(16), 0);
#ifdef FL_ARENA_SYNCHRONIZED
    Arena *shared_arena = new_shared_arena(MEBI(16), 0);
#endif

    bm_run("mtx lock/unlock pair", bm_mtx_pair, NULL, 10000000 / scale, 1);
    bm_run("SRWLOCK acquire/release pair", bm_srw_pair, NULL, 10000000 / scale, 1);
    if (tss_create(&g_bench_tss_key, NULL) == thrd_success) {
        tss_set(g_bench_tss_key, &g_bench_tss_key);
        bm_run("tss_get", bm_tss_get, NULL, 10000000 / scale, 1);
    }
    bm_run("FL_TRY empty frame", bm_try_frame, NULL, 2000000 / scale, 1);
    bm_run("small 64B malloc+free", bm_small_pingpong, arena, 1000000 / scale, 2);
#ifdef FL_ARENA_SYNCHRONIZED
    bm_run("small 64B malloc+free (shared)", bm_small_pingpong, shared_arena,
           1000000 / scale, 2);
#endif
    bm_run("mixed 16B-8KiB malloc+free", bm_mixed_pingpong, arena, 1000000 / scale, 2);
    bm_run("bulk 4096x128B LIFO", bm_bulk_lifo, arena, 64 / scale, 2 * BM_BULK_COUNT);
    bm_run("bulk 4096x128B FIFO", bm_bulk_fifo, arena, 64 / scale, 2 * BM_BULK_COUNT);
    bm_run("bulk 4096x128B interleaved", bm_bulk_interleaved, arena, 64 / scale,
           2 * BM_BULK_COUNT);

    size_t max_threads = ncores < BM_MAX_THREADS ? (size_t)ncores : BM_MAX_THREADS;
    for (size_t t = 2; t <= max_threads; t *= 2) {
        char name[64];
        snprintf(name, sizeof name, "%zu threads, private arenas", t);
        bm_run_threads(name, NULL, NULL, t, 1000000 / scale);
    }

    ArenaPool *pool = new_arena_pool(MEBI(4), 0);
    bm_run_threads("1 thread, pool local free", NULL, pool, 1, 1000000 / scale);
    for (size_t t = 2; t <= max_threads; t *= 2) {
        char name[64];
        snprintf(name, sizeof name, "%zu threads, pool local free", t);
        bm_run_threads(name, NULL, pool, t, 1000000 / scale);
    }
    bm_run_xfer("producer-consumer, remote free", pool, 1000000 / scale);
    release_arena_pool(&pool);

#ifdef FL_ARENA_SYNCHRONIZED
    if (shared) {
        for (size_t t = 2; t <= max_threads; t *= 2) {
            char name[64];
            snprintf(name, sizeof name, "%zu threads, shared arena", t);
            bm_run_threads(name, shared_arena, NULL, t, 1000000 / scale);
        }
    }

    release_arena(&shared_arena);
#endif
    release_arena(&arena);
    flp_log_cleanup();
    return 0;
}
