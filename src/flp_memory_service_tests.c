#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "fl_exception_service.c"
#include "fla_exception_service.c"
#include "fla_log_service.c"
#include "fla_memory_service.c"
#include "../third_party/fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "set.c"
#include "fault_injector.c"
#include "flp_memory_service.c"
#include <faultline/fl_test.h>

typedef struct MemServiceTestCase {
    FLTestCase      tc;
    FLMemoryContext ctx;
    FaultInjector   fi;
} MemServiceTestCase;

FL_SETUP_FN(flp_setup) {
    MemServiceTestCase *mtc   = FL_CONTAINER_OF(tc, MemServiceTestCase, tc);
    Arena              *arena = new_arena(0, 0);
    FL_TRY {
        fault_injector_init(&mtc->fi, arena);
        flp_init_memory_context(&mtc->ctx, arena, &mtc->fi);
        flp_init_memory_service(fla_set_memory_service, &mtc->ctx);
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

FL_TYPE_TEST_SETUP_CLEANUP("Initialize Memory Service", MemServiceTestCase,
                           initialize_memory_service, flp_setup, flp_cleanup) {
    FL_UNUSED_TYPE_ARG;
    FL_ASSERT_NOT_NULL(g_fla_memory_service.ctx);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_aligned_alloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_calloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_free);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_malloc);
    FL_ASSERT_NOT_NULL(g_fla_memory_service.fl_realloc);
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(initialize_memory_service)
FL_SUITE_END;

FL_GET_TEST_SUITE("Memory Service", ts)
