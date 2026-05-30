/**
 * @file flp_fault_memory_service.c
 * @author Douglas Cuthbertson
 * @brief Fault-injecting platform memory service implementation.
 * @version 0.1
 * @date 2026-05-28
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_log.h>               // LOG_DEBUG
#include <faultline/fl_memory_service.h>    // FLMemoryService, FL_*_FN macros
#include <faultline/arena.h>                // arena_free_throw, arena_free_pointer_throw
#include <faultline/arena_malloc.h>         // arena_malloc, arena_calloc, etc.
#include <flp_memory_service.h>             // FLP_INIT_FAULT_MEMORY_SERVICE_FN
#include <stddef.h>                         // NULL
#include "chunk.h"                          // FREE_CHUNK_FROM_MEMORY
#include "fault_injector_internal.h"        // fault_injector_*
#include <faultline/fl_exception_service.h> // FL_REASON, FL_FILE, FL_LINE
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_NOT_NULL
#include <faultline/fl_try.h>                      // FL_TRY, FL_CATCH, FL_END_TRY
#include "flp_fault_memory_context.h"              // FLFaultMemoryContext,
                                                   //   FLP_INIT_FAULT_MEMORY_CONTEXT_FN

static FLFaultMemoryContext *g_fault_ctx = NULL;

static FLMemoryService g_fault_memory_service = {
    .ctx              = NULL,
    .fl_aligned_alloc = flp_fault_aligned_alloc,
    .fl_calloc        = flp_fault_calloc,
    .fl_free          = flp_fault_free,
    .fl_malloc        = flp_fault_malloc,
    .fl_realloc       = flp_fault_realloc,
};

// ── Context init ──────────────────────────────────────────────────────────────

FLP_INIT_FAULT_MEMORY_CONTEXT_FN(flp_init_fault_memory_context) {
    FL_ASSERT_NOT_NULL(ctx);
    FL_ASSERT_NOT_NULL(arena);
    FL_ASSERT_NOT_NULL(fi);

    ctx->arena = arena;
    ctx->fi    = fi;
}

// ── Service init ──────────────────────────────────────────────────────────────

FLP_INIT_FAULT_MEMORY_SERVICE_FN(flp_init_fault_memory_service) {
    FL_ASSERT_NOT_NULL(fla_set);
    FL_ASSERT_NOT_NULL(ctx);

    g_fault_ctx                = ctx;
    g_fault_memory_service.ctx = (FLMemoryContext *)ctx;
    fla_set(&g_fault_memory_service, sizeof g_fault_memory_service);
}

// ── Allocator implementations ─────────────────────────────────────────────────

FL_ALIGNED_ALLOC_FN(flp_fault_aligned_alloc) {
    void volatile *mem = NULL;

    FL_TRY {
        fault_injector_try_throw_with_site(g_fault_ctx->fi, file, line);
        mem = arena_aligned_alloc(g_fault_ctx->arena, alignment, size, file, line);
        if (mem != NULL) {
            fault_injector_record_allocate(g_fault_ctx->fi, mem, file, line);
        }
    }
    FL_CATCH(fault_injector_exception) {
        mem = NULL;
    }
    FL_END_TRY;

    return (void *)mem;
}

FL_CALLOC_FN(flp_fault_calloc) {
    void volatile *mem = NULL;

    FL_TRY {
        fault_injector_try_throw_with_site(g_fault_ctx->fi, file, line);
        mem = arena_calloc(g_fault_ctx->arena, nmemb, size, file, line);
        if (mem != NULL) {
            fault_injector_record_allocate(g_fault_ctx->fi, mem, file, line);
        }
    }
    FL_CATCH(fault_injector_exception) {
        mem = NULL;
    }
    FL_END_TRY;

    return (void *)mem;
}

FL_FREE_FN(flp_fault_free) {
    FL_TRY {
        LOG_DEBUG("FAULT FREE", "releasing 0x%p", ptr);
        (void)FREE_CHUNK_FROM_MEMORY(ptr, file, line);

        FL_TRY {
            arena_free_throw(g_fault_ctx->arena, ptr, file, line);
            fault_injector_record_free(g_fault_ctx->fi, ptr, file, line);
        }
        FL_CATCH(fl_invalid_address) {
            LOG_DEBUG("FAULT FREE", "do free exception");
            fault_injector_put_invalid_free(g_fault_ctx->fi, ptr, FL_REASON, file, line);
        }
        FL_END_TRY;
    }
    FL_CATCH(fl_invalid_address) {
        LOG_DEBUG("FAULT FREE", "recording invalid free: 0x%p, reason=%s, details=%s",
                  ptr, FL_REASON, FL_DETAILS);
        fault_injector_put_invalid_free(g_fault_ctx->fi, ptr, FL_REASON, file, line);
    }
    FL_END_TRY;
}

FL_FREE_POINTER_FN(flp_fault_free_pointer) {
    FL_ASSERT_NOT_NULL(ptr);
    void *mem = *ptr;

    FL_TRY {
        (void)FREE_CHUNK_FROM_MEMORY(mem, file, line);

        FL_TRY {
            arena_free_pointer_throw(g_fault_ctx->arena, ptr, file, line);
            fault_injector_record_free(g_fault_ctx->fi, mem, file, line);
        }
        FL_CATCH(fl_invalid_address) {
            fault_injector_put_invalid_free(g_fault_ctx->fi, mem, FL_REASON, file, line);
        }
        FL_END_TRY;
    }
    FL_CATCH(fl_invalid_address) {
        fault_injector_put_invalid_free(g_fault_ctx->fi, mem, FL_REASON, file, line);
    }
    FL_END_TRY;
}

FL_MALLOC_FN(flp_fault_malloc) {
    void volatile *mem = NULL;

    FL_TRY {
        fault_injector_try_throw_with_site(g_fault_ctx->fi, file, line);
        mem = arena_malloc(g_fault_ctx->arena, size, file, line);
        if (mem != NULL) {
            fault_injector_record_allocate(g_fault_ctx->fi, mem, file, line);
        }
    }
    FL_CATCH(fault_injector_exception) {
        mem = NULL;
    }
    FL_END_TRY;

    return (void *)mem;
}

FL_REALLOC_FN(flp_fault_realloc) {
    void volatile *mem = ptr;

    FL_TRY {
        fault_injector_try_throw_with_site(g_fault_ctx->fi, file, line);
        mem = arena_realloc(g_fault_ctx->arena, ptr, size, file, line);
        if (mem != NULL && mem != ptr) {
            fault_injector_record_free(g_fault_ctx->fi, ptr, file, line);
            fault_injector_record_allocate(g_fault_ctx->fi, mem, file, line);
        }
    }
    FL_CATCH(fault_injector_exception) {
        mem = NULL;
    }
    FL_END_TRY;

    return (void *)mem;
}
