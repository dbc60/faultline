#ifndef FLP_FAULT_MEMORY_CONTEXT_H_
#define FLP_FAULT_MEMORY_CONTEXT_H_

/**
 * @file flp_fault_memory_context.h
 * @author Douglas Cuthbertson
 * @brief Fault-injecting memory context for the platform memory service.
 * @version 0.1
 * @date 2026-05-28
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/arena.h>          // Arena
#include <faultline/fault_injector.h> // FaultInjector (opaque)

typedef struct FLFaultMemoryContext {
    Arena         *arena;
    FaultInjector *fi;
} FLFaultMemoryContext;

#define FLP_INIT_FAULT_MEMORY_CONTEXT_FN(name) \
    void name(FLFaultMemoryContext *ctx, Arena *arena, FaultInjector *fi)
typedef FLP_INIT_FAULT_MEMORY_CONTEXT_FN(flp_init_fault_memory_context_fn);
FLP_INIT_FAULT_MEMORY_CONTEXT_FN(flp_init_fault_memory_context);

#endif // FLP_FAULT_MEMORY_CONTEXT_H_
