#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "fl_exception_service.c"
#include "fl_threads.c" // mtx_init, mtx_lock, mtx_unlock, mtx_destroy
#include "fla_exception_service.c"
#include "fla_log_service.c"
#include "fla_memory_service.c"
#include "fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"
#include "set.c"
#include "fault_injector.c"
#include "flp_memory_service.c"
#include "flp_fault_memory_service.c"
#include <faultline/fl_test.h>

// ── Fault-injecting service fixtures ─────────────────────────────────────────

typedef struct MemServiceTestCase {
    FLTestCase           tc;
    FLFaultMemoryContext ctx;
    FaultInjector        fi;
} MemServiceTestCase;

FL_SETUP_FN(flp_setup) {
    MemServiceTestCase *mtc   = FL_CONTAINER_OF(tc, MemServiceTestCase, tc);
    Arena              *arena = new_arena(0, 0);
    FL_TRY {
        fault_injector_init(&mtc->fi, arena);
        flp_init_fault_memory_context(&mtc->ctx, arena, &mtc->fi);
        flp_init_fault_memory_service(fla_set_memory_service, &mtc->ctx);
    }
    FL_CATCH_ALL {
        release_arena(&arena);
        FL_RETHROW;
    }
    FL_END_TRY;
}

FL_CLEANUP_FN(flp_cleanup) {
    MemServiceTestCase *mtc = FL_CONTAINER_OF(tc, MemServiceTestCase, tc);
    fault_injector_uninit(&mtc->fi);
    release_arena(&mtc->ctx.arena);
}

// ── Arena-only service fixtures ───────────────────────────────────────────────

typedef struct ArenaServiceTestCase {
    FLTestCase      tc;
    FLMemoryContext ctx;
} ArenaServiceTestCase;

FL_SETUP_FN(arena_setup) {
    ArenaServiceTestCase *atc   = FL_CONTAINER_OF(tc, ArenaServiceTestCase, tc);
    Arena                *arena = new_arena(0, 0);
    FL_TRY {
        flp_init_memory_context(&atc->ctx, arena);
        flp_init_memory_service(fla_set_memory_service, &atc->ctx);
    }
    FL_CATCH_ALL {
        release_arena(&arena);
        FL_RETHROW;
    }
    FL_END_TRY;
}

FL_CLEANUP_FN(arena_cleanup) {
    ArenaServiceTestCase *atc = FL_CONTAINER_OF(tc, ArenaServiceTestCase, tc);
    release_arena(&atc->ctx.arena);
}

// ── Compatibility fixtures ────────────────────────────────────────────────────

typedef struct CompatTestCase {
    FLTestCase           tc;
    Arena               *arena;
    FLMemoryContext      arena_ctx;
    FLFaultMemoryContext fault_ctx;
    FaultInjector        fi;
} CompatTestCase;

FL_SETUP_FN(compat_setup) {
    CompatTestCase *ctc = FL_CONTAINER_OF(tc, CompatTestCase, tc);
    ctc->arena          = new_arena(0, 0);
    FL_TRY {
        flp_init_memory_context(&ctc->arena_ctx, ctc->arena);
        fault_injector_init(&ctc->fi, ctc->arena);
        flp_init_fault_memory_context(&ctc->fault_ctx, ctc->arena, &ctc->fi);
    }
    FL_CATCH_ALL {
        release_arena(&ctc->arena);
        FL_RETHROW;
    }
    FL_END_TRY;
}

FL_CLEANUP_FN(compat_cleanup) {
    CompatTestCase *ctc = FL_CONTAINER_OF(tc, CompatTestCase, tc);
    fault_injector_uninit(&ctc->fi);
    release_arena(&ctc->arena);
}

// ── Tests: fault-injecting service ───────────────────────────────────────────

FL_TYPE_TEST_SETUP_CLEANUP("Initialize Fault Service", MemServiceTestCase,
                           initialize_fault_service, flp_setup, flp_cleanup) {
    FL_UNUSED_TYPE_ARG;
    FL_ASSERT_NOT_NULL(g_fla_memory_service.ctx);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_aligned_alloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_calloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_free);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_malloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_realloc);
}

// ── Tests: arena-only service ─────────────────────────────────────────────────

FL_TYPE_TEST_SETUP_CLEANUP("Initialize Arena Service", ArenaServiceTestCase,
                           initialize_arena_service, arena_setup, arena_cleanup) {
    FL_UNUSED_TYPE_ARG;
    FL_ASSERT_NOT_NULL(g_fla_memory_service.ctx);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_aligned_alloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_calloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_free);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_malloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_realloc);
}

// Verify that a successful arena-only allocation round-trips through the service.
FL_TYPE_TEST_SETUP_CLEANUP("Arena Malloc And Free", ArenaServiceTestCase,
                           arena_malloc_and_free, arena_setup, arena_cleanup) {
    FL_UNUSED_TYPE_ARG;
    void *ptr = malloc(64);
    FL_ASSERT_NOT_NULL(ptr);
    free(ptr);
}

// Verify that an arena-only OOM returns NULL rather than propagating
// arena_out_of_memory as a thrown exception. A request for 1 TiB is
// guaranteed to exhaust any real arena without risking size_t overflow
// in the arena's internal calculations.
FL_TYPE_TEST_SETUP_CLEANUP("Arena Malloc OOM Returns Null", ArenaServiceTestCase,
                           arena_malloc_oom_returns_null, arena_setup, arena_cleanup) {
    FL_UNUSED_TYPE_ARG;
    void *ptr = malloc((size_t)1 << 40);
    FL_ASSERT_NULL(ptr);
}

// ── Tests: compatibility ──────────────────────────────────────────────────────

// Verify that the same application code works correctly when the arena-only
// and fault-injecting services are injected in turn, and that the two
// implementations install different function pointers.
FL_TYPE_TEST_SETUP_CLEANUP("Arena And Fault Service Compatibility", CompatTestCase,
                           arena_fault_service_compatibility, compat_setup,
                           compat_cleanup) {
    // Phase 1: inject arena-only service
    flp_init_memory_service(fla_set_memory_service, &t->arena_ctx);
    fl_malloc_fn *arena_malloc_fn = g_fla_memory_service.fl_malloc;

    void *p1 = malloc(64);
    FL_ASSERT_NOT_NULL(p1);
    free(p1);

    // Phase 2: inject fault-injecting service, same operations
    flp_init_fault_memory_service(fla_set_memory_service, &t->fault_ctx);
    fl_malloc_fn *fault_malloc_fn = g_fla_memory_service.fl_malloc;

    void *p2 = malloc(64);
    FL_ASSERT_NOT_NULL(p2);
    free(p2);

    // The two implementations must install distinct function pointers
    FL_ASSERT_TRUE(arena_malloc_fn != fault_malloc_fn);
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(initialize_fault_service)
FL_SUITE_ADD_EMBEDDED(initialize_arena_service)
FL_SUITE_ADD_EMBEDDED(arena_malloc_and_free)
FL_SUITE_ADD_EMBEDDED(arena_malloc_oom_returns_null)
FL_SUITE_ADD_EMBEDDED(arena_fault_service_compatibility)
FL_SUITE_END;

FL_GET_TEST_SUITE("Memory Service", ts)
