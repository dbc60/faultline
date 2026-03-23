/**
 * @file main_windows.c
 * @author Douglas Cuthbertson
 * @brief Faultline test driver entry point (Windows)
 * @version 0.2
 * @date 2024-12-14
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

// Must be defined before any headers are included so that fl_log.h and fl_try.h
// select the platform (driver) implementations of the log and exception services.
#ifndef FL_BUILD_DRIVER
#define FL_BUILD_DRIVER
#endif

// Unity build: pull all implementation source into this translation unit.
#include "../../src/arena.c"
#include "../../src/arena_dbg.c"
#include "../../src/arena_malloc.c"
#include "../../src/buffer.c"
#include "../../src/command.c"
#include "../../src/digital_search_tree.c"
#include "../../src/fault_injector.c"
#include "../../src/faultline_context.c"
#include "../../src/faultline_driver.c"
#include "../../src/faultline_sqlite.c"
#include "../../src/fl_exception_service.c"
#include "../../src/flp_exception_service.c"
#include "../../src/flp_log_service.c"
#include "../../src/flp_memory_service.c"
#include "../../src/output_junit.c"
#include "../../third_party/fnv/FNV64.c"
#include "../../src/region.c"
#include "../../src/region_node.c"
#include "../../src/set.c"
#include "../../src/win_timer.c"

// Command infrastructure
#include "faultline_commands.c"
#include "command_run.c"
#include "command_show.c"
#include "command_help.c"

#include "../../src/flp_memory_context.h" // FLMemoryContext (full definition)

#include "../../src/command.h"        // parse_command, has_option, get_string_option
#include <faultline/fault_injector.h> // fault_injector_init
#include <faultline/fl_context.h>     // faultline_initialize
#include <faultline_sqlite.h>   // faultline_init_database, faultline_close_database
#include <flp_log_service.h>    // flp_log_init, flp_log_set_level, flp_log_cleanup
#include <flp_memory_service.h> // flp_init_memory_service, flp_init_memory_context

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static char const *module = "Faultline";

// No-op fla_set for the unity build: flp_init_memory_service requires a non-NULL
// setter, but DLL injection is handled per-suite in command_run.c. The main exe
// uses FL_BUILD_DRIVER, so FL_MALLOC routes directly through flp_malloc, not through
// g_fla_memory_service.
static FLA_SET_MEMORY_SERVICE_FN(noop_fla_set_memory_service) {
    (void)svc;
    (void)size;
}

/**
 * @brief Parse --log-level option string to FLLogLevel enum.
 */
static FLLogLevel parse_log_level(char const *level_str) {
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

int main(int argc, char **argv) {
    Arena          *arena   = new_arena(0, 0);
    FLContext       fctx    = {0};
    FLMemoryContext mem_ctx = {0};
    sqlite3        *db      = NULL;
    int volatile exit_code  = 0;

    flp_log_init_custom(LOG_LEVEL_INFO, "faultline.log");
    fctx.injector = arena_malloc_throw(arena, sizeof *fctx.injector, __FILE__, __LINE__);
    fault_injector_init(fctx.injector, arena);
    flp_init_memory_context(&mem_ctx, arena, fctx.injector);
    flp_init_memory_service(noop_fla_set_memory_service, &mem_ctx);

    FL_TRY {
        RuntimeCommand *parsed_cmd = parse_command(get_faultline_commands(), argc, argv);

        // Configure logging from parsed options
        FLLogLevel log_level
            = parse_log_level(get_string_option(parsed_cmd, "log-level", NULL));
        flp_log_set_level(log_level);

        char const *log_file = get_string_option(parsed_cmd, "log-file", NULL);
        if (log_file != NULL) {
            flp_log_set_output_path(log_file);
        }

        // Initialize database unless --no-db is specified
        if (!has_option(parsed_cmd, "no-db")) {
            char const *db_path
                = get_string_option(parsed_cmd, "db", "./faultline_results.sqlite");
            FL_TRY {
                db = faultline_init_database(db_path);
                if (db == NULL) {
                    LOG_ERROR(module, "Failed to open database at \"%s\"", db_path);
                }
            }
            FL_CATCH_ALL {
                LOG_ERROR(module, "Database initialization failed: %s", FL_REASON);
                db = NULL;
            }
            FL_END_TRY;
        }

        faultline_initialize(&fctx, NULL, arena);

        // Build the combined execution context (RuntimeCommand must be first field)
        ExecutionContext ectx = {
            .cmd            = *parsed_cmd,
            .db             = db,
            .fctx           = &fctx,
            .log_level      = (int)log_level,
            .arena          = arena,
            .mem_ctx        = &mem_ctx,
            .junit_xml_path = get_string_option(parsed_cmd, "junit-xml", NULL),
        };

        // Dispatch to command handler
        ectx.cmd.command->handler((RuntimeCommand *)&ectx);
    }
    FL_CATCH(command_error) {
        printf("Usage: faultline <command> [options] [arguments]\n");
        printf("Try 'faultline help' for usage information.\n");
        exit_code = 1;
    }
    FL_CATCH(command_unknown) {
        printf("Error: Unknown command: %s.\n", FL_DETAILS);
        printf("Try 'faultline help' for usage information.\n");
        exit_code = 1;
    }
    FL_CATCH_ALL {
        LOG_ERROR(module, "Unhandled exception: %s", FL_REASON);
        exit_code = 1;
    }
    FL_END_TRY;

    if (db != NULL) {
        faultline_close_database(db);
    }
    flp_log_cleanup();
    return exit_code;
}
