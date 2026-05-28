#ifndef FLP_MEMORY_SERVICE_H_
#define FLP_MEMORY_SERVICE_H_

/**
 * @file flp_memory_service.h
 * @author Douglas Cuthbertson
 * @brief Memory management service provided by the test driver.
 * @version 0.2
 * @date 2026-02-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_memory_service.h> // FLMemoryContext, fla_set_memory_service_fn,
                                         // FL_*_FN macros

#if defined(__cplusplus)
extern "C" {
#endif

// ── Arena-only service ────────────────────────────────────────────────────────

#define FLP_INIT_MEMORY_SERVICE_FN(name) \
    void name(fla_set_memory_service_fn *fla_set, FLMemoryContext *ctx)
typedef FLP_INIT_MEMORY_SERVICE_FN(flp_init_memory_service_fn);
FLP_INIT_MEMORY_SERVICE_FN(flp_init_memory_service);

FL_ALIGNED_ALLOC_FN(flp_aligned_alloc);
FL_CALLOC_FN(flp_calloc);
FL_FREE_FN(flp_free);
FL_FREE_POINTER_FN(flp_free_pointer);
FL_MALLOC_FN(flp_malloc);
FL_REALLOC_FN(flp_realloc);

// ── Fault-injecting service ───────────────────────────────────────────────────

typedef struct FLFaultMemoryContext FLFaultMemoryContext;

#define FLP_INIT_FAULT_MEMORY_SERVICE_FN(name) \
    void name(fla_set_memory_service_fn *fla_set, FLFaultMemoryContext *ctx)
typedef FLP_INIT_FAULT_MEMORY_SERVICE_FN(flp_init_fault_memory_service_fn);
FLP_INIT_FAULT_MEMORY_SERVICE_FN(flp_init_fault_memory_service);

FL_ALIGNED_ALLOC_FN(flp_fault_aligned_alloc);
FL_CALLOC_FN(flp_fault_calloc);
FL_FREE_FN(flp_fault_free);
FL_FREE_POINTER_FN(flp_fault_free_pointer);
FL_MALLOC_FN(flp_fault_malloc);
FL_REALLOC_FN(flp_fault_realloc);

#if defined(__cplusplus)
}
#endif

#endif // FLP_MEMORY_SERVICE_H_
