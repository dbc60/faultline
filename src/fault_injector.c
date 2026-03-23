/**
 * @file fault_injector.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2022-02-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/set.h>                  // for destroy_set, init_set_...
#include <faultline/fault.h>                // for fault_buffer_allocate_...
#include <faultline/fault_injector.h>       // for fault_injector_init
#include <faultline/fault_site.h>           // for fault_site_buffer_clear
#include <faultline/fl_result_codes.h>      // for FLResultCode
#include <faultline/fl_exception_service.h> // for fl_internal_error, fl_...
#include <faultline/fl_exception_types.h>   // for FLExceptionReason
#include <faultline/fl_log.h>               // for LOG_VERBOSE, LOG_DEBUG
#include <faultline/arena.h>                // for Arena
#include <stdbool.h>                        // for false, true, bool
#include <stddef.h>                         // for NULL
#include "fault_injector_internal.h"        // for FaultInjector, fault_i...
#include "fault_injector_resource.h"        // for injector_resource_set_...
#include <faultline/fl_test_summary.h>      // for FLTestSummary
#include <faultline/fl_abbreviated_types.h> // for i64
#include <faultline/fl_try.h> // for FL_THROW (selects flp or fla based on FL_BUILD_DRIVER)

FLExceptionReason fault_injector_exception = "fault injected";

void fault_injector_init(FaultInjector *injector, Arena *arena) {
    injector->fault_index         = FAULT_INJECTOR_INITIAL_INDEX;
    injector->injection_threshold = FAULT_INJECTOR_INITIAL_THRESHOLD;
    init_set_custom((Set *)&injector->allocated_resources, arena,
                    FAULT_INJECTOR_INITIAL_CAPACITY, sizeof(InjectorResource),
                    injector_resource_hash, injector_resource_compare);
    init_set_custom((Set *)&injector->released_resources, arena,
                    FAULT_INJECTOR_INITIAL_CAPACITY, sizeof(InjectorResource),
                    injector_resource_hash, injector_resource_compare);
    init_fault_buffer(&injector->faults, arena, 0);
    init_fault_site_buffer(&injector->fault_sites, arena, 0);

    injector->arena          = arena;
    injector->enabled        = false;
    injector->discovery_mode = false;
    injector->initialized    = true;
}

void fault_injector_uninit(FaultInjector *injector) {
    destroy_set(&injector->allocated_resources);
    destroy_set(&injector->released_resources);
    destroy_fault_buffer(&injector->faults);
    destroy_fault_site_buffer(&injector->fault_sites);
    injector->arena          = NULL;
    injector->enabled        = false;
    injector->discovery_mode = false;
    injector->initialized    = false;
}

void fault_injector_record_free(FaultInjector *injector, void volatile *resource,
                                char const *file, int line) {
    LOG_DEBUG("FAULT INJECTOR RECORD FREE", "release - 0x%p, %s, %d", resource, file,
              line);
    InjectorResource value;
    LOG_VERBOSE("FAULT INJECTOR RECORD FREE", "looking for 0x%p, %s, %d", resource, file,
                line);
    injector_resource_init(&value, resource, file, line);

    // Guard: resource must be in the allocated set
    if (!injector_resource_set_contains(&injector->allocated_resources, &value)) {
        FLResultCode code;
        char const  *details;
        if (injector_resource_set_contains(&injector->released_resources, &value)) {
            code    = FL_DOUBLE_FREE;
            details = "double free";
        } else {
            code    = FL_INVALID_FREE;
            details = "invalid free";
        }
        Fault *fault = fault_buffer_allocate_next_free_slot(&injector->faults);
        if (fault != NULL) {
            init_fault(fault, injector->fault_index, code, resource, fl_invalid_address,
                       details, file, line);
        } else {
            LOG_ERROR("FAULT INJECTOR RECORD FREE",
                      "unable to allocate slot from fault buffer");
        }
        return;
    }

    LOG_DEBUG("FAULT INJECTOR RECORD FREE", "allocated contains resource 0x%p",
              resource);

    // Guard: resource must not already be in the released set (internal inconsistency)
    if (injector_resource_set_contains(&injector->released_resources, &value)) {
        Fault *fault = fault_buffer_allocate_next_free_slot(&injector->faults);
        if (fault != NULL) {
            init_fault(fault, injector->fault_index, FL_BAD_STATE, resource,
                       fl_internal_error, "resource both allocated and released", file,
                       line);
        } else {
            LOG_ERROR("FAULT INJECTOR RECORD FREE",
                      "unable to allocate slot from fault buffer");
        }
        return;
    }

    // Happy path: move the resource from the allocated set to the released set
    LOG_VERBOSE("FAULT INJECTOR RECORD FREE", "0x%p, allocated before: %zd", resource,
                fault_injector_get_allocated_count(injector));
    LOG_VERBOSE("FAULT INJECTOR RECORD FREE", "0x%p, released before:  %zd", resource,
                fault_injector_get_released_count(injector));
    injector_resource_set_remove(&injector->allocated_resources, &value);
    injector_resource_set_insert(&injector->released_resources, &value);
    LOG_VERBOSE("FAULT INJECTOR RECORD FREE", "0x%p allocated after: %zd", resource,
                fault_injector_get_allocated_count(injector));
    LOG_VERBOSE("FAULT INJECTOR RECORD FREE", "0x%p release after:   %zd", resource,
                fault_injector_get_released_count(injector));
}

void fault_injector_put_invalid_free(FaultInjector *injector, void volatile *resource,
                                     FLExceptionReason reason, char const *file,
                                     int line) {
    bool reenable = false;

    // if necessary, disable the fault injector before recording the fault
    if (fault_injector_is_enabled(injector)) {
        LOG_DEBUG("FAULT INJECTOR", "temporarily disabling fault injector");
        fault_injector_disable(injector);
        reenable = true;
    }

    LOG_DEBUG("FAULT INJECTOR", "recording invalid free");
    Fault *fault = fault_buffer_allocate_next_free_slot(&injector->faults);
    if (fault != NULL) {
        init_fault(fault, injector->fault_index, FL_INVALID_FREE, resource, reason,
                   "invalid free", file, line);
    } else {
        LOG_ERROR("FAULT INJECTOR", "unable to allocate slot from fault buffer");
    }

    if (reenable) {
        LOG_DEBUG("FAULT INJECTOR", "re-enabling fault injector");
        // If we disabled faults, now we enabled them again
        fault_injector_enable(injector);
    }
}

void fault_injector_reset(FaultInjector *injector) {
    injector->fault_index         = FAULT_INJECTOR_INITIAL_INDEX;
    injector->injection_threshold = FAULT_INJECTOR_INITIAL_THRESHOLD;
    injector_resource_set_clear(&injector->allocated_resources);
    injector_resource_set_clear(&injector->released_resources);
    fault_buffer_clear(&injector->faults);
    fault_site_buffer_clear(&injector->fault_sites);
    injector->enabled        = false;
    injector->discovery_mode = false;
    injector->initialized    = true;
}

void fault_injector_try_throw_with_site(FaultInjector *injector, char const *file,
                                        int line) {
    ++injector->fault_index;

    if (injector->discovery_mode) {
        // During discovery, record this site but don't throw
        FaultSite *site
            = fault_site_buffer_allocate_next_free_slot(&injector->fault_sites);
        fault_site_init(site, injector->fault_index, file, line);
        return;
    }

    if (injector->enabled && injector->fault_index >= injector->injection_threshold) {
        FL_THROW(fault_injector_exception);
    }
}

void fault_injector_record_allocate(FaultInjector *injector, void volatile *resource,
                                    char const *file, int line) {
    InjectorResource value;
    LOG_VERBOSE("FAULT INJECTOR RECORD ALLOCATION", "recording 0x%p, %s, %d", resource,
                file, line);
    injector_resource_init(&value, resource, file, line);
    LOG_DEBUG("FAULT INJECTOR RECORD ALLOCATION", "insert InjectorResourc 0x%p", &value);
    LOG_VERBOSE("FAULT INJECTOR RECORD ALLOCATION", "0x%p, allocated before: %zd",
                resource, fault_injector_get_allocated_count(injector));
    LOG_VERBOSE("FAULT INJECTOR RECORD ALLOCATION", "0x%p, released before:  %zd",
                resource, fault_injector_get_released_count(injector));
    injector_resource_set_insert(&injector->allocated_resources, &value);
    injector_resource_set_remove(&injector->released_resources, &value);
    LOG_VERBOSE("FAULT INJECTOR RECORD ALLOCATION", "0x%p, allocated after: %zd",
                resource, fault_injector_get_allocated_count(injector));
    LOG_VERBOSE("FAULT INJECTOR RECORD ALLOCATION", "0x%p, released after:  %zd",
                resource, fault_injector_get_released_count(injector));
}

void fault_injector_record_leak_details(FaultInjector *injector, FLTestSummary *summary,
                                        i64 fault_index) {
    if (!fault_injector_has_allocated_resources(injector)) {
        return;
    }
    LeakRecordCtx ctx = {.summary = summary, .fault_index = fault_index};
    set_foreach((Set const *)&injector->allocated_resources, record_one_leak, &ctx);
}

void fault_injector_enable_discovery(FaultInjector *injector) {
    injector->discovery_mode = true;
    injector->enabled        = false; // disable fault injection during discovery
    injector->fault_index    = FAULT_INJECTOR_INITIAL_INDEX;
    fault_site_buffer_clear(&injector->fault_sites);
    fault_buffer_clear(&injector->faults);
}
