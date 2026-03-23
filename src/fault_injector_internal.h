#ifndef FAULT_INJECTOR_INTERNAL_H_
#define FAULT_INJECTOR_INTERNAL_H_

/**
 * @file fault_injector_internal.h
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
#include <faultline/arena.h>
#include "fault_injector_resource.h" // InjectorResourceSet

#include <faultline/set.h>             // Set implementation (needs full definition)
#include <faultline/fl_log.h>          // LOG_* macros
#include <faultline/buffer.h>          // Buffer API
#include <faultline/fault.h>           // Fault
#include <faultline/fault_site.h>      // FaultSite, FaultSiteBuffer
#include <faultline/fl_test_summary.h> // FLTestSummary

#include <faultline/fl_try.h>               // FL_THROW
#include <faultline/fl_exception_types.h>   // FLExceptionReason
#include <assert.h>                         // assert
#include <faultline/fl_abbreviated_types.h> // i64

#include <stdbool.h> // bool
#include <stddef.h>  // size_t

#if defined(__cplusplus)
extern "C" {
#endif

#define FAULT_INJECTOR_INITIAL_INDEX 0 ///< the initial value of fault_index
#define FAULT_INJECTOR_INITIAL_THRESHOLD \
    1 ///< the initial value of the injection_threshold
#define FAULT_INJECTOR_INITIAL_CAPACITY \
    16 ///< the initial capacity of the allocated and released resources sets

extern FLExceptionReason fault_injector_exception;

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
typedef struct FaultInjector {
    Arena *arena; ///< the arena to use for memory allocations
    /**
     * fault_index is the number of times a tracked resource has been allocated
     * in the code under test. When it equals injection_point and enabled is true,
     * the test harness will throw fault_injector_exception to fail the allocation.
     */
    i64 fault_index;

    /**
     * injection_threshold is the value of fault_index at which the next allocation will
     * fail.
     */
    i64 injection_threshold;

    InjectorResourceSet
        allocated_resources; ///< a set of addresses acquired from an allocator and not
                             ///< yet released back to it.

    InjectorResourceSet
        released_resources; ///< a set of addresses released from an allocator.

    FaultBuffer faults; ///< a collection of allocation errors.

    FaultSiteBuffer fault_sites; ///< collection of discovered fault injection sites

    /**
     * enabled is false if exceptions should not be thrown, otherwise throw when
     * fault_index equals injection_threshold.
     */
    bool enabled;

    /**
     * discovery_mode is true during the initial non-injection pass to discover and
     * catalog fault sites. When true, no faults are injected but each allocation adds an
     * entry to fault_sites. If any are recorded, they will be used to inject faults
     * during a set of fault-injection passes.
     */
    bool discovery_mode;

    bool initialized; ///< used for initialization
} FaultInjector;

/**
 * @brief free collision chain entries; bucket arrays and buffer backing stores remain
 * allocated.
 *
 * @param injector
 */
void fault_injector_reset(FaultInjector *injector);

static inline bool fault_injector_is_initialized(FaultInjector *injector) {
    return injector->initialized;
}

/**
 * @brief If faults are enabled and the current value of fault_index is equal to
 * injection_threshold, then throw fault_injector_exception.
 *
 * @param injector the address of a FaultInjector
 * @throw fault_injector_exception
 */
static inline void fault_injector_try_throw(FaultInjector *injector) {
    if (injector->enabled && ++injector->fault_index >= injector->injection_threshold) {
        FL_THROW(fault_injector_exception);
    }
}

/**
 * @brief If faults are enabled and the current value of fault_index is equal to
 * injection_threshold, then throw fault_injector_exception. In discovery mode, no
 * exceptions are thrown but allocation sites are recorded.
 *
 * @param injector the address of a FaultInjector
 * @param type the type of allocation operation
 * @param file the source file where allocation occurs
 * @param line the line number where allocation occurs
 * @param size the size of the allocation (0 for free operations)
 * @throw fault_injector_exception
 */
void fault_injector_try_throw_with_site(FaultInjector *injector, char const *file,
                                        int line);

/**
 * @brief Set injector to throw its exception on the given threshold, reset fault_index
 * to 0, and clear the invalid addresses buffer.
 *
 * @param injector the address of a FaultInjector
 * @param threshold the number of allocations to inject a fault.
 */
static inline void fault_injector_set_threshold(FaultInjector *injector, i64 threshold) {
    injector->fault_index         = FAULT_INJECTOR_INITIAL_INDEX;
    injector->injection_threshold = threshold;
    fault_buffer_clear(&injector->faults);
    injector_resource_set_clear(&injector->allocated_resources);
    injector_resource_set_clear(&injector->released_resources);
}

/**
 * @brief Increment the threshold by one, reset the fault_index, clear the faults
 * buffer, and clear the resource tracking sets so each injection pass starts with a
 * clean slate.
 *
 * @param injector the address of a FaultInjector
 */
static inline void fault_injector_advance_threshold(FaultInjector *injector) {
    injector->fault_index = FAULT_INJECTOR_INITIAL_INDEX;
    injector->injection_threshold++;
    fault_buffer_clear(&injector->faults);
    injector_resource_set_clear(&injector->allocated_resources);
    injector_resource_set_clear(&injector->released_resources);
}

/**
 * @brief return the value that will inject a fault.
 *
 * @param injector the address of a FaultInjector
 * @return i64 the number of allocations that will inject a fault.
 */
static inline i64 fault_injector_get_threshold(FaultInjector *injector) {
    return injector->injection_threshold;
}

/**
 * @brief enable the injector
 *
 * @param injector
 */
static inline void fault_injector_enable(FaultInjector *injector) {
    injector->enabled = true;
}

/**
 * @brief disable the fault injector
 *
 * @param injector
 */
static inline void fault_injector_disable(FaultInjector *injector) {
    injector->enabled = false;
}

/**
 * @brief return whether or not the FaultInjector is enabled.
 *
 * @param injector
 * @return true
 * @return false
 */
static inline bool fault_injector_is_enabled(FaultInjector *injector) {
    return injector->enabled;
}

/**
 * @brief return true if a fault_injector_exception has been thrown, and false otherwise.
 *
 * When a fault_injector_exception is thrown, the test harness should catch it and cause
 * resource acquisition to fail (e.g., cause malloc to return NULL).
 *
 * @param injector the address of a FaultInjector
 * @return true if the exception was thrown.
 * @return false if the exception was not thrown.
 */
static inline bool fault_injector_triggered(FaultInjector *injector) {
    return injector->fault_index >= injector->injection_threshold;
}

/**
 * @brief record that a resource was freed (e.g., when memory is freed).
 *
 * @param injector the address of a FaultInjector
 * @param resource the address of the resource (memory) that was freed/released.
 * @param file the file name in which the resource was released
 * @param line the line number in the file in which the resource was released
 */
void fault_injector_record_free(FaultInjector *injector, void volatile *resource,
                                char const *file, int line);

// An attempt was made to pass an invalid address to a free/release function.
void fault_injector_put_invalid_free(FaultInjector *injector, void volatile *resource,
                                     FLExceptionReason reason, char const *file,
                                     int line);

/**
 * @brief return the number of allocated resources.
 *
 * @param injector the address of a FaultInjector
 * @return a counting of resource acquisitions
 */
static inline i64 fault_injector_get_allocated_count(FaultInjector *injector) {
    return (i64)set_size(&injector->allocated_resources);
}

static inline i64 fault_injector_get_released_count(FaultInjector *injector) {
    return (i64)set_size(&injector->released_resources);
}

static inline bool fault_injector_has_allocated_resources(FaultInjector *injector) {
    return !set_is_empty(&injector->allocated_resources);
}

/**
 * @brief record that a resource was allocated (e.g., when memory is allocated).
 *
 * @param injector the address of a FaultInjector
 * @param resource the address of allocated memory or other resource.
 */
void fault_injector_record_allocate(FaultInjector *injector, void volatile *resource,
                                    char const *file, int line);

/**
 * @brief Return the number of faults tested
 *
 * @param injector the address of a FaultInjector
 * @return a count of tested faults
 */
static inline i64 fault_injector_get_allocation_index(FaultInjector *injector) {
    return injector->fault_index;
}

/**
 * @brief the number of invalid resources passed to fault_release
 *
 * @param injector the address of a FaultInjector
 * @return i64
 */
static inline i64 fault_injector_get_invalid_address_count(FaultInjector *injector) {
    return fault_buffer_count(&injector->faults);
}

/**
 * @brief Return the fault at the given index, or NULL if index is out of range.
 *
 * @param injector the address of a FaultInjector
 * @param index 0-based index into the faults buffer
 * @return pointer to the Fault, or NULL if index >= fault count
 */
static inline Fault *fault_injector_get_fault(FaultInjector *injector, size_t index) {
    if (index >= (size_t)fault_injector_get_invalid_address_count(injector)) {
        return NULL;
    }
    return fault_buffer_get(&injector->faults, index);
}

/**
 * @brief Enable discovery mode to catalog fault sites without injecting faults
 *
 * @param injector the address of a FaultInjector
 */
void fault_injector_enable_discovery(FaultInjector *injector);

/**
 * @brief Disable discovery mode and return to normal fault injection mode
 *
 * @param injector the address of a FaultInjector
 */
static inline void fault_injector_disable_discovery(FaultInjector *injector) {
    injector->discovery_mode = false;
}

/**
 * @brief Check if the injector is in discovery mode
 *
 * @param injector the address of a FaultInjector
 * @return true if in discovery mode, false otherwise
 */
static inline bool fault_injector_is_discovery_mode(FaultInjector *injector) {
    return injector->discovery_mode;
}

/**
 * @brief Get the number of discovered fault sites
 *
 * @param injector the address of a FaultInjector
 * @return number of fault sites discovered
 */
static inline i64 fault_injector_get_site_count(FaultInjector *injector) {
    return (i64)fault_site_buffer_count(&injector->fault_sites);
}

/**
 * @brief Get a fault site by index
 *
 * @param injector the address of a FaultInjector
 * @param index the index of the site to retrieve (0-based)
 * @return pointer to the FaultSite, or NULL if index is out of range
 */
static inline FaultSite *fault_injector_get_site(FaultInjector *injector, size_t index) {
    if (index >= fault_site_buffer_count(&injector->fault_sites)) {
        return NULL;
    }
    return fault_site_buffer_get(&injector->fault_sites, index);
}

/**
 * @brief Clear all discovered fault sites
 *
 * @param injector the address of a FaultInjector
 */
static inline void fault_injector_clear_sites(FaultInjector *injector) {
    fault_site_buffer_clear(&injector->fault_sites);
}

typedef struct LeakRecordCtx {
    FLTestSummary *summary;
    i64            fault_index;
} LeakRecordCtx;

static inline void record_one_leak(void const *value, void *ctx_) {
    InjectorResource const *ir  = value;
    LeakRecordCtx          *ctx = ctx_;
    faultline_test_summary_add_fault(ctx->summary, ctx->fault_index, FL_LEAK, ir->value,
                                     "resource leak detected", "memory not freed",
                                     ir->file, ir->line);
}

/**
 * @brief Record individual leaked resources with their allocation sites
 *
 * This function iterates through all allocated resources in the injector and
 * records each one as a separate leak fault with its original allocation file/line.
 *
 * @param injector the address of a FaultInjector containing leaked resources
 * @param summary the test summary to record leak faults in
 * @param fault_index the current fault injection index
 */
void fault_injector_record_leak_details(FaultInjector *injector, FLTestSummary *summary,
                                        i64 fault_index);

#if defined(__cplusplus)
}
#endif

#endif // FAULT_INJECTOR_INTERNAL_H_
