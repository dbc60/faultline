/**
 * @file flp_memory_service.c
 * @author Douglas Cuthbertson
 * @brief Platform side of the memory service implementation
 * @version 0.2
 * @date 2026-02-23
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_log.h>                      // for LOG_DEBUG
#include <faultline/fl_memory_service.h>           // for FLMemoryService, FL_ALIGNED...
#include <faultline/arena.h>                       // for arena_aligned_alloc_throw
#include <faultline/arena_malloc.h>                // for arena_out_of_memory
#include <flp_memory_service.h>                    // for FLP_INIT_MEMORY_SERVICE_FN
#include <stddef.h>                                // for NULL
#include "chunk.h"                                 // for FREE_CHUNK_FROM_MEMORY
#include "fault_injector_internal.h"               // for fault_injector_put_invalid_...
#include <faultline/fl_exception_service.h>        // for FL_REASON, FL_FILE, FL_LINE
#include <faultline/fl_exception_service_assert.h> // for FL_ASSERT_NOT_NULL
#include <faultline/fl_try.h>                      // for FL_CATCH, FL_END_TRY, FL_TRY
#include "flp_memory_context.h"                    // for FLMemoryContext, FLP_INIT_M...

/**
 * @brief Platform-global state for the allocation functions.
 */
static FLMemoryService g_memory_service = {
    .ctx              = NULL,
    .fl_aligned_alloc = flp_aligned_alloc,
    .fl_calloc        = flp_calloc,
    .fl_free          = flp_free,
    .fl_malloc        = flp_malloc,
    .fl_realloc       = flp_realloc,
};

// ── Service-function implementations ────────────────────────────────────────

FLP_INIT_MEMORY_SERVICE_FN(flp_init_memory_service) {
    FL_ASSERT_NOT_NULL(fla_set);
    FL_ASSERT_NOT_NULL(ctx);

    g_memory_service.ctx = ctx;
    fla_set(&g_memory_service, sizeof g_memory_service);
}

FLP_INIT_MEMORY_CONTEXT_FN(flp_init_memory_context) {
    FL_ASSERT_NOT_NULL(ctx);
    FL_ASSERT_NOT_NULL(arena);
    FL_ASSERT_NOT_NULL(fi);

    ctx->arena = arena;
    ctx->fi    = fi;
}

FL_ALIGNED_ALLOC_FN(flp_aligned_alloc) {
    void volatile *mem = NULL;

    FL_TRY {
        fault_injector_try_throw_with_site(g_memory_service.ctx->fi, file, line);

        mem = arena_aligned_alloc_throw(g_memory_service.ctx->arena, alignment, size,
                                        file, line);
        fault_injector_record_allocate(g_memory_service.ctx->fi, mem, file, line);
    }
    FL_CATCH(arena_out_of_memory) {
        mem = NULL;
    }
    FL_CATCH(fault_injector_exception) {
        mem = NULL;
    }
    FL_END_TRY;

    return (void *)mem;
}

FL_CALLOC_FN(flp_calloc) {
    void volatile *mem = NULL;

    FL_TRY {
        fault_injector_try_throw_with_site(g_memory_service.ctx->fi, file, line);

        mem = arena_calloc_throw(g_memory_service.ctx->arena, nmemb, size, file, line);
        fault_injector_record_allocate(g_memory_service.ctx->fi, mem, file, line);
    }
    FL_CATCH(arena_out_of_memory) {
        mem = NULL;
    }
    FL_CATCH(fault_injector_exception) {
        // simulate out-of-memory to see if the caller handles it correctly.
        mem = NULL;
    }
    FL_END_TRY;

    return (void *)mem;
}

FL_FREE_FN(flp_free) {
    FL_TRY {
        LOG_DEBUG("FAULT FREE", "releasing 0x%p", ptr);
        (void)FREE_CHUNK_FROM_MEMORY(ptr, file, line);

        // Address is valid - now actually free it.
        FL_TRY {
            arena_free_throw(g_memory_service.ctx->arena, ptr, file, line);
            // move the memory address from the allocated set to the released set
            fault_injector_record_free(g_memory_service.ctx->fi, ptr, file, line);
        }
        FL_CATCH_ALL {
            LOG_DEBUG("FAULT FREE", "do free exception");
            fault_injector_put_invalid_free(g_memory_service.ctx->fi, ptr, FL_REASON,
                                            FL_FILE, FL_LINE);
        }
        FL_END_TRY;
    }
    FL_CATCH(fl_invalid_address) {
        LOG_DEBUG("FAULT FREE", "recording invalid free: 0x%p, reason=%s, details=%s",
                  ptr, FL_REASON, FL_DETAILS);
        fault_injector_put_invalid_free(g_memory_service.ctx->fi, ptr, FL_REASON,
                                        FL_FILE, FL_LINE);
    }
    FL_END_TRY;
}

FL_FREE_POINTER_FN(flp_free_pointer) {
    FL_ASSERT_NOT_NULL(ptr);
    void *mem = *ptr;

    FL_TRY {
        (void)FREE_CHUNK_FROM_MEMORY((void *)mem, file, line);

        // Address is valid - now actually free it.
        FL_TRY {
            arena_free_pointer_throw(g_memory_service.ctx->arena, ptr, file, line);
            // move the memory address from the allocated set to the released set
            fault_injector_record_free(g_memory_service.ctx->fi, mem, file, line);
        }
        FL_CATCH_ALL {
            // record an error during free
            fault_injector_put_invalid_free(g_memory_service.ctx->fi, mem, FL_REASON,
                                            FL_FILE, FL_LINE);
        }
        FL_END_TRY;
    }
    FL_CATCH(fl_invalid_address) {
        fault_injector_put_invalid_free(g_memory_service.ctx->fi, mem, FL_REASON,
                                        FL_FILE, FL_LINE);
    }
    FL_END_TRY;
}

FL_MALLOC_FN(flp_malloc) {
    void volatile *mem = NULL;

    FL_TRY {
        fault_injector_try_throw_with_site(g_memory_service.ctx->fi, file, line);

        mem = arena_malloc_throw(g_memory_service.ctx->arena, size, file, line);
        fault_injector_record_allocate(g_memory_service.ctx->fi, mem, file, line);
    }
    FL_CATCH(arena_out_of_memory) {
        mem = NULL;
    }
    FL_CATCH(fault_injector_exception) {
        // simulate out-of-memory to see if the caller handles it correctly.
        mem = NULL;
    }
    FL_END_TRY;

    return (void *)mem;
}

FL_REALLOC_FN(flp_realloc) {
    void volatile *mem = ptr;

    FL_TRY {
        fault_injector_try_throw_with_site(g_memory_service.ctx->fi, file, line);

        mem = arena_realloc_throw(g_memory_service.ctx->arena, ptr, size, file, line);
        if (mem != ptr) {
            // remove the old address from the set and add the new one if it's not null
            fault_injector_record_free(g_memory_service.ctx->fi, ptr, file, line);
            if (mem != NULL) {
                fault_injector_record_allocate(g_memory_service.ctx->fi, mem, file,
                                               line);
            }
        }
    }
    FL_CATCH(arena_out_of_memory) {
        mem = NULL;
    }
    FL_CATCH(fault_injector_exception) {
        // simulate out-of-memory to see if the caller handles it correctly.
        mem = ptr;
    }
    FL_END_TRY;

    return (void *)mem;
}
