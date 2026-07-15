/**
 * @file faultline_app_main.c
 * @author Douglas Cuthbertson
 * @brief OS-free application entry point for the platform/app split.
 * @version 0.1
 * @date 2026-06-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This is the "application layer" counterpart to the thin Win32 host in
 * win32_faultline_main.c. The host builds an FLPlatformAPI and calls
 * faultline_app_main(); this function reaches the OS only through `platform`.
 *
 * Logging configuration and suite loading route through the platform;
 * parsing/dispatch match the monolithic driver.
 */
#include "faultline_commands.h" // ExecutionContext, get_faultline_commands, help_cmd
#include "command.h"            // parse_command_with_globals, RuntimeCommand, command_*
#include <platform_api.h>       // FLPlatformAPI, FL_APP_MAIN

// Application-side service accessors: installing into these globals is what makes
// FL_TRY / FL_MALLOC / LOG_* in the core route through the platform.
#include <faultline/fla_exception_service.h> // fla_set_exception_service
#include <faultline/fla_memory_service.h>    // fla_set_memory_service
#include <faultline/fla_log_service.h>       // fla_set_log_service
#include <faultline/fla_timer_service.h>     // fla_set_timer_service
#include <faultline/fla_file_service.h>      // fla_set_file_service
#include <faultline/fl_try.h>                // FL_TRY/FL_CATCH (selects fla_ backend)

#include <faultline/fl_context.h>     // FLContext, faultline_initialize
#include <faultline/fl_log.h>         // LOG_ERROR
#include <faultline/fl_log_service.h> // FLLogLevel
#include <faultline_sqlite.h> // faultline_init_database, faultline_close_database

#include <stdio.h> // printf

static char const *module = "Faultline";

FL_APP_MAIN(faultline_app_main) {
    // Install the platform's services into this module's fla_ globals. Order matters:
    // exception first (FL_TRY depends on it), then memory (FL_MALLOC), then logging. The
    // PLAIN, non-fault-injecting memory service goes here. The framework's own
    // allocations must never be fault-injected. The fault-injecting service is pushed
    // into  test-suite DLLs by platform->inject_services(), and is never installed into
    // the core.
    fla_set_exception_service(platform->exception, sizeof *platform->exception);
    fla_set_memory_service(platform->memory, sizeof *platform->memory);
    fla_set_log_service(platform->log, sizeof *platform->log);
    fla_set_timer_service(platform->timer, sizeof *platform->timer);
    fla_set_file_service(platform->file, sizeof *platform->file);

    FLContext fctx         = {0};
    sqlite3  *db           = NULL;
    int volatile exit_code = 0;

    // The fault injector instance is owned by the platform (it backs the fault-injecting
    // memory service); the driver drives that same instance.
    fctx.injector = platform->injector;

    FL_TRY {
        RuntimeCommand *parsed_cmd
            = parse_command_with_globals(get_faultline_commands(),
                                         get_faultline_global_options(), argc, argv);

        // Logging level/destination are platform-owned resources; hand the user's parsed
        // choices back to the platform to apply.
        FLLogLevel log_level
            = parse_log_level(get_string_option(parsed_cmd, "log-level", NULL));
        platform->configure_log(log_level,
                                get_string_option(parsed_cmd, "log-file", NULL));

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

        faultline_initialize(&fctx, NULL, platform->arena);
        fctx.injector
            = platform->injector; // restore after memset in faultline_initialize

        ExecutionContext ectx = {
            .cmd            = *parsed_cmd,
            .db             = db,
            .fctx           = &fctx,
            .log_level      = (int)log_level,
            .arena          = platform->arena,
            .platform       = platform, // lets command_run load/inject suites
            .junit_xml_path = get_string_option(parsed_cmd, "junit-xml", NULL),
        };

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
    return exit_code;
}
