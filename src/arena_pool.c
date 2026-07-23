/**
 * @file arena_pool.c
 * @author Douglas Cuthbertson
 * @brief Sharded allocation pool: one unsynchronized Arena per thread with
 * remote-free queues for cross-thread frees.
 * @version 0.1
 * @date 2026-07-19
 *
 * Ownership model: the thread-specific-storage value is a pointer to the
 * thread's shard record. The free path compares the block's recorded owner
 * (arena_owner) with the caller's shard; a mismatch, including a caller that
 * never allocated, always takes the remote-queue path, which is safe from
 * any thread. Shard adoption happens under the pool lock, which is touched
 * only on thread arrival; allocation and free never take it.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/arena_pool.h>

#include <faultline/arena.h>
#include <faultline/fl_exception_types.h> // FLExceptionReason
#include <faultline/fl_macros.h>          // FL_THREAD_LOCAL
#include <faultline/fl_threads.h>         // tss_t
#include <faultline/fl_try.h>             // FL_TRY, FL_THROW
#include "chunk.h"                        // CHUNK_PAYLOAD_SIZE
#include "fl_lock.h"                      // FLLock

#include <stdatomic.h>
#include <stddef.h>
#include <string.h> // memcpy, memset

FLExceptionReason arena_pool_exhausted = "arena pool exhausted";

enum {
    ARENA_POOL_MAX_SHARDS = 64,
};

typedef enum ShardState {
    SHARD_FREE = 0, ///< slot has no arena yet
    SHARD_ACTIVE,   ///< owned by a live thread
    SHARD_ORPHANED, ///< owner exited; adoptable, memory still valid
} ShardState;

typedef struct ShardRecord {
    Arena      *arena;
    _Atomic int state;
} ShardRecord;

struct ArenaPool {
    FLLock        lock;       ///< serializes shard adoption/creation only
    tss_t         key;        ///< per-thread ShardRecord*
    unsigned long generation; ///< distinguishes pools that reuse an address
    size_t        shard_commit;
    unsigned int  shard_reserve;
    Arena        *book; ///< bookkeeping arena; this struct lives inside it
    ShardRecord   shards[ARENA_POOL_MAX_SHARDS];
};

/*
 * Per-thread fast path for the shard lookup. tss_get is a CRT call costly
 * enough to dominate the allocator itself (and it serializes concurrent
 * callers on this platform), so the hot path uses thread-local storage the
 * compiler can read directly; tss remains only for its thread-exit
 * destructor. The generation check prevents a stale binding from matching a
 * new pool that happens to occupy a released pool's address.
 */
static _Atomic unsigned long g_pool_generation;

static FL_THREAD_LOCAL struct {
    ArenaPool    *pool;
    unsigned long generation;
    ShardRecord  *rec;
} tls_shard_cache;

/// The calling thread's shard, or NULL if it has none yet; never creates one.
static ShardRecord *arena_pool_current_shard(ArenaPool *pool) {
    if (tls_shard_cache.pool == pool && tls_shard_cache.generation == pool->generation) {
        return tls_shard_cache.rec;
    }
    return (ShardRecord *)tss_get(pool->key);
}

/**
 * @brief Thread-exit destructor: orphan the shard so a future thread can
 * adopt it. The arena is not released, so allocations may outlive the thread.
 */
static void arena_pool_thread_exit(void *value) {
    ShardRecord *rec = (ShardRecord *)value;
    atomic_store_explicit(&rec->state, SHARD_ORPHANED, memory_order_release);
}

/**
 * @brief Return the calling thread's shard, adopting an orphan or creating a
 * new arena on first use.
 */
static ShardRecord *arena_pool_thread_shard(ArenaPool *pool, char const *file,
                                            int line) {
    ShardRecord *rec = arena_pool_current_shard(pool);
    if (rec != NULL) {
        return rec;
    }

    fl_lock_acquire(&pool->lock);
    for (int i = 0; i < ARENA_POOL_MAX_SHARDS; i++) {
        int state = atomic_load_explicit(&pool->shards[i].state, memory_order_acquire);
        if (state == SHARD_ORPHANED) {
            rec = &pool->shards[i];
            atomic_store_explicit(&rec->state, SHARD_ACTIVE, memory_order_relaxed);
            break;
        }
    }

    if (rec == NULL) {
        for (int i = 0; i < ARENA_POOL_MAX_SHARDS; i++) {
            if (atomic_load_explicit(&pool->shards[i].state, memory_order_relaxed)
                == SHARD_FREE) {
                rec = &pool->shards[i];
                break;
            }
        }
        if (rec == NULL) {
            fl_lock_release(&pool->lock);
            FL_THROW_FILE_LINE(arena_pool_exhausted, file, line);
        }
        FL_TRY {
            rec->arena = new_arena(pool->shard_commit, pool->shard_reserve);
        }
        FL_CATCH_ALL_RETHROW {
            rec = NULL;
            fl_lock_release(&pool->lock);
        }
        FL_END_TRY;
        atomic_store_explicit(&rec->state, SHARD_ACTIVE, memory_order_relaxed);
    }
    fl_lock_release(&pool->lock);

    if (tss_set(pool->key, rec) != thrd_success) {
        // The shard stays adoptable; this thread just cannot keep it.
        atomic_store_explicit(&rec->state, SHARD_ORPHANED, memory_order_release);
        FL_THROW_FILE_LINE(fl_internal_error, file, line);
    }
    tls_shard_cache.pool       = pool;
    tls_shard_cache.generation = pool->generation;
    tls_shard_cache.rec        = rec;
    return rec;
}

ArenaPool *new_arena_pool(size_t shard_commit, unsigned int shard_reserve) {
    Arena     *book = new_arena(0, 0);
    ArenaPool *pool = arena_malloc_throw(book, sizeof(ArenaPool), __FILE__, __LINE__);

    memset(pool, 0, sizeof *pool);
    pool->book          = book;
    pool->shard_commit  = shard_commit;
    pool->shard_reserve = shard_reserve;
    pool->generation
        = atomic_fetch_add_explicit(&g_pool_generation, 1, memory_order_relaxed) + 1;

    if (!fl_lock_init(&pool->lock)) {
        release_arena(&book);
        FL_THROW(fl_internal_error);
    }
    if (tss_create(&pool->key, arena_pool_thread_exit) != thrd_success) {
        fl_lock_destroy(&pool->lock);
        release_arena(&book);
        FL_THROW(fl_internal_error);
    }

    return pool;
}

void release_arena_pool(ArenaPool **pool) {
    if (pool == NULL || *pool == NULL) {
        return;
    }

    ArenaPool *p = *pool;
    tss_delete(p->key);
    for (int i = 0; i < ARENA_POOL_MAX_SHARDS; i++) {
        if (p->shards[i].arena != NULL) {
            release_arena(&p->shards[i].arena);
        }
    }
    fl_lock_destroy(&p->lock);

    // The pool struct lives inside the bookkeeping arena, so read it out
    // before the release invalidates p.
    Arena *book = p->book;
    release_arena(&book);
    *pool = NULL;
}

void *arena_pool_malloc_throw(ArenaPool *pool, size_t size, char const *file, int line) {
    ShardRecord *rec = arena_pool_thread_shard(pool, file, line);
    return arena_malloc_throw(rec->arena, size, file, line);
}

void *arena_pool_calloc_throw(ArenaPool *pool, size_t count, size_t size,
                              char const *file, int line) {
    ShardRecord *rec = arena_pool_thread_shard(pool, file, line);
    return arena_calloc_throw(rec->arena, count, size, file, line);
}

void *arena_pool_realloc_throw(ArenaPool *pool, void *mem, size_t size, char const *file,
                               int line) {
    if (mem == NULL) {
        return arena_pool_malloc_throw(pool, size, file, line);
    }

    Arena       *owner = arena_owner(mem);
    ShardRecord *rec   = arena_pool_thread_shard(pool, file, line);
    if (rec->arena == owner) {
        return arena_realloc_throw(owner, mem, size, file, line);
    }

    // The block belongs to another thread's shard: allocate from the caller's
    // shard, copy, and queue the old block back to its owner. The old block's
    // header is stable while it remains allocated, so reading its payload
    // size here is safe.
    Chunk *ch      = (Chunk *)((char *)mem - CHUNK_ALIGNED_SIZE);
    size_t payload = CHUNK_PAYLOAD_SIZE(ch);
    void  *fresh   = arena_malloc_throw(rec->arena, size, file, line);
    (void)memcpy(fresh, mem, size < payload ? size : payload);
    arena_free_remote(owner, mem);
    return fresh;
}

void arena_pool_free_throw(ArenaPool *pool, void *mem, char const *file, int line) {
    Arena       *owner = arena_owner(mem);
    ShardRecord *rec   = arena_pool_current_shard(pool);
    if (rec != NULL && rec->arena == owner) {
        arena_free_throw(owner, mem, file, line);
    } else {
        arena_free_remote(owner, mem);
    }
}
