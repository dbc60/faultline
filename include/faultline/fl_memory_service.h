#ifndef FL_MEMORY_SERVICE_H_
#define FL_MEMORY_SERVICE_H_

/**
 * @file fl_memory_service.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.2
 * @date 2026-02-23
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_macros.h> // FL_STR
#include <stddef.h>              // size_t

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct FLMemoryContext FLMemoryContext;

#define FL_ALIGNED_ALLOC_FN(name) \
    void *name(size_t alignment, size_t size, char const *file, int line)
typedef FL_ALIGNED_ALLOC_FN(fl_aligned_alloc_fn);

#define FL_CALLOC_FN(name) \
    void *name(size_t nmemb, size_t size, char const *file, int line)
typedef FL_CALLOC_FN(fl_calloc_fn);

#define FL_FREE_FN(name) void name(void *ptr, char const *file, int line)
typedef FL_FREE_FN(fl_free_fn);

#define FL_FREE_POINTER_FN(name) void name(void **ptr, char const *file, int line)
typedef FL_FREE_POINTER_FN(fl_free_pointer_fn);

#define FL_MALLOC_FN(name) void *name(size_t size, char const *file, int line)
typedef FL_MALLOC_FN(fl_malloc_fn);

#define FL_REALLOC_FN(name) \
    void *name(void *ptr, size_t size, char const *file, int line)
typedef FL_REALLOC_FN(fl_realloc_fn);

typedef struct FLMemoryService {
    FLMemoryContext     *ctx;
    fl_aligned_alloc_fn *fl_aligned_alloc;
    fl_calloc_fn        *fl_calloc;
    fl_free_fn          *fl_free;
    fl_malloc_fn        *fl_malloc;
    fl_realloc_fn       *fl_realloc;
} FLMemoryService;

// These definitions are common to both the platform and application implementations
#define FLA_SET_MEMORY_SERVICE_FN(name) \
    void name(FLMemoryService *const svc, size_t size)
typedef FLA_SET_MEMORY_SERVICE_FN(fla_set_memory_service_fn);
#define FLA_SET_MEMORY_SERVICE_STR FL_STR(fla_set_memory_service)

#if defined(__cplusplus)
}
#endif

#endif // FL_MEMORY_SERVICE_H_
