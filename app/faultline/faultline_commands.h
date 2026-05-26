/**
 * @file faultline_commands.h
 * @author Douglas Cuthbertson
 * @brief Faultline-specific command definitions and handlers
 * @version 0.1
 * @date 2026-01-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#ifndef FAULTLINE_COMMANDS_H_
#define FAULTLINE_COMMANDS_H_

#include "../../src/command.h"
#include <faultline/fl_context.h>
#include <faultline/fl_memory_service.h> // FLMemoryContext (forward declaration)
#include <faultline/arena.h>             // Arena
#include <sqlite/sqlite3.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Execution context passed to all command handlers.
 *
 * IMPORTANT: RuntimeCommand must be the first field so that handlers can safely
 * cast between `const RuntimeCommand *cmd` (their parameter type) and
 * `ExecutionContext *` to access the driver fields. main() builds one of these,
 * copies the parsed RuntimeCommand into the `cmd` member, fills the remaining
 * fields, and dispatches via `cmd.command->handler((RuntimeCommand *)&exec_ctx)`.
 *
 * Lifetime: Created once in main(), passed to all command handlers.
 * Ownership: main() owns the context; handlers borrow but do not take ownership.
 * Database: db may be NULL if --no-db was specified or db init failed.
 */
typedef struct {
    RuntimeCommand   cmd;       // MUST be first - enables handler type-pun
    sqlite3         *db;        // Database connection (may be NULL)
    FLContext       *fctx;      // Test execution context (run command only)
    int              log_level; // Configured logging level
    Arena           *arena;     // owned by main(), borrowed by command handlers
    FLMemoryContext *mem_ctx;   // initialized from arena, borrowed by command handlers
    char const      *junit_xml_path; // path to output for writing JUNIT XML
} ExecutionContext;

// Get the faultline command table
FormalCommand const *get_faultline_commands(void);

// Top-level command handlers
COMMAND_HANDLER(run_cmd);
COMMAND_HANDLER(show_cmd);
COMMAND_HANDLER(help_cmd);
COMMAND_HANDLER(version_cmd);

// Show subcommand handlers
COMMAND_HANDLER(show_results_cmd);
COMMAND_HANDLER(show_result_cmd);
COMMAND_HANDLER(show_suites_cmd);
COMMAND_HANDLER(show_suite_cmd);
COMMAND_HANDLER(show_cases_cmd);
COMMAND_HANDLER(show_hotspots_cmd);
COMMAND_HANDLER(show_trends_cmd);

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_COMMANDS_H_
