/**
 * @file arena_pool_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the sharded arena pool and its remote-free queues.
 * @version 0.1
 * @date 2026-07-19
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "arena.c"
#include "arena_dbg.c"
#include "arena_pool.c"
#include "digital_search_tree.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"
#include "lock_os.c"
#include "fl_exception_service.c"
#include "fl_threads.c"
#include "fla_exception_service.c"
#include "fla_log_service.c"

#include <faultline/arena.h>
#include <faultline/arena_pool.h>
#include <faultline/fl_test.h>
#include <faultline/fl_threads.h>

#include "atomic.h" // FL_ATOMIC and the C11 atomic_* calls
#include <string.h>

typedef struct TestPool {
    FLTestCase tc;
    ArenaPool *pool;
} TestPool;

FL_SETUP_FN(setup_pool) {
    TestPool *t = FL_CONTAINER_OF(tc, TestPool, tc);
    t->pool     = new_arena_pool(0, 0);
}

FL_CLEANUP_FN(cleanup_pool) {
    TestPool *t = FL_CONTAINER_OF(tc, TestPool, tc);
    release_arena_pool(&t->pool);
    FL_ASSERT_NULL(t->pool);
}

FL_TYPE_TEST_SETUP_CLEANUP("Roundtrip", TestPool, test_pool_roundtrip, setup_pool,
                           cleanup_pool) {
    void *mem = ARENA_POOL_MALLOC_THROW(t->pool, 64);
    memset(mem, 0xAB, 64);

    Arena *owner = arena_owner(mem);
    FL_ASSERT_NOT_NULL(owner);
    FL_ASSERT_EQ_SIZE_T(arena_allocation_count(owner), 1);

    ARENA_POOL_FREE_THROW(t->pool, mem);
    FL_ASSERT_EQ_SIZE_T(arena_allocation_count(owner), 0);

    // The same thread keeps the same shard.
    void *again = ARENA_POOL_MALLOC_THROW(t->pool, 64);
    FL_ASSERT_EQ_PTR(arena_owner(again), owner);
    ARENA_POOL_FREE_THROW(t->pool, again);
}

FL_TYPE_TEST_SETUP_CLEANUP("Calloc zeroes", TestPool, test_pool_calloc, setup_pool,
                           cleanup_pool) {
    unsigned char *mem = (unsigned char *)ARENA_POOL_CALLOC_THROW(t->pool, 8, 8);
    for (size_t i = 0; i < 64; i++) {
        FL_ASSERT_EQ_SIZE_T((size_t)mem[i], 0);
    }
    ARENA_POOL_FREE_THROW(t->pool, mem);
}

FL_TYPE_TEST_SETUP_CLEANUP("Realloc preserves", TestPool, test_pool_realloc, setup_pool,
                           cleanup_pool) {
    char *mem = (char *)ARENA_POOL_MALLOC_THROW(t->pool, 32);
    memset(mem, 0x5C, 32);

    char *grown = (char *)ARENA_POOL_REALLOC_THROW(t->pool, mem, 4096);
    for (size_t i = 0; i < 32; i++) {
        FL_ASSERT_EQ_SIZE_T((size_t)(unsigned char)grown[i], 0x5C);
    }
    ARENA_POOL_FREE_THROW(t->pool, grown);
}

/* ------------------------- cross-thread scenarios ------------------------ */

enum {
    POOL_TEST_BLOCKS = 100
};

typedef struct RemoteFreeArg {
    ArenaPool *pool;
    void      *blocks[POOL_TEST_BLOCKS];
} RemoteFreeArg;

static int remote_free_worker(void *arg) {
    RemoteFreeArg *rfa = (RemoteFreeArg *)arg;
    for (size_t i = 0; i < POOL_TEST_BLOCKS; i++) {
        // This thread never allocated, so every free takes the remote path.
        arena_pool_free_throw(rfa->pool, rfa->blocks[i], __FILE__, __LINE__);
    }
    return 0;
}

FL_TYPE_TEST_SETUP_CLEANUP("Remote free + absorb", TestPool, test_pool_remote_free,
                           setup_pool, cleanup_pool) {
    RemoteFreeArg rfa;
    rfa.pool = t->pool;
    for (size_t i = 0; i < POOL_TEST_BLOCKS; i++) {
        rfa.blocks[i] = ARENA_POOL_MALLOC_THROW(t->pool, 48 + 16 * (i % 8));
    }

    Arena *owner = arena_owner(rfa.blocks[0]);
    FL_ASSERT_EQ_SIZE_T(arena_allocation_count(owner), POOL_TEST_BLOCKS);

    thrd_t worker;
    FL_ASSERT_EQ_SIZE_T((size_t)thrd_create(&worker, remote_free_worker, &rfa),
                        (size_t)thrd_success);
    int res = -1;
    thrd_join(worker, &res);
    FL_ASSERT_EQ_SIZE_T((size_t)res, 0);

    // The queued blocks are reclaimed by this thread's next allocation.
    void *mem = ARENA_POOL_MALLOC_THROW(t->pool, 64);
    FL_ASSERT_EQ_PTR(arena_owner(mem), owner);
    FL_ASSERT_EQ_SIZE_T(arena_allocation_count(owner), 1);
    ARENA_POOL_FREE_THROW(t->pool, mem);
    FL_ASSERT_EQ_SIZE_T(arena_allocation_count(owner), 0);
}

typedef struct AdoptArg {
    ArenaPool *pool;
    Arena     *owner;    ///< set by the worker: its shard's arena
    void      *block;    ///< set by the first worker, freed by the second
    size_t     leftover; ///< set by the second worker: final allocation count
} AdoptArg;

static int adopt_first_worker(void *arg) {
    AdoptArg *aa = (AdoptArg *)arg;
    aa->block    = arena_pool_malloc_throw(aa->pool, 32, __FILE__, __LINE__);
    aa->owner    = arena_owner(aa->block);
    return 0;
}

static int adopt_second_worker(void *arg) {
    AdoptArg *aa  = (AdoptArg *)arg;
    void     *mem = arena_pool_malloc_throw(aa->pool, 32, __FILE__, __LINE__);
    aa->owner     = arena_owner(mem);
    arena_pool_free_throw(aa->pool, mem, __FILE__, __LINE__);
    // The first worker's block belongs to the shard this thread adopted, so
    // this free takes the local path.
    arena_pool_free_throw(aa->pool, aa->block, __FILE__, __LINE__);
    aa->leftover = arena_allocation_count(aa->owner);
    return 0;
}

FL_TYPE_TEST_SETUP_CLEANUP("Orphan + adopt", TestPool, test_pool_adoption, setup_pool,
                           cleanup_pool) {
    AdoptArg first;
    first.pool = t->pool;

    thrd_t worker;
    FL_ASSERT_EQ_SIZE_T((size_t)thrd_create(&worker, adopt_first_worker, &first),
                        (size_t)thrd_success);
    int res = -1;
    thrd_join(worker, &res);
    FL_ASSERT_EQ_SIZE_T((size_t)res, 0);
    FL_ASSERT_NOT_NULL(first.owner);

    AdoptArg second = first;
    FL_ASSERT_EQ_SIZE_T((size_t)thrd_create(&worker, adopt_second_worker, &second),
                        (size_t)thrd_success);
    thrd_join(worker, &res);
    FL_ASSERT_EQ_SIZE_T((size_t)res, 0);

    // The second thread adopted the orphaned shard rather than creating one.
    FL_ASSERT_EQ_PTR(second.owner, first.owner);
    FL_ASSERT_EQ_SIZE_T(second.leftover, 0);
}

typedef struct DistinctArg {
    ArenaPool  *pool;
    atomic_int *arrived;
    Arena      *owner;
} DistinctArg;

static int distinct_worker(void *arg) {
    DistinctArg *da  = (DistinctArg *)arg;
    void        *mem = arena_pool_malloc_throw(da->pool, 32, __FILE__, __LINE__);
    da->owner        = arena_owner(mem);

    // Hold the shard until both workers have one, so they cannot share.
    atomic_fetch_add(da->arrived, 1);
    while (atomic_load(da->arrived) < 2) {
        struct timespec nap = {.tv_sec = 0, .tv_nsec = 1000000};
        thrd_sleep(&nap, NULL);
    }

    arena_pool_free_throw(da->pool, mem, __FILE__, __LINE__);
    return 0;
}

FL_TYPE_TEST_SETUP_CLEANUP("Concurrent threads get distinct shards", TestPool,
                           test_pool_distinct_shards, setup_pool, cleanup_pool) {
    atomic_int  arrived = 0;
    DistinctArg args[2];
    thrd_t      workers[2];

    for (size_t i = 0; i < 2; i++) {
        args[i].pool    = t->pool;
        args[i].arrived = &arrived;
        args[i].owner   = NULL;
        FL_ASSERT_EQ_SIZE_T((size_t)thrd_create(&workers[i], distinct_worker, &args[i]),
                            (size_t)thrd_success);
    }
    for (size_t i = 0; i < 2; i++) {
        int res = -1;
        thrd_join(workers[i], &res);
        FL_ASSERT_EQ_SIZE_T((size_t)res, 0);
    }

    FL_ASSERT_NOT_NULL(args[0].owner);
    FL_ASSERT_NOT_NULL(args[1].owner);
    FL_ASSERT_NEQ_PTR(args[0].owner, args[1].owner);
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(test_pool_roundtrip)
FL_SUITE_ADD_EMBEDDED(test_pool_calloc)
FL_SUITE_ADD_EMBEDDED(test_pool_realloc)
FL_SUITE_ADD_EMBEDDED(test_pool_remote_free)
FL_SUITE_ADD_EMBEDDED(test_pool_adoption)
FL_SUITE_ADD_EMBEDDED(test_pool_distinct_shards)
FL_SUITE_END;
FL_GET_TEST_SUITE("Arena Pool", ts)
