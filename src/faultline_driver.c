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
#include <stdbool.h>                        // for true, bool, false
#include <stddef.h>                         // for NULL, size_t
#include <faultline/fl_timer.h>             // for FL_TIMER_SERVICE
#include <faultline/fl_stopwatch.h>         // for FLStopwatch, fl_stopwatch_*
#include <faultline/fault_site.h>           // for fault_site_buffer_copy
#include <faultline/fl_test_summary.h>      // for FLTestSummary
#include "fault_injector_internal.h"        // for fault_injector_get_all...
#include "faultline_test_result.h"          // for test_result_clear, Tes...

/**
 * @brief Run one test case in the suite module and record what it reported.
 *
 * The module contains the case's exceptions and reports an FLCaseOutcome, so there is
 * no exception handling here. An expected failure is not a failure: the module reports
 * it as FL_CASE_EXPECTED_FAILURE, which this reads as FL_PASS.
 *
 * @param fctx a context used to track the current test case and test results.
 * @param out receives the module's report, including the time the test body took.
 * @param setup_cleanup_result records failures in the setup or cleanup functions.
 * @param test_result records failures of the test itself.
 * @param phase controls whether failures on this call will be counted against the test
 * case. The discovery phase is when failures are counted against the test case. The
 * injection phase exercises the test case against injected faults.
 * @return FLResultCode
 */
static FLResultCode run_test_case(FLContext *fctx, FLCaseOutcome *out,
                                  TestResult *setup_cleanup_result,
                                  TestResult *test_result, FLTestPhase phase) {
    FLRunCaseResult ran = fctx->run_case(fctx->index, out, sizeof *out);

    // Both non-OK values report an invalid argument: an index at or past ts->count, or
    // an out_size smaller than the module's sizeof(FLCaseOutcome). No case ran, so
    // there is no test result to record.
    if (ran != FL_RUN_CASE_OK) {
        LOG_ERROR(fctx->ts->name, "%zu. fl_run_case returned an error: %s",
                  fctx->index + 1, fl_run_case_result_str(ran));
        return FL_FAIL;
    }

    if (out->status != FL_CASE_UNEXPECTED_FAILURE) {
        return FL_PASS;
    }

    // A setup or cleanup failure and a test-body failure are separate results.
    TestResult *target
        = (out->failure_type == FL_TEST_FAILURE) ? test_result : setup_cleanup_result;
    test_result_init(target, out->reason, out->details, out->file, out->line, FL_FAIL,
                     true, out->failure_type);

    if (phase == FL_DISCOVERY_PHASE) {
        switch (out->failure_type) {
        case FL_SETUP_FAILURE:
            fctx->setups_failed++;
            break;
        case FL_TEST_FAILURE:
            fctx->tests_failed++;
            break;
        case FL_CLEANUP_FAILURE:
            fctx->cleanups_failed++;
            break;
        case FL_FAILURE_NONE:
        case FL_LEAK_FAILURE:
        case FL_INVALID_FREE_FAILURE:
        default:
            // fl_run_case reports only the three phase failures (setup, cleanup, or
            // test) above, so any other value means the module and this driver were
            // built with different definitions of FLFailureType.
            LOG_ERROR(fctx->ts->name,
                      "%zu. unrecognized failure type %d from fl_run_case",
                      fctx->index + 1, (int)out->failure_type);
            break;
        }
    }

    return FL_FAIL;
}

/**
 * @brief faultline_run_test runs the test case selected by the context.
 *
 * Testing takes place in two phases. In Phase 1, the test executes without fault
 * injection, but all of the
 *
 */
FL_EXERCISE_TEST(faultline_run_test) {
    FaultInjector *injector  = fctx->injector;
    FLStopwatch    run_timer = fl_stopwatch_make(FL_TIMER_SERVICE());
    bool           triggered = false;
    FLTestSummary  summary;
    i64            total_fault_sites = 0;
    FLResultCode   rc                = FL_FAIL;

    LOG_DEBUG("Run Test", "run test %s", fctx->ts->test_cases[fctx->index]->name);
    if (injector == NULL || !fault_injector_is_initialized(injector)) {
        fctx->injector = fault_injector_create(fctx->arena);
        injector       = fctx->injector;
    } else {
        fault_injector_reset(injector);
    }
    LOG_DEBUG("Run Test", "initialized injector");

    // Initialize the FLTestSummary object
    init_faultline_test_summary(&summary, fctx->arena, (u32)fctx->index, FL_PASS, NULL,
                                NULL);

    fl_stopwatch_start(&run_timer);
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
    i64 initial_resources = fault_injector_get_allocated_count(injector);

    // Prepare to capture discovery phase results
    TestResult setup_cleanup_result, discovery_result;
    test_result_clear(&setup_cleanup_result);
    test_result_clear(&discovery_result);

    // Run the test case once in discovery mode
    FLCaseOutcome discovery_outcome = {0};
    rc = run_test_case(fctx, &discovery_outcome, &setup_cleanup_result,
                       &discovery_result, FL_DISCOVERY_PHASE);

    fctx->tests_run++;
    bool discovery_counted_test_failure = discovery_result.unexpected_exception;

    // Record total number of fault sites discovered
    total_fault_sites = fault_injector_get_site_count(injector);

    // Immediate reporting of discovery phase results
    if (rc != FL_PASS) {
        if (setup_cleanup_result.unexpected_exception) {
            LOG_ERROR(fctx->ts->name, "%zu. %s: DISCOVERY FAILED - %s, %s. (%s:%d)",
                      fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                      setup_cleanup_result.reason ? setup_cleanup_result.reason
                                                  : "unknown reason",
                      setup_cleanup_result.details ? setup_cleanup_result.details : "",
                      setup_cleanup_result.file ? setup_cleanup_result.file
                                                : "unknown file",
                      setup_cleanup_result.line);
        } else {
            LOG_WARN(fctx->ts->name, "%zu. %s: DISCOVERY COMPLETED with result %d",
                     fctx->index + 1, fctx->ts->test_cases[fctx->index]->name, rc);
        }

        if (discovery_result.unexpected_exception) {
            LOG_ERROR(fctx->ts->name, "%zu. %s: DISCOVERY FAILED - %s, %s. (%s:%d)",
                      fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                      discovery_result.reason ? discovery_result.reason
                                              : "unknown reason",
                      discovery_result.details ? discovery_result.details : "",
                      discovery_result.file ? discovery_result.file : "unknown file",
                      discovery_result.line);
        } else {
            LOG_WARN(fctx->ts->name, "%zu. %s: DISCOVERY COMPLETED with result %d",
                     fctx->index + 1, fctx->ts->test_cases[fctx->index]->name, rc);
        }
    } else {
        LOG_VERBOSE(fctx->ts->name,
                    "%zu. %s: Discovery completed successfully, found %lld fault sites",
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
                                        discovery_outcome.elapsed_seconds);

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
        default:
            // fctx->tests_failed already incremented in run_test_case.
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
                                "%zu. %s: injecting fault %lld/%lld (%d%%) at %s:%d",
                                fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                                current_threshold, total_fault_sites,
                                (int)((current_threshold * 100) / total_fault_sites),
                                site->file ? site->file : "unknown", site->line);
                } else {
                    LOG_VERBOSE(fctx->ts->name,
                                "%zu. %s: injecting fault %lld/%lld (%d%%)",
                                fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                                current_threshold, total_fault_sites,
                                (int)((current_threshold * 100) / total_fault_sites));
                }
            }
        }

        // Get the number of resources allocated before the test case is exercised
        initial_resources = fault_injector_get_allocated_count(injector);

        // enable the fault and set the throw count
        fault_injector_enable(injector);

        // Prepare to capture injection phase results
        TestResult injection_setup_cleanup;
        TestResult injection_result;
        test_result_clear(&injection_setup_cleanup);
        test_result_clear(&injection_result);

        // Zeroed per pass so a pass that reports no case leaves zero elapsed time
        // rather than the prior pass's.
        FLCaseOutcome injection_outcome = {0};
        rc = run_test_case(fctx, &injection_outcome, &injection_setup_cleanup,
                           &injection_result, FL_INJECTION_PHASE);

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
            double injection_time = injection_outcome.elapsed_seconds;

            // Validate injection result for debugging
            faultline_validate_test_result(&injection_result);

            // Immediate failure reporting with structured logging
            bool had_failure = false;

            // A test exception during injection is expected: the injected fault
            // propagated up and caused an assertion to fail. Log at DEBUG level so
            // it's still visible when diagnosing problems, but don't treat it as a
            // failure. Only memory leaks and invalid frees represent real bugs.
            if (injection_result.unexpected_exception) {
                LOG_DEBUG(fctx->ts->name,
                          "%zu. %s: INJECTION FAULT %lld - %s (%s) at %s:%d",
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
                    "%zu. %s: INJECTION FAULT %lld - Invalid resource release detected",
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
                          "%zu. %s: INJECTION FAULT %lld - Resource leak detected (%lld "
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
                             "%zu. %s: Infrastructure leak detected (%lld resources) - "
                             "not test-related",
                             fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                             leaks);
                }
            }

            // Report successful fault handling
            if (!had_failure) {
                LOG_DEBUG("Run Test",
                          "%zu. %s: Injection fault %lld handled successfully",
                          fctx->index + 1, fctx->ts->test_cases[fctx->index]->name,
                          fault_index);
            }
        }
        fault_injector_advance_threshold(injector);
    }

    // Count this test case in exactly one bucket: passed (implicit), passed with leaks,
    // or failed, and as failed at most once, only if discovery didn't already count it.
    if (!discovery_counted_test_failure) {
        if (summary.code == FL_LEAK) {
            // The test body passed; the injected faults exposed resource leaks.
            fctx->tests_passed_with_leaks++;
        } else if (summary.code != FL_PASS) {
            fctx->tests_failed++;
        }
    }

    fl_stopwatch_stop(&run_timer);
    LOG_VERBOSE("Run Test", "stopped run timer");

    // Store the elapsed time and total fault sites in the result object
    summary.elapsed_seconds  = fl_stopwatch_elapsed_seconds(&run_timer);
    summary.faults_exercised = total_fault_sites;

    // Store the final summary with all results from discovery and injection phases
    FLTestSummary *final_summary
        = faultline_test_summary_buffer_allocate_next_free_slot(&fctx->results);
    *final_summary = summary; // Copy the summary we've been building

    fault_injector_reset(injector);
}
