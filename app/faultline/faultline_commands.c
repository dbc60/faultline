/**
 * @file faultline_commands.c
 * @author Douglas Cuthbertson
 * @brief Faultline command definitions and option tables
 * @version 0.1
 * @date 2026-01-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "faultline_commands.h"

#include <string.h> // strcmp

// ============================================================================
// Option Definitions
// ============================================================================

// CoFmmon options (database and logging), shared by commands that dispatch to
// subcommands (show, baseline) so the global options may precede the subcommand.
static FormalOption common_options[] = {
    {"d", "db", "Database file path (default: ./faultline_results.sqlite)", true, NULL},
    {NULL, "no-db", "Disable database storage (run command only)", false, NULL},
    {"l", "log-level", "Set log verbosity (error|warn|info|debug)", true, NULL},
    {NULL, "log-file", "Log output destination file", true, NULL},
    {NULL, NULL, NULL, false, NULL}, // NULL terminator
};

// Run command options
static FormalOption run_options[] = {
    {"d", "db", "Database file path (default: ./faultline_results.sqlite)", true, NULL},
    {NULL, "no-db", "Disable database storage", false, NULL},
    {"l", "log-level", "Set log verbosity (error|warn|info|debug)", true, NULL},
    {NULL, "log-file", "Log output destination file", true, NULL},
    {NULL, "junit-xml",
     "Write JUNIT XML output to PATH (must include filename, e.g. results/junit.xml)",
     true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Show results options
static FormalOption results_options[] = {
    {"f", "failures", "Filter to failed runs only", false, NULL},
    {"s", "suite", "Filter by suite name", true, NULL},
    {NULL, "since", "Time-based filtering (YYYY-MM-DD)", true, NULL},
    {"n", "limit", "Limit result count (default: 10, 0=unlimited)", true, NULL},
    {NULL, "type", "Filter by failure type (setup|test|cleanup|leak|invalid_free)", true,
     NULL},
    {NULL, "phase", "Filter by test phase (discovery|injection)", true, NULL},
    {NULL, "with-source", "Include source file/line information", false, NULL},
    {NULL, "format", "Output format (text|json|csv)", true, NULL},
    {NULL, "sort", "Sort by field", true, NULL},
    {"v", "verbose", "Show detailed output", false, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Show result (single) options
static FormalOption result_options[] = {
    {NULL, "format", "Output format (text|json|csv)", true, NULL},
    {"v", "verbose", "Show detailed output", false, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Show suites options
static FormalOption suites_options[] = {
    {"n", "limit", "Limit result count (default: 10, 0=unlimited)", true, NULL},
    {NULL, "format", "Output format (text|json|csv)", true, NULL},
    {"v", "verbose", "Show detailed output", false, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Show suite (single) options
static FormalOption suite_options[] = {
    {NULL, "format", "Output format (text|json|csv)", true, NULL},
    {"v", "verbose", "Show detailed output", false, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Show cases options
static FormalOption cases_options[] = {
    {"s", "suite", "Filter by suite name", true, NULL},
    {"n", "limit", "Limit result count (default: 0=unlimited)", true, NULL},
    {NULL, "format", "Output format (text|json|csv)", true, NULL},
    {"v", "verbose", "Show detailed output", false, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Show hotspots options
static FormalOption hotspots_options[] = {
    {"s", "suite", "Filter by suite name", true, NULL},
    {"n", "limit", "Limit result count (default: 0=unlimited)", true, NULL},
    {NULL, "format", "Output format (text|json|csv)", true, NULL},
    {"v", "verbose", "Show detailed output", false, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Show trends options
static FormalOption trends_options[] = {
    {"s", "suite", "Filter by suite name", true, NULL},
    {"n", "limit", "Limit result count (default: 0=unlimited)", true, NULL},
    {NULL, "format", "Output format (text|json|csv)", true, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Show regressions options
static FormalOption regressions_options[] = {
    {"s", "suite", "Filter by suite name", true, NULL},
    {"n", "limit", "Limit result count (default: 0=unlimited)", true, NULL},
    {"t", "threshold", "Runtime regression threshold percent (default: 20)", true, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// Baseline reset options
static FormalOption reset_options[] = {
    {"s", "suite", "Limit to a single suite", true, NULL},
    {"t", "test", "Limit to a single test case", true, NULL},
    {"d", "db", "Database file path", true, NULL},
    {NULL, NULL, NULL, false, NULL},
};

// ============================================================================
// Subcommand Definitions
// ============================================================================

// Show subcommands
static FormalCommand show_subcommands[] = {
    {"results", "List test results", show_results_cmd, results_options, NULL, NULL},
    {"result", "Show specific result details", show_result_cmd, result_options, NULL,
     NULL},
    {"suites", "List registered test suites", show_suites_cmd, suites_options, NULL,
     NULL},
    {"suite", "Show suite info and test cases", show_suite_cmd, suite_options, NULL,
     NULL},
    {"cases", "Search test case catalog", show_cases_cmd, cases_options, NULL, NULL},
    {"hotspots", "Show frequently failing locations", show_hotspots_cmd,
     hotspots_options, NULL, NULL},
    {"trends", "Show performance trends", show_trends_cmd, trends_options, NULL, NULL},
    {"regressions", "Show coverage/runtime regressions vs baseline",
     show_regressions_cmd, regressions_options, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL, NULL}, // NULL terminator
};

// Baseline subcommands
static FormalCommand baseline_subcommands[] = {
    {"reset", "Re-pin baselines to each test's latest run", baseline_reset_cmd,
     reset_options, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL, NULL}, // NULL terminator
};

// ============================================================================
// Top-Level Command Definitions
// ============================================================================

static FormalCommand commands[] = {
    {"run", "Execute test suites", run_cmd, run_options, NULL,
     "<suite.dll> [suite.dll ...]"},
    {"show", "Display test results", show_cmd, common_options, show_subcommands, NULL},
    {"baseline", "Manage regression baselines", baseline_cmd, common_options,
     baseline_subcommands, NULL},
    {"help", "Display help information", help_cmd, NULL, NULL, NULL},
    {"version", "Display version information", version_cmd, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL, NULL}, // NULL terminator
};

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Get the faultline command table
 *
 * @return FormalCommand const * NULL-terminated array of commands
 */
FormalCommand const *get_faultline_commands(void) {
    return commands;
}

FormalOption const *get_faultline_global_options(void) {
    // Options accepted before the command (e.g. `faultline --db x show ...`).
    return common_options;
}

FLLogLevel parse_log_level(char const *level_str) {
    if (level_str == NULL) {
        return LOG_LEVEL_INFO;
    }
    if (strcmp(level_str, "error") == 0) {
        return LOG_LEVEL_ERROR;
    }
    if (strcmp(level_str, "warn") == 0) {
        return LOG_LEVEL_WARN;
    }
    if (strcmp(level_str, "debug") == 0) {
        return LOG_LEVEL_DEBUG;
    }
    return LOG_LEVEL_INFO;
}
