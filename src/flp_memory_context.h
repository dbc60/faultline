#ifndef FLP_MEMORY_CONTEXT_H_
#define FLP_MEMORY_CONTEXT_H_

/**
 * @file flp_memory_context.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-03-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/arena.h>            // Arena
#include "fault_injector_internal.h" // FaultInjector

struct FLMemoryContext {
    Arena         *arena;
    FaultInjector *fi;
};

#define FLP_INIT_MEMORY_CONTEXT_FN(name) \
    void name(FLMemoryContext *ctx, Arena *arena, FaultInjector *fi)
typedef FLP_INIT_MEMORY_CONTEXT_FN(flp_init_memory_context_fn);
FLP_INIT_MEMORY_CONTEXT_FN(flp_init_memory_context);

#endif // FLP_MEMORY_CONTEXT_H_
