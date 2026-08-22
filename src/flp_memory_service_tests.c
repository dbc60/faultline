#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "fl_exception_service.c"
#include "fla_exception_service.c"
#include "fla_log_service.c"
#include "fla_memory_service.c"
#include "fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "region_os.c"
#include "lock_os.c"
#include "set.c"
#include "fault_injector.c"
#include "flp_memory_service.c"
#include "flp_fault_memory_service.c"
#include <faultline/fl_test.h>
#include <stddef.h> // offsetof

// -- Fault-injecting service fixtures -----------------------------------------

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
    FL_CATCH_ALL_RETHROW {
        release_arena(&arena);
    }
    FL_END_TRY;
}

FL_CLEANUP_FN(flp_cleanup) {
    MemServiceTestCase *mtc = FL_CONTAINER_OF(tc, MemServiceTestCase, tc);
    fault_injector_uninit(&mtc->fi);
    release_arena(&mtc->ctx.arena);
}

// -- Arena-only service fixtures -----------------------------------------------

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
    FL_CATCH_ALL_RETHROW {
        release_arena(&arena);
    }
    FL_END_TRY;
}

FL_CLEANUP_FN(arena_cleanup) {
    ArenaServiceTestCase *atc = FL_CONTAINER_OF(tc, ArenaServiceTestCase, tc);
    release_arena(&atc->ctx.arena);
}

// -- Compatibility fixtures ----------------------------------------------------

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
    FL_CATCH_ALL_RETHROW {
        release_arena(&ctc->arena);
    }
    FL_END_TRY;
}

FL_CLEANUP_FN(compat_cleanup) {
    CompatTestCase *ctc = FL_CONTAINER_OF(tc, CompatTestCase, tc);
    fault_injector_uninit(&ctc->fi);
    release_arena(&ctc->arena);
}

// -- Tests: fault-injecting service -------------------------------------------

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

// -- Tests: arena-only service -------------------------------------------------

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
// arena_out_of_memory as a thrown exception. Half the size_t range exhausts any
// real arena on any address-space width, while staying below the arena's
// maximum request so the request is rejected for want of memory rather than for
// being out of range.
FL_TYPE_TEST_SETUP_CLEANUP("Arena Malloc OOM Returns Null", ArenaServiceTestCase,
                           arena_malloc_oom_returns_null, arena_setup, arena_cleanup) {
    FL_UNUSED_TYPE_ARG;
    void *ptr = malloc(HALF_MAX_SIZE_T);
    FL_ASSERT_NULL(ptr);
}

// -- Tests: compatibility ------------------------------------------------------

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

// -- Tests: contract layout ----------------------------------------------------

// FLMemoryService crosses the DLL boundary by value: the driver passes its own
// sizeof to fla_set_memory_service, which copies the struct through the suite's
// layout. Growth is therefore safe only when the new member is APPENDED -- a
// suite built against the older contract then reads a prefix that is identical
// in both layouts. An INSERTED member shifts every later slot, and the size
// guard cannot see it: a newer driver still satisfies "size >= sizeof", so an
// older suite silently calls the wrong function pointer through the right name.
//
// These assertions make growth deliberate. If one fires, decide which case you
// are in before updating the numbers: appending is a compatible change, and
// inserting requires every suite DLL to be rebuilt in lockstep.
FL_TEST("Memory Service Layout", memory_service_layout) {
    // All members are pointers, so the struct is pointer-sized throughout and
    // these hold on both x86 and x64.
    FL_ASSERT_EQ_SIZE_T(sizeof(FLMemoryService), 6 * sizeof(void *));

    FL_ASSERT_EQ_SIZE_T(offsetof(FLMemoryService, ctx), 0 * sizeof(void *));
    FL_ASSERT_EQ_SIZE_T(offsetof(FLMemoryService, fl_aligned_alloc), 1 * sizeof(void *));
    FL_ASSERT_EQ_SIZE_T(offsetof(FLMemoryService, fl_calloc), 2 * sizeof(void *));
    FL_ASSERT_EQ_SIZE_T(offsetof(FLMemoryService, fl_free), 3 * sizeof(void *));
    FL_ASSERT_EQ_SIZE_T(offsetof(FLMemoryService, fl_malloc), 4 * sizeof(void *));
    FL_ASSERT_EQ_SIZE_T(offsetof(FLMemoryService, fl_realloc), 5 * sizeof(void *));
}

// -- Tests: free_pointer -------------------------------------------------------
//
// The free_pointer variants are free() plus "clear the caller's pointer". They
// have no consumer inside this repo -- the worldbuilder host calls
// flp_free_pointer directly -- so these tests are the only thing holding their
// behavior in place.

// The arena-only provider clears the caller's pointer and returns the block.
FL_TYPE_TEST_SETUP_CLEANUP("Free Pointer Clears Pointer", ArenaServiceTestCase,
                           free_pointer_clears_pointer, arena_setup, arena_cleanup) {
    FL_UNUSED_TYPE_ARG;
    void *ptr = malloc(64);
    FL_ASSERT_NOT_NULL(ptr);

    flp_free_pointer(&ptr, __FILE__, __LINE__);
    FL_ASSERT_NULL(ptr);

    // The block returned to the arena is available again.
    void *again = malloc(64);
    FL_ASSERT_NOT_NULL(again);
    free(again);
}

// Freeing through a pointer that already holds NULL must not crash and must
// leave the pointer NULL. flp_free_pointer reaches arena_free_throw, which does
// not special-case NULL, so this also pins down that the resulting exception is
// contained by the provider rather than escaping to the caller.
FL_TYPE_TEST_SETUP_CLEANUP("Free Pointer Accepts Null Target", ArenaServiceTestCase,
                           free_pointer_accepts_null_target, arena_setup,
                           arena_cleanup) {
    FL_UNUSED_TYPE_ARG;
    void *ptr = NULL;
    flp_free_pointer(&ptr, __FILE__, __LINE__);
    FL_ASSERT_NULL(ptr);
}

// The fault-injecting provider clears the pointer the same way. malloc may
// legitimately return NULL when the injector forces this site to fail, so the
// free is guarded -- the point is that free_pointer behaves identically under
// either service.
FL_TYPE_TEST_SETUP_CLEANUP("Fault Free Pointer Clears Pointer", MemServiceTestCase,
                           fault_free_pointer_clears_pointer, flp_setup, flp_cleanup) {
    FL_UNUSED_TYPE_ARG;
    void *ptr = malloc(64);
    if (ptr != NULL) {
        flp_fault_free_pointer(&ptr, __FILE__, __LINE__);
    }
    FL_ASSERT_NULL(ptr);
}

// arena_free_pointer is the layer the providers delegate to. Freeing through it
// must leave the arena in the same state as arena_free plus a manual clear, so
// the two spellings are interchangeable at a call site.
FL_TYPE_TEST_SETUP_CLEANUP("Arena Free Pointer Matches Free", ArenaServiceTestCase,
                           arena_free_pointer_matches_free, arena_setup, arena_cleanup) {
    Arena *arena = t->ctx.arena;

    void *a = arena_malloc(arena, 64, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(a);
    arena_free(arena, a, __FILE__, __LINE__);
    size_t after_free = arena_allocation_count(arena);

    void *b = arena_malloc(arena, 64, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(b);
    arena_free_pointer(arena, &b, __FILE__, __LINE__);
    FL_ASSERT_NULL(b);

    FL_ASSERT_EQ_SIZE_T(arena_allocation_count(arena), after_free);
}

// -- Tests: realloc ------------------------------------------------------------

// C17 7.22.3.5: a null first argument makes realloc behave like malloc. The
// fault-injecting provider must record only the new allocation -- charging a
// release for the absent prior block fabricates an invalid free the caller never
// committed, and that fault fails an otherwise clean test.
FL_TYPE_TEST_SETUP_CLEANUP("Fault Realloc Accepts Null Pointer", MemServiceTestCase,
                           fault_realloc_accepts_null_pointer, flp_setup, flp_cleanup) {
    FL_ASSERT_ZERO_INT64(fault_injector_get_invalid_address_count(&t->fi));

    void *ptr = flp_fault_realloc(NULL, 64, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(ptr);
    FL_ASSERT_ZERO_INT64(fault_injector_get_invalid_address_count(&t->fi));

    // The block was recorded as a live allocation, so releasing it is a clean
    // free rather than an invalid one.
    flp_fault_free(ptr, __FILE__, __LINE__);
    FL_ASSERT_ZERO_INT64(fault_injector_get_invalid_address_count(&t->fi));
}

// Resizing a real block keeps the accounting balanced whether the block moves or
// is resized in place: the old address must not be left looking live, and the
// returned address must be the one that is tracked.
FL_TYPE_TEST_SETUP_CLEANUP("Fault Realloc Tracks Resized Block", MemServiceTestCase,
                           fault_realloc_tracks_resized_block, flp_setup, flp_cleanup) {
    void *ptr = flp_fault_malloc(64, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(ptr);

    void *grown = flp_fault_realloc(ptr, 4096, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(grown);
    FL_ASSERT_ZERO_INT64(fault_injector_get_invalid_address_count(&t->fi));

    flp_fault_free(grown, __FILE__, __LINE__);
    FL_ASSERT_ZERO_INT64(fault_injector_get_invalid_address_count(&t->fi));
}

// -- Cross-boundary injection --------------------------------------------------
//
// Unlike the tests above -- whose setups self-inject a service built from a
// DLL-local arena -- this one relies entirely on the driver. When it loads this
// DLL it resolves the exported fla_set_memory_service via GetProcAddress and
// installs the host's fault-injecting memory service into g_fla_memory_service
// before any test runs. A pass proves the symbol was exported and the service
// crossed the DLL boundary; were it missing,
// g_fla_memory_service would still hold the default_malloc / default_free abort
// stubs (static in the included fla_memory_service.c). The host installs its own
// allocator -- a different address from this DLL's copy -- so we assert the stubs
// were replaced rather than pointer identity, then exercise the injected allocator.
//
// Registered first so it observes the driver's injection before any self-injecting
// setup replaces g_fla_memory_service with a DLL-local one.

FL_TEST("Driver Injects Memory Service", driver_injects_memory_service) {
    FL_ASSERT_TRUE(g_fla_memory_service.fl_malloc != default_malloc);
    FL_ASSERT_TRUE(g_fla_memory_service.fl_free != default_free);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.ctx);

    // Exercise the injected allocator; if these were still the abort stubs the
    // calls would terminate the process. malloc may legitimately return NULL when
    // the fault injector forces this site to fail, so guard the free.
    void *ptr = malloc(64);
    if (ptr != NULL) {
        free(ptr);
    }
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD(driver_injects_memory_service)
FL_SUITE_ADD(memory_service_layout)
FL_SUITE_ADD_EMBEDDED(initialize_fault_service)
FL_SUITE_ADD_EMBEDDED(initialize_arena_service)
FL_SUITE_ADD_EMBEDDED(arena_malloc_and_free)
FL_SUITE_ADD_EMBEDDED(arena_malloc_oom_returns_null)
FL_SUITE_ADD_EMBEDDED(arena_fault_service_compatibility)
FL_SUITE_ADD_EMBEDDED(free_pointer_clears_pointer)
FL_SUITE_ADD_EMBEDDED(free_pointer_accepts_null_target)
FL_SUITE_ADD_EMBEDDED(fault_free_pointer_clears_pointer)
FL_SUITE_ADD_EMBEDDED(arena_free_pointer_matches_free)
FL_SUITE_ADD_EMBEDDED(fault_realloc_accepts_null_pointer)
FL_SUITE_ADD_EMBEDDED(fault_realloc_tracks_resized_block)
FL_SUITE_END;

FL_GET_TEST_SUITE("Memory Service", ts)
