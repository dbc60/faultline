/**
 * @file flp_memory_service.c
 * @author Douglas Cuthbertson
 * @brief Arena-only platform memory service implementation.
 * @version 0.2
 * @date 2026-02-23
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_memory_service.h>           // FLMemoryService, FL_*_FN macros
#include <faultline/arena.h>                       // arena_realloc_throw
#include <faultline/arena_malloc.h>                // arena_malloc/calloc/free/..., arena_out_of_memory
#include <flp_memory_service.h>                    // FLP_INIT_MEMORY_SERVICE_FN
#include <stddef.h>                                // NULL
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_NOT_NULL
#include <faultline/fl_try.h>                      // FL_TRY, FL_CATCH, FL_END_TRY
#include "flp_memory_context.h"                    // FLMemoryContext, FLP_INIT_MEMORY_CONTEXT_FN
#include "region.h"                                // region_initialization_failure, region_out_of_memory

static FLMemoryService g_memory_service = {
    .ctx              = NULL,
    .fl_aligned_alloc = flp_aligned_alloc,
    .fl_calloc        = flp_calloc,
    .fl_free          = flp_free,
    .fl_malloc        = flp_malloc,
    .fl_realloc       = flp_realloc,
};

// ── Context init ──────────────────────────────────────────────────────────────

FLP_INIT_MEMORY_CONTEXT_FN(flp_init_memory_context) {
    FL_ASSERT_NOT_NULL(ctx);
    FL_ASSERT_NOT_NULL(arena);

    ctx->arena = arena;
}

// ── Service init ──────────────────────────────────────────────────────────────

FLP_INIT_MEMORY_SERVICE_FN(flp_init_memory_service) {
    FL_ASSERT_NOT_NULL(fla_set);
    FL_ASSERT_NOT_NULL(ctx);

    g_memory_service.ctx = ctx;
    fla_set(&g_memory_service, sizeof g_memory_service);
}

// ── Allocator implementations ─────────────────────────────────────────────────
// Delegate to the non-throwing arena_malloc.c wrappers, which are the canonical
// layer for translating all arena/region OOM exceptions into a NULL return.

FL_ALIGNED_ALLOC_FN(flp_aligned_alloc) {
    return arena_aligned_alloc(g_memory_service.ctx->arena, alignment, size, file, line);
}

FL_CALLOC_FN(flp_calloc) {
    return arena_calloc(g_memory_service.ctx->arena, nmemb, size, file, line);
}

FL_FREE_FN(flp_free) {
    arena_free(g_memory_service.ctx->arena, ptr, file, line);
}

FL_FREE_POINTER_FN(flp_free_pointer) {
    arena_free_pointer(g_memory_service.ctx->arena, ptr, file, line);
}

FL_MALLOC_FN(flp_malloc) {
    return arena_malloc(g_memory_service.ctx->arena, size, file, line);
}

FL_REALLOC_FN(flp_realloc) {
    return arena_realloc(g_memory_service.ctx->arena, ptr, size, file, line);
}
