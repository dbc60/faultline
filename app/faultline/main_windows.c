/**
 * @file main_windows.c
 * @author Douglas Cuthbertson
 * @brief Faultline test driver entry point (Windows)
 * @version 0.2
 * @date 2024-12-14
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include "faultline_commands.h" // ExecutionContext, get_faultline_commands, help_cmd
                                // (transitively: command.h, fl_context.h, arena.h,
                                //  flp_fault_memory_context.h, sqlite3.h)
#include "../../src/command.h"  // parse_command, RuntimeCommand, command_error/unknown
#include <faultline/flp_fault_memory_context.h> // FLFaultMemoryContext, flp_init_fault_memory_context

#include <faultline/arena.h>             // Arena, new_arena
#include <faultline/fl_context.h>        // FLContext, faultline_initialize
#include <faultline/fault_injector.h>    // fault_injector_create
#include <faultline/fl_log.h>            // LOG_ERROR
#include <faultline/fl_try.h>            // FL_TRY / FL_CATCH / FL_REASON / FL_DETAILS
#include <faultline/fl_memory_service.h> // FLA_SET_MEMORY_SERVICE_FN
#include <flp_log_service.h>    // flp_log_init_custom/set_level/set_output_path/cleanup
#include <flp_memory_service.h> // flp_init_fault_memory_service
#include <faultline_sqlite.h>   // faultline_init_database, faultline_close_database

#include <stdio.h>  // printf
#include <string.h> // strcmp

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
    Arena               *arena   = new_arena(0, 0);
    FLContext            fctx    = {0};
    FLFaultMemoryContext mem_ctx = {0};
    sqlite3             *db      = NULL;
    int volatile exit_code       = 0;

    flp_log_init_custom(LOG_LEVEL_INFO, "faultline.log");
    fctx.injector = fault_injector_create(arena);
    flp_init_fault_memory_context(&mem_ctx, arena, fctx.injector);
    flp_init_fault_memory_service(noop_fla_set_memory_service, &mem_ctx);

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
        printf("Usage: faultline <command> [options] [arguments]\n\n");
        RuntimeCommand help_request = {0};
        help_cmd(&help_request);
        exit_code = 1;
    }
    FL_CATCH(command_unknown) {
        printf("Error: Unknown command: %s.\n\n", FL_DETAILS);
        RuntimeCommand help_request = {0};
        help_cmd(&help_request);
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
