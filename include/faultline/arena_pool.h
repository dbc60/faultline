#ifndef FAULTLINE_ARENA_POOL_H_
#define FAULTLINE_ARENA_POOL_H_

/**
 * @file arena_pool.h
 * @author Douglas Cuthbertson
 * @brief A sharded allocation pool: one unsynchronized Arena per thread, with
 * lock-free remote-free queues for cross-thread frees.
 * @version 0.1
 * @date 2026-07-19
 *
 * The pool maps each calling thread to its own shard (a private Arena), so
 * allocation and same-thread free run the arena's unsynchronized hot path
 * with no lock and no contention. Freeing a block on a different thread than
 * the one that allocated it pushes the block onto the owning shard's
 * remote-free queue with one atomic operation; the owner reclaims queued
 * blocks at its next allocation.
 *
 * Shard lifecycle: a thread's first allocation adopts an orphaned shard if
 * one exists, otherwise creates a new one. When a thread exits, its shard is
 * orphaned automatically (thread-specific-storage destructor): the shard's
 * memory stays valid, so allocations that outlive the thread remain usable
 * and freeable, and a future thread adopts the shard. Shards are destroyed
 * only by release_arena_pool.
 *
 * Thread safety: all functions here may be called concurrently from any
 * thread, except release_arena_pool, which requires that no other thread is
 * still using the pool.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <stddef.h> // size_t

#if defined(__cplusplus)
extern "C" {
#endif

#define ARENA_POOL_MALLOC_THROW(P, BYTES) \
    arena_pool_malloc_throw((P), (BYTES), __FILE__, __LINE__)
#define ARENA_POOL_CALLOC_THROW(P, COUNT, BYTES) \
    arena_pool_calloc_throw((P), (COUNT), (BYTES), __FILE__, __LINE__)
#define ARENA_POOL_REALLOC_THROW(P, PTR, BYTES) \
    arena_pool_realloc_throw((P), (PTR), (BYTES), __FILE__, __LINE__)
#define ARENA_POOL_FREE_THROW(P, PTR) \
    arena_pool_free_throw((P), (PTR), __FILE__, __LINE__)

/// Opaque handle to a sharded allocation pool.
typedef struct ArenaPool ArenaPool;

/// Thrown when a thread needs a shard but every slot is active or orphaned.
extern char const *arena_pool_exhausted;

/**
 * @brief Create a pool. Each shard is created lazily with new_arena using
 * these commit/reserve parameters (see new_arena for their meaning).
 *
 * @throw arena_out_of_memory, fl_internal_error
 */
ArenaPool *new_arena_pool(size_t shard_commit, unsigned int shard_reserve);

/**
 * @brief Destroy the pool and every shard, and set *pool to NULL.
 *
 * All memory allocated from the pool becomes invalid. No other thread may be
 * using the pool, and threads that used it should have exited or stopped
 * calling into it.
 */
void release_arena_pool(ArenaPool **pool);

/**
 * @brief Allocate from the calling thread's shard, creating or adopting the
 * shard on first use.
 *
 * @throw arena_out_of_memory, arena_pool_exhausted, fl_internal_error
 */
void *arena_pool_malloc_throw(ArenaPool *pool, size_t size, char const *file, int line);

/// Zeroed allocation from the calling thread's shard.
void *arena_pool_calloc_throw(ArenaPool *pool, size_t count, size_t size,
                              char const *file, int line);

/**
 * @brief Resize a block. If the calling thread owns the block's shard this is
 * a normal realloc; otherwise the new block comes from the caller's shard,
 * the contents are copied, and the old block is queued to its owner.
 */
void *arena_pool_realloc_throw(ArenaPool *pool, void *mem, size_t size,
                               char const *file, int line);

/**
 * @brief Free a block allocated from any shard of any pool thread.
 *
 * A block freed on its owning thread takes the arena's normal free path; a
 * block freed on any other thread is pushed to the owner's remote-free queue
 * (lock-free, unvalidated) and reclaimed at the owner's next allocation.
 */
void arena_pool_free_throw(ArenaPool *pool, void *mem, char const *file, int line);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_ARENA_POOL_H_
