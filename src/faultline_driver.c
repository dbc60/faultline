/**
 * @file faultline_driver.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fault.h>                // for fault_buffer_count
#include <faultline/fault_injector.h>       // for fault_injector_init
#include <faultline/fl_context.h>           // for faultline_update_summa...
#include <faultline_driver.h>               // for FL_EXERCISE_TEST
#include <faultline/fl_result_codes.h>      // for FLResultCode
#include <faultline/fl_types.h>             // for FLFailureType
#include <faultline/fl_abbreviated_types.h> // for i64, u32
#include <faultline/fl_log.h>               // for LOG_DEBUG, LOG_VERBOSE
#include <faultline/fl_test.h>              // for FLTestSuite, FLTestCase
#include <faultline/fl_try.h>               // for FL_END_TRY, FL_TRY
#include <stdbool.h>                        // for true, bool, false
#include <stddef.h>                         // for NULL, size_t
#include <faultline/timer.h>                // for elapsed_win_seconds
#include <faultline/fault_site.h>           // for fault_site_buffer_copy
#include <faultline/fl_test_summary.h>      // for FLTestSummary
#include <faultline/fl_exception_service.h> // for FL_REASON, FL_DETAILS
#include <faultline/fl_exception_types.h>   // for FLExceptionReason
#include <faultline/arena.h>                // for arena_malloc_throw
#include "fault_injector_internal.h"        // for fault_injector_get_all...
#include "faultline_test_result.h"          // for test_result_clear, Tes...

static FLExceptionReason setup_failure = "setup failed";

/**
 * @brief run a timed test
 *
 * @param fctx a context used to track the current test case and test results.
 * @param timer measure how long the test takes to run.
 * @param setup_cleanup_result records failures in the setup or cleanup functions.
 * @param test_result records failures of the test itself.
 * @param phase controls whether failures on this call will be counted against the test
 * case. The discovery phase is when failures are counted against the test case. The
 * injection phase exercises the test case against injected faults.
 * @return FLResultCode
 */
static FLResultCode run_timed_test(FLContext *fctx, WinTimer *timer,
                                   TestResult *setup_cleanup_result,
                                   TestResult *test_result, FLTestPhase phase) {
    FLTestCase  *tc = fctx->ts->test_cases[fctx->index];
    FLResultCode rc = FL_PASS;

    if (tc->setup != NULL) {
        FL_TRY {
            tc->setup(tc);
        }
        FL_CATCH_ALL {
            if (FL_UNEXPECTED_EXCEPTION(FL_REASON)) {
                rc = FL_FAIL;
                test_result_init(setup_cleanup_result, FL_REASON, FL_DETAILS, FL_FILE,
                                 FL_LINE, rc, true, FL_SETUP_FAILURE);
                if (phase == FL_DISCOVERY_PHASE) {
                    fctx->setups_failed++;
                }
                FL_THROW(setup_failure);
            }
        }
        FL_END_TRY;
    }

    if (rc == FL_PASS) {
        // exercise the test case
        FL_TRY {
            start_win(timer);
            tc->test(tc);
        }
        FL_CATCH_ALL {
            if (FL_UNEXPECTED_EXCEPTION(FL_REASON)) {
                rc = FL_FAIL;
                test_result_init(test_result, FL_REASON, FL_DETAILS, FL_FILE, FL_LINE,
                                 rc, true, FL_TEST_FAILURE);
                if (phase == FL_DISCOVERY_PHASE) {
                    fctx->tests_failed++;
                }
            }
        }
        FL_FINALLY {
            stop_win(timer);
        }
        FL_END_TRY;
    }

    if (tc->cleanup != NULL) {
        FL_TRY {
            tc->cleanup(tc);
        }
        FL_CATCH_ALL {
            // Check that rc doesn't equal FL_FAILED so we don't overwrite a test
            // failure with a cleanup failure
            if (FL_UNEXPECTED_EXCEPTION(FL_REASON) && rc != FL_FAIL) {
                rc = FL_FAIL;
                test_result_init(setup_cleanup_result, FL_REASON, FL_DETAILS, FL_FILE,
                                 FL_LINE, rc, true, FL_CLEANUP_FAILURE);
                if (phase == FL_DISCOVERY_PHASE) {
                    fctx->cleanups_failed++;
                }
            }
        }
        FL_END_TRY;
    }

    return rc;
}

/**
 * @brief faultline_run_test runs the test case selected by the context.
 *
 * Testing takes place in two phases. In Phase 1, the test executes without fault
 * injection, but all of the
 *
 */
FL_EXERCISE_TEST(faultline_run_test) {
    FaultInjector *injector = fctx->injector;
    WinTimer       run_timer, test_timer;
    bool           triggered = false;
    FLTestSummary  summary;
    i64            total_fault_sites = 0;
    FLResultCode   rc;

    LOG_DEBUG("Run Test", "run test %s", fctx->ts->test_cases[fctx->index]->name);
    if (injector == NULL || !fault_injector_is_initialized(injector)) {
        fctx->injector = arena_malloc_throw(fctx->arena, sizeof *fctx->injector,
                                            __FILE__, __LINE__);
        injector       = fctx->injector;
        fault_injector_init(injector, fctx->arena);
    } else {
        fault_injector_reset(injector);
    }
    LOG_DEBUG("Run Test", "initialized injector");

    // Initialize the FLTestSummary object
    init_faultline_test_summary(&summary, fctx->arena, (u32)fctx->index, FL_PASS, NULL,
                                NULL);

    start_win(&run_timer);
    LOG_VERBOSE("Run Test", "started run timer");

    /*
     * PHASE 1: Discovery Pass - Run test case once without fault injection to discover
     * all fault sites
     */
    fault_injector_enable_discovery(injector);

    LOG_DEBUG("Run Test", "Phase 1: Discovery");
    // Run the test case and count the number of fault sites encounterd w/o injecting any
    // faults. Also collect metrics for this "main run"

    // Get the number of resources allocated before the test case is exercised
    i64 initial_resources = fault_injector_get_allocation_index(injector);

    // Prepare to capture discovery phase results
    TestResult setup_cleanup_result, discovery_result;
    test_result_clear(&setup_cleanup_result);
    test_result_clear(&discovery_result);

    // Run the test case once in discovery mode
    FL_TRY {
        rc = run_timed_test(fctx, &test_timer, &setup_cleanup_result, &discovery_result,
                            FL_DISCOVERY_PHASE);
    }
    FL_CATCH(setup_failure) {
        // continue
    }
    FL_END_TRY;

    fctx->tests_run++;
    bool discovery_counted_test_failure = discovery_result.unexpected_exception;

    // record the elapsed time for the test case
    ElapsedTime *elapsed
        = elapsed_time_buffer_allocate_next_free_slot(&fctx->elapsed_times);
    elapsed_time_init(elapsed, fctx->index, elapsed_win_seconds(&test_timer));

    // Record total number of fault sites discovered
    total_fault_sites = fault_injector_get_site_count(injector);

    // Immediate reporting of discovery phase results
    if (rc != FL_PASS) {
        if (setup_cleanup_result.unexpected_exception) {
            LOG_ERROR(fctx->ts->name, "%u. %s: DISCOVERY FAILED - %s (%s:%d)",
                      fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                      setup_cleanup_result.reason ? setup_cleanup_result.reason
                                                  : "unknown reason",
                      setup_cleanup_result.details ? setup_cleanup_result.details : "",
                      setup_cleanup_result.file ? setup_cleanup_result.file
                                                : "unknown file",
                      setup_cleanup_result.line);
        } else {
            LOG_WARN(fctx->ts->name, "%u. %s: DISCOVERY COMPLETED with result %d",
                     fctx->index + 1, fctx->ts->test_cases[fctx->index]->name, rc);
        }

        if (discovery_result.unexpected_exception) {
            LOG_ERROR(fctx->ts->name, "%u. %s: DISCOVERY FAILED - %s, %s. (%s:%d)",
                      fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                      discovery_result.reason ? discovery_result.reason
                                              : "unknown reason",
                      discovery_result.details ? discovery_result.details : "",
                      discovery_result.file ? discovery_result.file : "unknown file",
                      discovery_result.line);
        } else {
            LOG_WARN(fctx->ts->name, "%u. %s: DISCOVERY COMPLETED with result %d",
                     fctx->index + 1, fctx->ts->test_cases[fctx->index]->name, rc);
        }
    } else {
        LOG_VERBOSE(fctx->ts->name,
                    "%u. %s: Discovery completed successfully, found %lld fault sites",
                    fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                    total_fault_sites);
    }
    fault_injector_disable_discovery(injector);

    fault_site_buffer_copy(&fctx->fault_sites, &injector->fault_sites);

    // Reset for fault injection passes
    fault_injector_set_threshold(injector, FAULT_INJECTOR_INITIAL_THRESHOLD);

    // Validate discovery result for debugging
    faultline_validate_test_result(&discovery_result);

    // Update the existing summary with discovery phase results
    // This ensures detailed failure information is preserved for every test case
    //
    // Setup, test-body, and cleanup failures are mutually exclusive: a setup failure
    // throws before the test body runs; a test-body failure causes the cleanup catch
    // to bail early (rc already FL_FAIL). Pick whichever result captured the
    // exception so that reason, failure_type, and fault detail all come from it.
    TestResult *failed_discovery_result
        = discovery_result.unexpected_exception       ? &discovery_result
          : setup_cleanup_result.unexpected_exception ? &setup_cleanup_result
                                                      : NULL;

    summary.code   = rc;
    summary.reason = failed_discovery_result ? failed_discovery_result->reason : NULL;
    summary.faults_exercised = total_fault_sites;

    FLFailureType failure_type = failed_discovery_result
                                     ? failed_discovery_result->failure_type
                                     : FL_FAILURE_NONE;
    faultline_update_summary_phase_info(&summary, FL_DISCOVERY_PHASE, failure_type,
                                        elapsed_win_seconds(&test_timer));

    // Record detailed failure information if there was a failure during discovery
    if (rc != FL_PASS && failed_discovery_result) {
        faultline_test_summary_add_fault(&summary, 0, rc, FAULT_NO_RESOURCE,
                                         failed_discovery_result->reason,
                                         failed_discovery_result->details,
                                         failed_discovery_result->file,
                                         failed_discovery_result->line);
    }

    // Update legacy counters for backward compatibility
    if (total_fault_sites == 0) {
        switch (rc) {
        case FL_PASS:
            fctx->tests_passed++;
            break;
        case FL_LEAK:
            fctx->tests_passed_with_leaks++;
            break;
        default:
            // fctx->tests_failed already incremented in run_timed_test.
            break;
        }
    }

    /*
     * PHASE 2: Fault Injection Passes - Test each fault site sequentially.
     */
    i64 current_pass = 0;
    while (current_pass < total_fault_sites) {
        current_pass++;

        // Enhanced progress reporting - show current fault being tested with context
        if (total_fault_sites > 0) {
            i64 current_threshold = fault_injector_get_threshold(injector);
            if (current_threshold <= total_fault_sites) {
                // Get fault site information for better context
                FaultSite *site
                    = fault_injector_get_site(injector, (size_t)(current_threshold - 1));
                if (site) {
                    LOG_VERBOSE(fctx->ts->name,
                                "%u. %s: injecting fault %lld/%lld (%d%%) at %s:%d",
                                fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                                current_threshold, total_fault_sites,
                                (int)((current_threshold * 100) / total_fault_sites),
                                site->file ? site->file : "unknown", site->line);
                } else {
                    LOG_VERBOSE(fctx->ts->name,
                                "%u. %s: injecting fault %lld/%lld (%d%%)",
                                fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                                current_threshold, total_fault_sites,
                                (int)((current_threshold * 100) / total_fault_sites));
                }
            }
        }

        // Get the number of resources allocated before the test case is exercised
        initial_resources = fault_injector_get_allocation_index(injector);

        // enable the fault and set the throw count
        fault_injector_enable(injector);

        // Prepare to capture injection phase results
        TestResult injection_setup_cleanup;
        TestResult injection_result;
        test_result_clear(&injection_setup_cleanup);
        test_result_clear(&injection_result);

        FL_TRY {
            rc = run_timed_test(fctx, &test_timer, &injection_setup_cleanup,
                                &injection_result, FL_INJECTION_PHASE);
        }
        FL_CATCH(setup_failure) {
            // continue
        }
        FL_END_TRY;

        triggered = fault_injector_triggered(injector);
        fault_injector_disable(injector);
        i64 leaks = fault_injector_get_allocated_count(injector) - initial_resources;

        /*
         * triggered is true if the test case exercised on a fault (not the
         * main path). These results are recorded as test-point results so the driver
         * can keep metrics for each fault separate from the main path. I believe
         * that can help with debugging.
         */
        if (triggered) {
            i64    fault_index    = fault_injector_get_threshold(injector);
            double injection_time = elapsed_win_seconds(&test_timer);

            // Validate injection result for debugging
            faultline_validate_test_result(&injection_result);

            // Immediate failure reporting with structured logging
            bool had_failure = false;

            // A test exception during injection is expected: the injected fault
            // propagated up and caused an assertion to fail. Log at DEBUG level so
            // it's still visible when diagnosing problems, but don't treat it as a
            // failure — only leaks and invalid frees represent real bugs.
            if (injection_result.unexpected_exception) {
                LOG_DEBUG(fctx->ts->name,
                          "%u. %s: INJECTION FAULT %lld - %s (%s) at %s:%d",
                          fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                          fault_index,
                          injection_result.reason ? injection_result.reason
                                                  : "unknown reason",
                          injection_result.failure_type == FL_SETUP_FAILURE  ? "SETUP"
                          : injection_result.failure_type == FL_TEST_FAILURE ? "TEST"
                          : injection_result.failure_type == FL_CLEANUP_FAILURE
                              ? "CLEANUP"
                              : "UNKNOWN",
                          injection_result.file ? injection_result.file : "unknown file",
                          injection_result.line);
            }

            // Report invalid address/free failures immediately
            i64 invalid_count = fault_injector_get_invalid_address_count(injector);
            if (invalid_count > 0) {
                LOG_ERROR(
                    fctx->ts->name,
                    "%u. %s: INJECTION FAULT %lld - Invalid resource release detected",
                    fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                    fault_index);

                faultline_record_injection_failure(&summary, fault_index,
                                                   FL_INVALID_FREE,
                                                   FL_INVALID_FREE_FAILURE,
                                                   &injection_result, injection_time,
                                                   "invalid free",
                                                   "invalid resource release", __FILE__,
                                                   __LINE__);
                had_failure = true;
            }

            // Report memory leak failures immediately
            if (leaks > 0) {
                LOG_ERROR(fctx->ts->name,
                          "%u. %s: INJECTION FAULT %lld - Resource leak detected (%lld "
                          "resources)",
                          fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                          fault_index, leaks);

                // Record detailed leak information with actual allocation sites
                size_t initial_fault_count = fault_buffer_count(&summary.fault_buffer);
                fault_injector_record_leak_details(injector, &summary, fault_index);
                size_t final_fault_count = fault_buffer_count(&summary.fault_buffer);

                // Only mark as leak if we actually recorded fault details
                if (final_fault_count > initial_fault_count) {
                    // Update summary with injection phase failure information
                    faultline_update_summary_phase_info(&summary, FL_INJECTION_PHASE,
                                                        FL_LEAK_FAILURE, injection_time);

                    // Set the overall result code to indicate leak
                    summary.code = FL_LEAK;

                    had_failure = true;
                } else {
                    // False positive - infrastructure leak, not test-related
                    LOG_WARN(fctx->ts->name,
                             "%u. %s: Infrastructure leak detected (%lld resources) - "
                             "not test-related",
                             fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                             leaks);
                }
            }

            // Report successful fault handling
            if (!had_failure) {
                LOG_DEBUG("Run Test",
                          "%u. %s: Injection fault %lld handled successfully",
                          fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                          fault_index);
            }
        }
        fault_injector_advance_threshold(injector);
    }

    // Count this test case as failed at most once, only if discovery didn't already.
    if (summary.code != FL_PASS && !discovery_counted_test_failure) {
        fctx->tests_failed++;
    }

    stop_win(&run_timer);
    LOG_VERBOSE("Run Test", "stopped run timer");

    // Store the elapsed time and total fault sites in the result object
    summary.elapsed_seconds  = elapsed_win_seconds(&run_timer);
    summary.faults_exercised = total_fault_sites;

    // Store the final summary with all results from discovery and injection phases
    FLTestSummary *final_summary
        = faultline_test_summary_buffer_allocate_next_free_slot(&fctx->results);
    *final_summary = summary; // Copy the summary we've been building

    fault_injector_reset(injector);
}
