#ifndef FAULT_INJECTOR_H_
#define FAULT_INJECTOR_H_

/**
 * @file fault_injector.h
 * @author Douglas Cuthbertson
 * @brief Definition of FaultInjector.
 * @version 0.2.0
 * @date 2022-02-11
 *
 * FaultInjector tracks the acquisition and release of resources, sequentially failing
 * allocations.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/arena.h> // for Arena

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief FaultInjector tracks the number of resources acquired and released in the code
 * under test.
 *
 * A FaultInjector has code that calls functions instrumented to watch for resource
 * allocations and releases, such as memory allocators. It assumes that each allocation
 * should be balanced by a release of the resource.
 *
 * Properly instrumented code can report whether all allocated resources were released,
 * or an attempt was made to release already released resources.
 */
typedef struct FaultInjector FaultInjector;

/**
 * @brief Initialize a FaultInjector.
 *
 * If a new injector is being initialized, use the arena to allocate its internal
 * structures.
 *
 * If the injector is already initialized, and the same arena is used, then reset it so
 * all internal structures are set to their initial states and ready for use. If a
 * different arena is used on an already initialized injector, then release all allocated
 * memory to the old arena and reinitialize the injector using the new arena.
 *
 * @param injector the address of a FaultInjector to be initialized.
 */
void fault_injector_init(FaultInjector *injector, Arena *arena);

/**
 * @brief Uninitialize the fault injector, releasing all memory from its internal
 * structures.
 *
 * @param injector
 */
void fault_injector_uninit(FaultInjector *injector);

#if defined(__cplusplus)
}
#endif

#endif // FAULT_INJECTOR_H_
