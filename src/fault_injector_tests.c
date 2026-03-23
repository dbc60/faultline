/**
 * @file fault_injector_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the fault injector->
 * @version 0.1
 * @date 2026-02-21
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "arena.c"
#include "arena_dbg.c"
#include "arena_malloc.c"
#include "buffer.c"
#include "digital_search_tree.c"
#include "fl_exception_service.c"  // fl_test_exception and other exception reasons
#include "fla_exception_service.c" // app-side TLS exception service
#include "fla_log_service.c"       // app-side TLS log service
#include "fla_memory_service.c"
#include "../third_party/fnv/FNV64.c"
#include "region.c"
#include "region_node.c"
#include "set.c"
#include "fault_injector.c"

#include <faultline/fl_test.h> // FL_TYPE_TEST_SETUP_CLEANUP, FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h> // FL_TRY, FL_CATCH, FL_THROW, FL_THROW_DETAILS, FL_END_TRY

#include <faultline/fl_macros.h> // FL_CONTAINER_OF

#include <stdbool.h> // bool
#include <stddef.h>  // NULL
#include <string.h>  // memset

/**
 * @brief a test case to pass a FaultInjector to setup, test, and cleanup functions.
 */
typedef struct FaultTestCase {
    FLTestCase    tc;
    Arena        *arena;
    FaultInjector injector;
} FaultTestCase;

static FL_SETUP_FN(setup_config) {
    FaultTestCase *ftc = FL_CONTAINER_OF(tc, FaultTestCase, tc);
    ftc->arena         = new_arena(0, 0);
    fault_injector_init(&ftc->injector, ftc->arena);
}

static FL_CLEANUP_FN(cleanup_config) {
    FaultTestCase *ftc = FL_CONTAINER_OF(tc, FaultTestCase, tc);
    fault_injector_reset(&ftc->injector);
    release_arena(&ftc->arena);
}

// Test FaultInjector enable/disable functionality
FL_TYPE_TEST_SETUP_CLEANUP("Enable/Disable", FaultTestCase, test_fault_enable_disable,
                           setup_config, cleanup_config) {
    FaultInjector *injector = &t->injector;

    fault_injector_enable(injector);
    if (!fault_injector_is_enabled(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected enabled == true after fault_injector_enable");
    }

    fault_injector_disable(injector);
    if (fault_injector_is_enabled(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected enabled == false after fault_injector_disable");
    }
}

/**
 * @brief verify initialization succeeded
 */
FL_TYPE_TEST_SETUP_CLEANUP("Default Values", FaultTestCase, default_values, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;

    if (!fault_injector_is_initialized(injector)) {
        FL_THROW_DETAILS(fl_test_exception, "expected initialized == true");
    }
    if (fault_injector_is_enabled(injector)) {
        FL_THROW_DETAILS(fl_test_exception, "expected enabled == false");
    }
    if (fault_injector_get_allocation_index(injector) != FAULT_INJECTOR_INITIAL_INDEX) {
        FL_THROW_DETAILS(fl_test_exception, "expected fault_index == %d, got %lld",
                         FAULT_INJECTOR_INITIAL_INDEX,
                         fault_injector_get_allocation_index(injector));
    }
    if (fault_injector_get_threshold(injector) != FAULT_INJECTOR_INITIAL_THRESHOLD) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected injection_threshold == %d, got %lld",
                         FAULT_INJECTOR_INITIAL_THRESHOLD,
                         (long long)fault_injector_get_threshold(injector));
    }
    if (fault_injector_get_allocated_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected allocated_resources to be empty, got %zu",
                         fault_injector_get_allocated_count(injector));
    }
    if (fault_injector_get_released_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected released_resources to be empty, got %zu",
                         fault_injector_get_released_count(injector));
    }
    if (fault_injector_get_invalid_address_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception, "expected faults to be empty, got %lld",
                         (long long)fault_injector_get_invalid_address_count(injector));
    }
    if (fault_injector_get_site_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception, "expected fault_sites to be empty, got %lld",
                         (long long)fault_injector_get_site_count(injector));
    }
}

/**
 * @brief verify the FaultInjector triggers at the expected threshold.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Threshold Test", FaultTestCase, threshold, setup_config,
                           cleanup_config) {
    FaultInjector *injector  = &t->injector;
    int const      threshold = 3, max_count = 10;
    // volatile: C11 §7.13.2.1 ¶3 - automatic variables modified between setjmp and
    // longjmp have indeterminate value after longjmp unless volatile-qualified. Without
    // volatile, LLVM promotes i to a callee-saved register which longjmp restores to the
    // setjmp-time value, making i indeterminate (UB) in the FL_CATCH block. The
    // optimizer then inserts unreachable after FL_HANDLED, eliminating the catch body
    // entirely.
    int volatile i;

    FL_TRY {
        fault_injector_enable(injector);
        fault_injector_set_threshold(injector, threshold);
        for (i = 1; i < max_count; i++) {
            fault_injector_try_throw(injector);
        }
        FL_THROW(fl_test_exception);
    }
    FL_CATCH(fault_injector_exception) {
        if (i != threshold) {
            FL_THROW_DETAILS(fl_test_exception,
                             "injector exception caught, but throw-count is incorrect. "
                             "Expected %d. Actual %d",
                             threshold, i);
        }
    }
    FL_END_TRY;

    if (!fault_injector_triggered(injector)) {
        FL_THROW_DETAILS(fl_test_exception, "Expected injector to be triggered");
    }
}

/**
 * @brief verify disabling a FaultInjector prevents it from triggering.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Disable", FaultTestCase, disable_fault, setup_config,
                           cleanup_config) {
    FaultInjector *injector  = &t->injector;
    int const      threshold = 3, max_count = 10;
    int volatile i; // C11 §7.13.2.1 ¶3 - modified in FL_TRY, read in FL_CATCH

    FL_TRY {
        fault_injector_disable(injector);
        fault_injector_set_threshold(injector, threshold);
        for (i = 1; i < max_count; i++) {
            fault_injector_try_throw(injector);
        }
    }
    FL_CATCH(fault_injector_exception) {
        FL_THROW_DETAILS(fl_test_exception,
                         "caught unexpected fault_injector_exception, throw count %d, "
                         "iteration %d, has thrown(%d)",
                         threshold, i, fault_injector_triggered(injector));
    }
    FL_END_TRY;

    if (fault_injector_triggered(injector)) {
        FL_THROW(fl_test_exception);
    }
}

/**
 * @brief verify acquire adds a resource to the allocated set.
 *
 * Note that &dummy is a stack address used as the resource handle so it's a valid
 * pointer the set can hash, but it is never dereferenced by the injector->
 */
FL_TYPE_TEST_SETUP_CLEANUP("Acquire", FaultTestCase, acquire_resource, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    fault_injector_record_allocate(injector, resource, __FILE__, __LINE__);

    if (fault_injector_get_allocated_count(injector) != 1) {
        FL_THROW_DETAILS(fl_test_exception, "expected allocated_count == 1, got %lld",
                         (long long)fault_injector_get_allocated_count(injector));
    }
    if (fault_injector_get_released_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception, "expected released_count == 0, got %lld",
                         (long long)fault_injector_get_released_count(injector));
    }
    if (fault_injector_get_invalid_address_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception, "expected no faults, got %lld",
                         (long long)fault_injector_get_invalid_address_count(injector));
    }
    if (!fault_injector_has_allocated_resources(injector)) {
        FL_THROW_DETAILS(fl_test_exception, "expected has_allocated_resources == true");
    }
}

/**
 * @brief verify releasing an acquired resource moves it to the released set.
 *
 * Note that &dummy is a stack address used as the resource handle so it's a valid
 * pointer the set can hash, but it is never dereferenced by the injector->
 */
FL_TYPE_TEST_SETUP_CLEANUP("Acquire Then Release", FaultTestCase, acquire_then_release,
                           setup_config, cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    fault_injector_record_allocate(injector, resource, __FILE__, __LINE__);
    fault_injector_record_free(injector, resource, __FILE__, __LINE__);

    if (fault_injector_get_allocated_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected allocated_count == 0 after release, got %lld",
                         (long long)fault_injector_get_allocated_count(injector));
    }
    if (fault_injector_get_released_count(injector) != 1) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected released_count == 1 after release, got %lld",
                         (long long)fault_injector_get_released_count(injector));
    }
    if (fault_injector_get_invalid_address_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected no faults after valid release, got %lld",
                         (long long)fault_injector_get_invalid_address_count(injector));
    }
    if (fault_injector_has_allocated_resources(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected has_allocated_resources == false after release");
    }
}

/**
 * @brief verify that releasing a resource twice records a DOUBLE_FREE fault.
 *
 * Note that &dummy is a stack address used as the resource handle so it's a valid
 * pointer the set can hash, but it is never dereferenced by the injector->
 */
FL_TYPE_TEST_SETUP_CLEANUP("Double Free", FaultTestCase, double_free, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    fault_injector_record_allocate(injector, resource, __FILE__, __LINE__);
    fault_injector_record_free(injector, resource, __FILE__, __LINE__);
    fault_injector_record_free(injector, resource, __FILE__, __LINE__);

    i64 fault_count = fault_injector_get_invalid_address_count(injector);
    if (fault_count != 1) {
        FL_THROW_DETAILS(fl_test_exception, "expected 1 fault for double free, got %lld",
                         (long long)fault_count);
    }
    Fault *fault = fault_injector_get_fault(injector, 0);
    if (fault->code != FL_DOUBLE_FREE) {
        FL_THROW_DETAILS(fl_test_exception, "expected FL_DOUBLE_FREE, got %s",
                         faultline_result_code_to_string(fault->code));
    }
    if (fault->resource != resource) {
        FL_THROW_DETAILS(fl_test_exception, "expected resource 0x%p in fault, got 0x%p",
                         resource, fault->resource);
    }
}

/**
 * @brief verify that releasing a never-acquired resource records an INVALID_FREE fault.
 *
 * Note that &dummy is a stack address used as the resource handle so it's a valid
 * pointer the set can hash, but it is never dereferenced by the injector->
 */
FL_TYPE_TEST_SETUP_CLEANUP("Invalid Free", FaultTestCase, invalid_free, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    fault_injector_record_free(injector, resource, __FILE__, __LINE__);

    i64 fault_count = fault_injector_get_invalid_address_count(injector);
    if (fault_count != 1) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected 1 fault for invalid free, got %lld",
                         (long long)fault_count);
    }
    Fault *fault = fault_injector_get_fault(injector, 0);
    if (fault->code != FL_INVALID_FREE) {
        FL_THROW_DETAILS(fl_test_exception, "expected FL_INVALID_FREE, got %s",
                         faultline_result_code_to_string(fault->code));
    }
    if (fault->resource != resource) {
        FL_THROW_DETAILS(fl_test_exception, "expected resource 0x%p in fault, got 0x%p",
                         resource, fault->resource);
    }
}

/**
 * @brief verify that an acquired but unreleased resource is detectable as a leak.
 *
 * Note that &dummy is a stack address used as the resource handle so it's a valid
 * pointer the set can hash, but it is never dereferenced by the injector->
 */
FL_TYPE_TEST_SETUP_CLEANUP("Leak", FaultTestCase, resource_leak, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    fault_injector_record_allocate(injector, resource, __FILE__, __LINE__);

    if (!fault_injector_has_allocated_resources(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected has_allocated_resources == true for leaked resource");
    }
    if (fault_injector_get_allocated_count(injector) != 1) {
        FL_THROW_DETAILS(fl_test_exception, "expected allocated_count == 1, got %lld",
                         (long long)fault_injector_get_allocated_count(injector));
    }
    if (fault_injector_get_released_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception, "expected released_count == 0, got %lld",
                         (long long)fault_injector_get_released_count(injector));
    }
}

/**
 * @brief verify that calling fault_injector_init on an already-initialized injector
 * resets it to its initial state rather than reinitializing (which would leak
 * resources).
 *
 * Note that &dummy is a stack address used as the resource handle so it's a valid
 * pointer the set can hash, but it is never dereferenced by the injector->
 */
FL_TYPE_TEST_SETUP_CLEANUP("Reinitialize", FaultTestCase, reinitialize, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    // Mutate the injector's state: enable it, set a non-default threshold, acquire a
    // resource
    fault_injector_enable(injector);
    fault_injector_set_threshold(injector, 5);
    fault_injector_record_allocate(injector, resource, __FILE__, __LINE__);

    // Re-initializing must reset to a clean state rather than reallocate the sets
    fault_injector_reset(injector);

    if (!fault_injector_is_initialized(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected initialized == true after re-init");
    }
    if (fault_injector_is_enabled(injector)) {
        FL_THROW_DETAILS(fl_test_exception, "expected enabled == false after re-init");
    }
    if (fault_injector_get_allocation_index(injector) != FAULT_INJECTOR_INITIAL_INDEX) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected fault_index == %d after re-init, got %lld",
                         FAULT_INJECTOR_INITIAL_INDEX,
                         fault_injector_get_allocation_index(injector));
    }
    if (fault_injector_get_threshold(injector) != FAULT_INJECTOR_INITIAL_THRESHOLD) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected injection_threshold == %d after re-init, got %lld",
                         FAULT_INJECTOR_INITIAL_THRESHOLD,
                         (long long)fault_injector_get_threshold(injector));
    }
    if (fault_injector_has_allocated_resources(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected allocated_resources empty after re-init, got %lld",
                         (long long)fault_injector_get_allocated_count(injector));
    }
}

/**
 * @brief verify that fault_injector_set_threshold resets fault_index to its initial
 * value and clears the faults buffer in addition to setting the new threshold.
 *
 * Note that &dummy is a stack address used as the resource handle so it's a valid
 * pointer the set can hash, but it is never dereferenced by the injector->
 */
FL_TYPE_TEST_SETUP_CLEANUP("Set Threshold Side Effects", FaultTestCase,
                           set_threshold_side_effects, setup_config, cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    // Set a high threshold so try_throw doesn't fire, then advance fault_index
    fault_injector_enable(injector);
    fault_injector_set_threshold(injector, 10);
    fault_injector_try_throw(injector);
    fault_injector_try_throw(injector);
    fault_injector_try_throw(injector);

    // Populate the faults buffer with an invalid free
    fault_injector_record_free(injector, resource, __FILE__, __LINE__);
    if (fault_injector_get_invalid_address_count(injector) != 1) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected 1 fault before set_threshold, got %lld",
                         (long long)fault_injector_get_invalid_address_count(injector));
    }

    // set_threshold must reset fault_index and clear faults
    fault_injector_set_threshold(injector, 5);

    if (fault_injector_get_allocation_index(injector) != FAULT_INJECTOR_INITIAL_INDEX) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected fault_index == %d after set_threshold, got %lld",
                         FAULT_INJECTOR_INITIAL_INDEX,
                         fault_injector_get_allocation_index(injector));
    }
    if (fault_injector_get_threshold(injector) != 5) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected injection_threshold == 5, got %lld",
                         (long long)fault_injector_get_threshold(injector));
    }
    if (fault_injector_get_invalid_address_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected faults cleared by set_threshold, got %lld",
                         (long long)fault_injector_get_invalid_address_count(injector));
    }
}

/**
 * @brief verify that fault_injector_advance_threshold increments the threshold,
 * resets fault_index, and that the injector triggers at the new threshold.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Advance Threshold", FaultTestCase, advance_threshold,
                           setup_config, cleanup_config) {
    FaultInjector *injector  = &t->injector;
    int const      threshold = 2;
    int volatile i; // C11 §7.13.2.1 ¶3 - modified in FL_TRY, read in FL_CATCH

    fault_injector_enable(injector);
    fault_injector_set_threshold(injector, threshold);

    // First pass: must trigger at the original threshold
    FL_TRY {
        for (i = 1; i < 10; i++) {
            fault_injector_try_throw(injector);
        }
        FL_THROW(fl_test_exception);
    }
    FL_CATCH(fault_injector_exception) {
        if (i != threshold) {
            FL_THROW_DETAILS(fl_test_exception,
                             "first pass: expected throw at %d, got %d", threshold, i);
        }
    }
    FL_END_TRY;

    // Advance: threshold becomes threshold+1 and fault_index resets to 0
    fault_injector_advance_threshold(injector);

    if (fault_injector_get_threshold(injector) != threshold + 1) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected threshold == %d after advance, got %lld",
                         threshold + 1,
                         (long long)fault_injector_get_threshold(injector));
    }
    if (fault_injector_get_allocation_index(injector) != FAULT_INJECTOR_INITIAL_INDEX) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected fault_index == %d after advance, got %lld",
                         FAULT_INJECTOR_INITIAL_INDEX,
                         fault_injector_get_allocation_index(injector));
    }

    // Second pass: must trigger one step later
    FL_TRY {
        for (i = 1; i < 10; i++) {
            fault_injector_try_throw(injector);
        }
        FL_THROW(fl_test_exception);
    }
    FL_CATCH(fault_injector_exception) {
        if (i != threshold + 1) {
            FL_THROW_DETAILS(fl_test_exception,
                             "second pass: expected throw at %d, got %d", threshold + 1,
                             i);
        }
    }
    FL_END_TRY;
}

/**
 * @brief verify that fault_injector_put_invalid_free records a fault with the supplied
 * reason and leaves the injector in the same enabled/disabled state as before the call.
 *
 * Note that &dummy is a stack address used as the resource handle so it's a valid
 * pointer the set can hash, but it is never dereferenced by the injector->
 */
FL_TYPE_TEST_SETUP_CLEANUP("Put Invalid Free", FaultTestCase, put_invalid_free,
                           setup_config, cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    fault_injector_enable(injector);
    fault_injector_put_invalid_free(injector, resource, fl_invalid_address, __FILE__,
                                    __LINE__);

    i64 fault_count = fault_injector_get_invalid_address_count(injector);
    if (fault_count != 1) {
        FL_THROW_DETAILS(fl_test_exception, "expected 1 fault, got %lld",
                         (long long)fault_count);
    }
    Fault *fault = fault_injector_get_fault(injector, 0);
    if (fault->code != FL_INVALID_FREE) {
        FL_THROW_DETAILS(fl_test_exception, "expected FL_INVALID_FREE, got %s",
                         faultline_result_code_to_string(fault->code));
    }
    if (fault->resource != resource) {
        FL_THROW_DETAILS(fl_test_exception, "expected resource 0x%p in fault, got 0x%p",
                         resource, fault->resource);
    }
    if (fault->reason != fl_invalid_address) {
        FL_THROW_DETAILS(fl_test_exception, "expected reason fl_invalid_address, got %s",
                         fault->reason);
    }
    // The injector was enabled before the call and must still be enabled after
    if (!fault_injector_is_enabled(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected injector re-enabled after put_invalid_free");
    }
}

/**
 * @brief verify discovery mode: sites are recorded instead of exceptions thrown, sites
 * can be retrieved and cleared, and disabling discovery preserves the recorded sites.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Discovery Mode", FaultTestCase, discovery_mode, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;

    fault_injector_enable_discovery(injector);

    if (!fault_injector_is_discovery_mode(injector)) {
        FL_THROW_DETAILS(fl_test_exception, "expected discovery_mode == true");
    }
    // Discovery mode disables fault injection
    if (fault_injector_is_enabled(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected injector disabled during discovery");
    }

    // try_throw_with_site must record sites without throwing
    fault_injector_try_throw_with_site(injector, __FILE__, __LINE__);
    fault_injector_try_throw_with_site(injector, __FILE__, __LINE__);
    fault_injector_try_throw_with_site(injector, __FILE__, __LINE__);

    if (fault_injector_get_site_count(injector) != 3) {
        FL_THROW_DETAILS(fl_test_exception, "expected 3 fault sites, got %lld",
                         (long long)fault_injector_get_site_count(injector));
    }

    // Site at index 0 must be accessible
    FaultSite *site = fault_injector_get_site(injector, 0);
    if (site == NULL) {
        FL_THROW_DETAILS(fl_test_exception, "expected non-NULL site at index 0");
    }

    // Out-of-bounds access must return NULL
    if (fault_injector_get_site(injector, 3) != NULL) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected NULL for out-of-bounds site index 3");
    }

    // Disabling discovery must preserve the recorded sites
    fault_injector_disable_discovery(injector);
    if (fault_injector_is_discovery_mode(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected discovery_mode == false after disable");
    }
    if (fault_injector_get_site_count(injector) != 3) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected sites preserved after disable_discovery, got %lld",
                         (long long)fault_injector_get_site_count(injector));
    }

    // Clearing sites must empty the buffer
    fault_injector_clear_sites(injector);
    if (fault_injector_get_site_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected 0 sites after clear_sites, got %lld",
                         (long long)fault_injector_get_site_count(injector));
    }
}

/**
 * @brief verify that fault_injector_try_throw_with_site throws at the threshold when
 * not in discovery mode, identical to fault_injector_try_throw.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Try Throw With Site", FaultTestCase, try_throw_with_site,
                           setup_config, cleanup_config) {
    FaultInjector *injector  = &t->injector;
    int const      threshold = 3, max_count = 10;
    int volatile i; // C11 §7.13.2.1 ¶3 - modified in FL_TRY, read in FL_CATCH

    FL_TRY {
        fault_injector_enable(injector);
        fault_injector_set_threshold(injector, threshold);
        for (i = 1; i < max_count; i++) {
            fault_injector_try_throw_with_site(injector, __FILE__, __LINE__);
        }
        FL_THROW(fl_test_exception);
    }
    FL_CATCH(fault_injector_exception) {
        if (i != threshold) {
            FL_THROW_DETAILS(fl_test_exception, "expected throw at %d, got %d",
                             threshold, i);
        }
    }
    FL_END_TRY;

    // No sites should be recorded: try_throw_with_site only records in discovery mode
    if (fault_injector_get_site_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected no sites recorded outside discovery mode, got %lld",
                         (long long)fault_injector_get_site_count(injector));
    }
}

/**
 * @brief verify that fault_injector_reset restores all fields to their post-init
 * defaults regardless of prior state.
 *
 * Mutates discovery_mode, fault_index, injection_threshold, allocated_resources, and
 * fault_sites, then confirms reset clears each one. The faults and released_resources
 * sets happen to be empty before the call because enable_discovery clears faults and
 * resource2 was never acquired.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Reset", FaultTestCase, reset_injector, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy, dummy2;
    void          *resource  = &dummy;
    void          *resource2 = &dummy2;

    // Mutate observable state
    fault_injector_enable(injector);
    fault_injector_set_threshold(injector, 5);
    fault_injector_record_allocate(injector, resource, __FILE__, __LINE__);
    fault_injector_record_free(injector, resource2, __FILE__, __LINE__); // invalid free
    fault_injector_enable_discovery(
        injector); // clears faults/sites, sets discovery_mode
    fault_injector_try_throw_with_site(injector, __FILE__, __LINE__); // records a site

    fault_injector_reset(injector);

    if (!fault_injector_is_initialized(injector)) {
        FL_THROW_DETAILS(fl_test_exception, "expected initialized == true after reset");
    }
    if (fault_injector_is_enabled(injector)) {
        FL_THROW_DETAILS(fl_test_exception, "expected enabled == false after reset");
    }
    if (fault_injector_is_discovery_mode(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected discovery_mode == false after reset");
    }
    if (fault_injector_get_allocation_index(injector) != FAULT_INJECTOR_INITIAL_INDEX) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected fault_index == %d after reset, got %lld",
                         FAULT_INJECTOR_INITIAL_INDEX,
                         fault_injector_get_allocation_index(injector));
    }
    if (fault_injector_get_threshold(injector) != FAULT_INJECTOR_INITIAL_THRESHOLD) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected injection_threshold == %d after reset, got %lld",
                         FAULT_INJECTOR_INITIAL_THRESHOLD,
                         (long long)fault_injector_get_threshold(injector));
    }
    if (fault_injector_has_allocated_resources(injector)) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected allocated_resources empty after reset, got %lld",
                         (long long)fault_injector_get_allocated_count(injector));
    }
    if (fault_injector_get_released_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected released_resources empty after reset, got %lld",
                         (long long)fault_injector_get_released_count(injector));
    }
    if (fault_injector_get_invalid_address_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected faults empty after reset, got %lld",
                         (long long)fault_injector_get_invalid_address_count(injector));
    }
    if (fault_injector_get_site_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected fault_sites empty after reset, got %lld",
                         (long long)fault_injector_get_site_count(injector));
    }
}

/**
 * @brief verify that fault_injector_advance_threshold clears the faults buffer.
 *
 * The "Advance Threshold" test covers counter behavior; this covers the side-effect
 * that is symmetric with fault_injector_set_threshold clearing faults.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Advance Threshold Clears Faults", FaultTestCase,
                           advance_threshold_clears_faults, setup_config,
                           cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    // Populate the faults buffer via an invalid free
    fault_injector_record_free(injector, resource, __FILE__, __LINE__);
    if (fault_injector_get_invalid_address_count(injector) != 1) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected 1 fault before advance_threshold, got %lld",
                         (long long)fault_injector_get_invalid_address_count(injector));
    }

    fault_injector_advance_threshold(injector);

    if (fault_injector_get_invalid_address_count(injector) != 0) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected faults cleared by advance_threshold, got %lld",
                         (long long)fault_injector_get_invalid_address_count(injector));
    }
}

/**
 * @brief verify that fault_injector_get_fault returns NULL for an out-of-bounds index.
 */
FL_TYPE_TEST_SETUP_CLEANUP("Get Fault Out Of Bounds", FaultTestCase,
                           get_fault_out_of_bounds, setup_config, cleanup_config) {
    FaultInjector *injector = &t->injector;
    int            dummy;
    void          *resource = &dummy;

    // With no faults, index 0 is out of bounds
    if (fault_injector_get_fault(injector, 0) != NULL) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected NULL for index 0 when faults is empty");
    }

    // Add one fault via invalid free
    fault_injector_record_free(injector, resource, __FILE__, __LINE__);

    // Index 0 is now valid; index 1 is out of bounds
    if (fault_injector_get_fault(injector, 0) == NULL) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected non-NULL for index 0 after one fault");
    }
    if (fault_injector_get_fault(injector, 1) != NULL) {
        FL_THROW_DETAILS(fl_test_exception,
                         "expected NULL for index 1 with only one fault");
    }
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(test_fault_enable_disable)
FL_SUITE_ADD_EMBEDDED(default_values)
FL_SUITE_ADD_EMBEDDED(threshold)
FL_SUITE_ADD_EMBEDDED(disable_fault)
FL_SUITE_ADD_EMBEDDED(acquire_resource)
FL_SUITE_ADD_EMBEDDED(acquire_then_release)
FL_SUITE_ADD_EMBEDDED(double_free)
FL_SUITE_ADD_EMBEDDED(invalid_free)
FL_SUITE_ADD_EMBEDDED(resource_leak)
FL_SUITE_ADD_EMBEDDED(reinitialize)
FL_SUITE_ADD_EMBEDDED(reset_injector)
FL_SUITE_ADD_EMBEDDED(set_threshold_side_effects)
FL_SUITE_ADD_EMBEDDED(advance_threshold)
FL_SUITE_ADD_EMBEDDED(advance_threshold_clears_faults)
FL_SUITE_ADD_EMBEDDED(put_invalid_free)
FL_SUITE_ADD_EMBEDDED(get_fault_out_of_bounds)
FL_SUITE_ADD_EMBEDDED(discovery_mode)
FL_SUITE_ADD_EMBEDDED(try_throw_with_site)
FL_SUITE_END;

FL_GET_TEST_SUITE("Fault Injector", ts);
