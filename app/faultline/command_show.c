/**
 * @file command_show.c
 * @author Douglas Cuthbertson
 * @brief Implementation of "show" subcommand handlers
 * @version 0.1
 * @date 2026-01-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "faultline_commands.h"
#include <faultline_sqlite.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Handler for "show results" command
 *
 * Lists test results with optional filtering
 * Options: --failures, --suite <name>, --limit <N>, --type, --phase, --with-source
 */
COMMAND_HANDLER(show_results_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    // For show commands, db is guaranteed non-NULL by validation
    // Extract options with defaults
    bool        failures_only = has_option(cmd, "failures");
    int         limit         = get_int_option(cmd, "limit", 10); // default 10
    char const *suite         = get_string_option(cmd, "suite", NULL);

    // Failure-specific options (auto-imply --failures via validation)
    // bool        show_source = has_option(cmd, "with-source");
    // char const *type_filter  = get_string_option(cmd, "type", NULL);
    // char const *phase_filter = get_string_option(cmd, "phase", NULL);

    // Call appropriate database query function
    if (failures_only) {
        // Show only failed runs
        faultline_show_test_failures(ectx->db, suite, limit, false);
    } else {
        // Show recent runs (default view)
        faultline_show_recent_runs(ectx->db, limit);
    }

    // Note: type_filter, phase_filter, and show_source will be used once
    // faultline_show_test_failures is enhanced to support these filters
}

/**
 * @brief Handler for "show result <id>" command
 *
 * Shows detailed information about a specific test run
 * Positional argument: run_id
 * Options: --format, --verbose
 */
COMMAND_HANDLER(show_result_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    // Validation ensures we have exactly 1 positional argument
    int run_id = atoi(cmd->args[0]);

    // Call database function to show run details
    faultline_show_run_details(ectx->db, run_id);
}

/**
 * @brief Handler for "show suites" command
 *
 * Lists all registered test suites
 * Options: --limit <N>, --format
 */
COMMAND_HANDLER(show_suites_cmd) {
    int limit = get_int_option(cmd, "limit", 10); // default 10

    // TODO: Implement faultline_show_suites() in faultline_sqlite.c
    // For now, print a placeholder message
    printf("Listing test suites (limit: %d):\n", limit);
    printf("TODO: Implement suite listing query\n");
}

/**
 * @brief Handler for "show suite <name>" command
 *
 * Shows detailed information about a specific test suite
 * Positional argument: suite_name
 * Options: --verbose, --format
 */
COMMAND_HANDLER(show_suite_cmd) {
    ExecutionContext *ectx = (ExecutionContext *)cmd;

    // Validation ensures we have exactly 1 positional argument
    char const *suite_name = cmd->args[0];
    bool        verbose    = has_option(cmd, "verbose");

    // Call database function to show suite summary
    faultline_show_suite_summary(ectx->db, suite_name);

    // TODO: Add verbose mode output (historical stats, trend analysis)
    if (verbose) {
        printf("\n[Verbose mode: detailed statistics would appear here]\n");
    }
}

/**
 * @brief Handler for "show cases" command
 *
 * Searches the test case catalog
 * Options: --suite <name>, --limit <N>, --format
 */
COMMAND_HANDLER(show_cases_cmd) {
    char const *suite = get_string_option(cmd, "suite", NULL);
    int         limit = get_int_option(cmd, "limit", 0); // 0 = unlimited

    // TODO: Implement test case catalog query
    printf("Searching test cases:\n");
    if (suite != NULL) {
        printf("  Suite: %s\n", suite);
    }
    printf("  Limit: %d\n", limit);
    printf("TODO: Implement test case catalog query\n");
}

/**
 * @brief Handler for "show hotspots" command
 *
 * Shows code locations that frequently fail
 * Options: --suite <name>, --limit <N>, --format, --verbose
 */
COMMAND_HANDLER(show_hotspots_cmd) {
    char const *suite   = get_string_option(cmd, "suite", NULL);
    int         limit   = get_int_option(cmd, "limit", 10); // default 10
    bool        verbose = has_option(cmd, "verbose");

    // TODO: Implement fault hotspots query
    printf("Fault hotspots (limit: %d):\n", limit);
    if (suite != NULL) {
        printf("  Filtered to suite: %s\n", suite);
    }
    if (verbose) {
        printf("  Verbose mode: ON\n");
    }
    printf("TODO: Implement hotspots query\n");
}

/**
 * @brief Handler for "show trends" command
 *
 * Shows performance trends over time
 * Options: --suite <name>, --limit <N>, --format
 */
COMMAND_HANDLER(show_trends_cmd) {
    char const *suite = get_string_option(cmd, "suite", NULL);
    int         limit = get_int_option(cmd, "limit", 10); // default 10

    // TODO: Implement trends query
    printf("Performance trends (limit: %d):\n", limit);
    if (suite != NULL) {
        printf("  Filtered to suite: %s\n", suite);
    }
    printf("TODO: Implement trends query\n");
}

/**
 * @brief Handler for top-level "show" command
 *
 * Delegates to the subcommand handler, propagating the full ExecutionContext
 * so subcommand handlers can access db, fctx, arena, etc.
 */
COMMAND_HANDLER(show_cmd) {
    if (cmd->subcommand != NULL) {
        // Build a new ExecutionContext that wraps the subcommand's RuntimeCommand
        // but inherits all driver fields (db, fctx, arena, mem_ctx) from the parent.
        ExecutionContext *parent  = (ExecutionContext *)cmd;
        ExecutionContext  sub_ctx = *parent;
        sub_ctx.cmd               = *cmd->subcommand;
        sub_ctx.cmd.command->handler((RuntimeCommand *)&sub_ctx);
    } else {
        fprintf(stderr,
                "Error: 'show' requires a subcommand (results, result, suites, etc.)\n");
        fprintf(stderr, "Try 'faultline help show' for usage information\n");
    }
}
