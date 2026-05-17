/**
 * @file fla_memory_service.c
 * @author Douglas Cuthbertson
 * @brief Application side of the memory service implementation
 * @version 0.1
 * @date 2026-02-23
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
// CRT headers must come before fla_memory_service.h, which redefines malloc,
// calloc, free, realloc, and aligned_alloc as macros.  If the CRT headers are
// included after those macros are active, corecrt_malloc.h tries to declare
// e.g. "void* __cdecl malloc(size_t)" and the macro expansion produces an
// invalid declaration (C4229 / C2220 in MSVC).
#include <stdio.h>                       // fprintf
#include <stdlib.h>                      // abort, NULL
#include <faultline/fl_memory_service.h> // FL_ALIGNED_ALLOC_FN, FL_CALLOC_FN, ...
#include <faultline/fla_memory_service.h> // FLA_SET_MEMORY_SERVICE_FN, fla_set_memory_service
#include <faultline/fl_macros.h>          // FL_UNUSED

static FL_ALIGNED_ALLOC_FN(default_aligned_alloc) {
    FL_UNUSED(alignment);
    FL_UNUSED(size);
    FL_UNUSED(file);
    FL_UNUSED(line);
    fprintf(stderr,
            "Memory service is uninitialized - no aligned_alloc function provided\n");
    fflush(stderr);
    abort();
}

static FL_CALLOC_FN(default_calloc) {
    FL_UNUSED(nmemb);
    FL_UNUSED(size);
    FL_UNUSED(file);
    FL_UNUSED(line);
    fprintf(stderr, "Memory service is uninitialized - no calloc function provided\n");
    fflush(stderr);
    abort();
}

static FL_FREE_FN(default_free) {
    FL_UNUSED(ptr);
    FL_UNUSED(file);
    FL_UNUSED(line);
    fprintf(stderr, "Memory service is uninitialized - no free function provided\n");
    fflush(stderr);
    abort();
}

static FL_MALLOC_FN(default_malloc) {
    FL_UNUSED(size);
    FL_UNUSED(file);
    FL_UNUSED(line);
    fprintf(stderr, "Memory service is uninitialized - no malloc function provided\n");
    fflush(stderr);
    abort();
}

static FL_REALLOC_FN(default_realloc) {
    FL_UNUSED(ptr);
    FL_UNUSED(size);
    FL_UNUSED(file);
    FL_UNUSED(line);
    fprintf(stderr, "Memory service is uninitialized - no realloc function provided\n");
    fflush(stderr);
    abort();
}

FLMemoryService g_fla_memory_service = {
    .ctx              = NULL,
    .fl_aligned_alloc = default_aligned_alloc,
    .fl_calloc        = default_calloc,
    .fl_free          = default_free,
    .fl_malloc        = default_malloc,
    .fl_realloc       = default_realloc,
};

FL_DECL_SPEC FLA_SET_MEMORY_SERVICE_FN(fla_set_memory_service) {
    if (size < sizeof(FLMemoryService)) {
        fprintf(stderr, "invalid memory service - expected %zu bytes, received %zu\n",
                sizeof(FLMemoryService), size);
        fflush(stderr);
        abort();
    }
    g_fla_memory_service = *svc;
}
