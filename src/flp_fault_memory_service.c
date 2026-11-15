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
#include <faultline/fl_memory_service.h>        // FLMemoryService, FL_*_FN macros
#include <faultline/arena.h>                    // Arena
#include <faultline/arena_malloc.h>             // arena_malloc, arena_free
#include <flp_memory_service.h>                 // FLP_INIT_FAULT_MEMORY_SERVICE_FN
#include <stdbool.h>                            // bool
#include <stddef.h>                             // NULL, size_t
#include <stdint.h>                             // SIZE_MAX
#include <string.h>                             // memcpy, memset
#include "fault_injector_internal.h"            // fault_injector_*
#include <faultline/fl_exception_assert.h>      // FL_ASSERT_NOT_NULL
#include <faultline/fl_try.h>                   // FL_TRY, FL_CATCH, FL_END_TRY
#include <faultline/flp_fault_memory_context.h> // FLFaultMemoryContext,
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

// -- Guard bytes ---------------------------------------------------------------
//
// Each block this service returns lies between two runs of guard bytes, after a header
// that records what the free path needs:
//
//   base        user - GUARD_FRONT_SIZE     user          user + request
//   | padding   | GuardHeader | front guard | payload     | rear guard |
//
// A write to a front guard byte is an underflow; a write to a rear guard byte is an
// overflow. A freed payload is filled with GUARD_FREED_BYTE and held in the context's
// quarantine, so a write to it before the block returns to the arena is a use after
// free. The checks run when a block is freed, when it leaves the quarantine, and when
// the driver ends a pass (flp_fault_check).

typedef struct GuardHeader {
    void       *user;    ///< the payload's address, unless a write overwrote it
    void       *base;    ///< the address the arena returned
    size_t      request; ///< the size the caller asked for
    char const *file;    ///< the allocation site; the free site once the block is freed
    int         line;    ///< the line of file
} GuardHeader;

enum {
    GUARD_ALIGNMENT        = 16,   ///< the alignment the front run keeps
    GUARD_MIN_FRONT        = 16,   ///< the fewest guard bytes before the payload
    GUARD_REAR_SIZE        = 16,   ///< the guard bytes after the payload
    GUARD_BYTE             = 0xFD, ///< the value of each guard byte
    GUARD_FREED_BYTE       = 0xDD, ///< the value of each byte of a freed payload
    GUARD_MALLOC_ALIGNMENT = 0,    ///< an alignment that selects arena_malloc
};

/// The bytes from the header to the payload: the header and at least GUARD_MIN_FRONT
/// guard bytes, rounded up to GUARD_ALIGNMENT so the payload keeps the base's alignment.
#define GUARD_FRONT_SIZE                                                               \
    (((sizeof(GuardHeader) + GUARD_MIN_FRONT + GUARD_ALIGNMENT - 1) / GUARD_ALIGNMENT) \
     * GUARD_ALIGNMENT)

/// Which bytes guard_verify expects in the payload.
typedef enum GuardState {
    GUARD_LIVE,  ///< any bytes; the payload belongs to the caller
    GUARD_FREED, ///< every byte is GUARD_FREED_BYTE
} GuardState;

static GuardHeader *guard_header(unsigned char *user) {
    return (GuardHeader *)(user - GUARD_FRONT_SIZE);
}

// Return true if each of the n bytes at p equals value.
static bool guard_bytes_hold(unsigned char const *p, size_t n, unsigned char value) {
    size_t i = 0;
    while (i < n && p[i] == value) {
        i++;
    }

    return i == n;
}

// The bytes from base to the payload. An alignment above GUARD_ALIGNMENT rounds the
// front up to a multiple of it, so the payload keeps the alignment of base.
static size_t guard_front_size(size_t alignment) {
    size_t front = GUARD_FRONT_SIZE;
    if (alignment > GUARD_ALIGNMENT) {
        front = ((front + alignment - 1) / alignment) * alignment;
    }

    return front;
}

// Write the header and both runs of guard bytes around a request at base + front.
static unsigned char *guard_place(unsigned char *base, size_t front, size_t request,
                                  char const *file, int line) {
    unsigned char *user = base + front;
    GuardHeader   *hdr  = guard_header(user);

    memset(hdr + 1, GUARD_BYTE, GUARD_FRONT_SIZE - sizeof *hdr);
    memset(user + request, GUARD_BYTE, GUARD_REAR_SIZE);

    hdr->user    = user;
    hdr->base    = base;
    hdr->request = request;
    hdr->file    = file;
    hdr->line    = line;
    return user;
}

/**
 * @brief Record a fault for each guard a write changed.
 *
 * @return false if the header was overwritten, so the block's base and size are unknown
 * and the block cannot return to the arena.
 */
static bool guard_verify(FLFaultMemoryContext *ctx, unsigned char *user,
                         GuardState state) {
    GuardHeader *hdr    = guard_header(user);
    bool const   intact = hdr->user == user;
    char const  *site   = state == GUARD_LIVE ? "the block was allocated at this site"
                                              : "the block was freed at this site";

    bool const front_holds = guard_bytes_hold((unsigned char const *)(hdr + 1),
                                              GUARD_FRONT_SIZE - sizeof *hdr, GUARD_BYTE);
    if (!intact) {
        fault_injector_put_fault(ctx->fi, FL_BUFFER_UNDERFLOW, user,
                                 "write before the start of a block",
                                 "the write overwrote the block's header", "unknown", 0);
    } else if (!front_holds) {
        fault_injector_put_fault(ctx->fi, FL_BUFFER_UNDERFLOW, user,
                                 "write before the start of a block", site, hdr->file,
                                 hdr->line);
    }

    if (intact && !guard_bytes_hold(user + hdr->request, GUARD_REAR_SIZE, GUARD_BYTE)) {
        fault_injector_put_fault(ctx->fi, FL_BUFFER_OVERFLOW, user,
                                 "write past the end of a block", site, hdr->file,
                                 hdr->line);
    }

    if (intact && state == GUARD_FREED
        && !guard_bytes_hold(user, hdr->request, GUARD_FREED_BYTE)) {
        fault_injector_put_fault(ctx->fi, FL_USE_AFTER_FREE, user,
                                 "write to a block after it was freed", site, hdr->file,
                                 hdr->line);
    }

    return intact;
}

// -- Quarantine ----------------------------------------------------------------

// Check a block that leaves the quarantine and return it to the arena. A block whose
// header was overwritten stays out of the arena, because its base is unknown.
static void quarantine_release(FLFaultMemoryContext *ctx, unsigned char *user) {
    if (guard_verify(ctx, user, GUARD_FREED)) {
        arena_free(ctx->arena, guard_header(user)->base, __FILE__, __LINE__);
    }
}

// Release the oldest block in the quarantine.
static void quarantine_evict(FLFaultMemoryContext *ctx) {
    unsigned char *user = (unsigned char *)ctx->quarantine[ctx->quarantine_head];

    ctx->quarantine_head = (ctx->quarantine_head + 1) % FL_FAULT_QUARANTINE_CAPACITY;
    ctx->quarantine_count--;
    quarantine_release(ctx, user);
}

// Fill a freed payload, record the free site, and hold the block. A full quarantine
// releases its oldest block first.
static void quarantine_put(FLFaultMemoryContext *ctx, unsigned char *user,
                           char const *file, int line) {
    GuardHeader *hdr = guard_header(user);
    memset(user, GUARD_FREED_BYTE, hdr->request);
    hdr->file = file;
    hdr->line = line;

    if (ctx->quarantine_count == FL_FAULT_QUARANTINE_CAPACITY) {
        quarantine_evict(ctx);
    }

    size_t const tail
        = (ctx->quarantine_head + ctx->quarantine_count) % FL_FAULT_QUARANTINE_CAPACITY;
    ctx->quarantine[tail] = user;
    ctx->quarantine_count++;
}

// -- Pass-end check ------------------------------------------------------------

static void check_live_block(void const *value, void *ctx) {
    InjectorResource const *ir = (InjectorResource const *)value;
    (void)guard_verify((FLFaultMemoryContext *)ctx, (unsigned char *)ir->value,
                       GUARD_LIVE);
}

// Verify every live block and release every quarantined one, so the faults this records
// belong to the pass that just ended.
static void flp_fault_check(void *ctx_) {
    FLFaultMemoryContext *ctx = (FLFaultMemoryContext *)ctx_;

    fault_injector_foreach_allocated(ctx->fi, check_live_block, ctx);
    while (ctx->quarantine_count > 0) {
        quarantine_evict(ctx);
    }
}

// -- Context init --------------------------------------------------------------

FLP_INIT_FAULT_MEMORY_CONTEXT_FN(flp_init_fault_memory_context) {
    FL_ASSERT_NOT_NULL(ctx);
    FL_ASSERT_NOT_NULL(arena);
    FL_ASSERT_NOT_NULL(fi);

    ctx->arena            = arena;
    ctx->fi               = fi;
    ctx->quarantine_head  = 0;
    ctx->quarantine_count = 0;
    fault_injector_set_check(fi, flp_fault_check, ctx);
}

// -- Service init --------------------------------------------------------------

FLP_INIT_FAULT_MEMORY_SERVICE_FN(flp_init_fault_memory_service) {
    FL_ASSERT_NOT_NULL(fla_set);
    FL_ASSERT_NOT_NULL(ctx);

    g_fault_ctx                = ctx;
    g_fault_memory_service.ctx = (FLMemoryContext *)ctx;
    fla_set(&g_fault_memory_service, sizeof g_fault_memory_service);
}

// -- Allocator implementations -------------------------------------------------

/**
 * @brief Count an allocation site, then allocate a guarded block.
 *
 * @param alignment the payload's alignment, or GUARD_MALLOC_ALIGNMENT for arena_malloc's
 * @return the payload, or NULL when the injector fails the site or the arena has no room
 */
static void *guard_alloc(size_t alignment, size_t request, char const *file, int line) {
    void volatile *mem = NULL;

    FL_TRY {
        fault_injector_try_throw_with_site(g_fault_ctx->fi, file, line);

        size_t const front = guard_front_size(alignment);
        if (request <= SIZE_MAX - front - GUARD_REAR_SIZE) {
            size_t const   total = front + request + GUARD_REAR_SIZE;
            unsigned char *base
                = alignment == GUARD_MALLOC_ALIGNMENT
                      ? (unsigned char *)arena_malloc(g_fault_ctx->arena, total, file,
                                                      line)
                      : (unsigned char *)arena_aligned_alloc(g_fault_ctx->arena,
                                                             alignment, total, file,
                                                             line);
            if (base != NULL) {
                mem = guard_place(base, front, request, file, line);
                fault_injector_record_allocate(g_fault_ctx->fi, mem, file, line);
            }
        }
    }
    FL_CATCH(fault_injector_exception) {
        mem = NULL;
    }
    FL_END_TRY;

    return (void *)mem;
}

FL_ALIGNED_ALLOC_FN(flp_fault_aligned_alloc) {
    return guard_alloc(alignment, size, file, line);
}

FL_CALLOC_FN(flp_fault_calloc) {
    // A product that overflows asks for SIZE_MAX bytes, which guard_alloc cannot place.
    bool const   fits  = size == 0 || nmemb <= SIZE_MAX / size;
    size_t const total = fits ? nmemb * size : SIZE_MAX;
    void        *mem   = guard_alloc(GUARD_MALLOC_ALIGNMENT, total, file, line);

    if (mem != NULL) {
        memset(mem, 0, total);
    }

    return mem;
}

// A block that is not allocated is passed to fault_injector_record_free, which records a
// double free or an invalid free; its memory is not touched.
FL_FREE_FN(flp_fault_free) {
    // If mem is a NULL pointer, the function does nothing.
    if (ptr == NULL) {
        return;
    }

    FaultInjector *fi = g_fault_ctx->fi;
    bool const intact = fault_injector_is_allocated(fi, ptr)
                        && guard_verify(g_fault_ctx, (unsigned char *)ptr, GUARD_LIVE);

    fault_injector_record_free(fi, ptr, file, line);
    if (intact) {
        quarantine_put(g_fault_ctx, (unsigned char *)ptr, file, line);
    }
}

FL_FREE_POINTER_FN(flp_fault_free_pointer) {
    FL_ASSERT_NOT_NULL(ptr);
    flp_fault_free(*ptr, file, line);
    *ptr = NULL;
}

FL_MALLOC_FN(flp_fault_malloc) {
    return guard_alloc(GUARD_MALLOC_ALIGNMENT, size, file, line);
}

// The block always moves, so the old block goes through the free path's checks and the
// quarantine. When the new block cannot be allocated, the old block stays allocated.
FL_REALLOC_FN(flp_fault_realloc) {
    void *mem = NULL;

    if (ptr == NULL) {
        mem = flp_fault_malloc(size, file, line);
    } else if (!fault_injector_is_allocated(g_fault_ctx->fi, ptr)) {
        fault_injector_record_free(g_fault_ctx->fi, ptr, file, line);
    } else {
        mem = flp_fault_malloc(size, file, line);
        if (mem != NULL) {
            GuardHeader *hdr = guard_header((unsigned char *)ptr);
            if (hdr->user == ptr) {
                memcpy(mem, ptr, hdr->request < size ? hdr->request : size);
            }
            flp_fault_free(ptr, file, line);
        }
    }

    return mem;
}
