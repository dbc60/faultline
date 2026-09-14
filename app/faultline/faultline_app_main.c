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
#include "faultline_cli.h" // FLCliSession, faultline_cli_root, faultline_cli_run_once
#include <platform_api.h>  // FLPlatformAPI, FL_APP_MAIN

// Application-side service accessors: installing into these globals is what makes
// FL_TRY / FL_MALLOC / LOG_* in the core route through the platform.
#include <faultline/fla_memory_service.h> // fla_set_memory_service
#include <faultline/fla_log_service.h>    // fla_set_log_service
#include <faultline/fla_timer_service.h>  // fla_set_timer_service
#include <faultline/fla_file_service.h>   // fla_set_file_service
#include <faultline/fla_stream_service.h> // fla_set_stream_service
#include <faultline/fl_try.h>             // FL_TRY/FL_CATCH (selects fla_ backend)

#include <faultline/fl_context.h> // FLContext, faultline_initialize
#include <faultline/fl_log.h>     // LOG_ERROR

#include <stdio.h> // stdout, stderr, fprintf

static char const *module_name = "Faultline";

FL_APP_MAIN(faultline_app_main) {
    // Install the platform's services into this module's fla_ globals. Order matters:
    // memory first (FL_MALLOC), then logging. The non-fault-injecting memory service
    // goes here. The framework's own allocations must never be fault-injected. The
    // fault-injecting service is pushed into test-suite DLLs by
    // platform->inject_services(). There is no exception service to install: each
    // image compiles its own.
    fla_set_memory_service(platform->memory, sizeof *platform->memory);
    fla_set_log_service(platform->log, sizeof *platform->log);
    fla_set_timer_service(platform->timer, sizeof *platform->timer);
    fla_set_file_service(platform->file, sizeof *platform->file);
    fla_set_stream_service(platform->stream, sizeof *platform->stream);

    FLContext fctx         = {0};
    int volatile exit_code = 0;

    // static: it is changed inside FL_TRY and read after FL_END_TRY, and a non-volatile
    // automatic object changed between setjmp and longjmp is indeterminate afterwards
    // (C17 7.13.2.1p3). volatile would qualify every member, and &session is passed to
    // functions that take a plain FLCliSession *, and writing a volatile object through a
    // non-volatile pointer is undefined behavior (C17 6.7.3p7).
    static FLCliSession session = {0};

    // The fault injector instance is owned by the platform (it backs the fault-injecting
    // memory service); the driver drives that same instance.
    fctx.injector = platform->injector;

    FL_TRY {
        faultline_initialize(&fctx, NULL, platform->arena);
        fctx.injector
            = platform->injector; // restore after memset in faultline_initialize

        session.fctx          = &fctx;
        session.arena         = platform->arena;
        session.platform      = platform; // lets the run command load and inject suites
        session.configure_log = platform->configure_log;
        session.io.out        = stdout;
        session.io.err        = stderr;

        // argv[0] is the program name, so the command line starts at argv[1].
        exit_code
            = faultline_cli_run_once(&session, faultline_cli_root(), argc - 1, argv + 1);
    }
    FL_CATCH_ALL {
        char const *details = (FL_DETAILS != NULL) ? FL_DETAILS : "";
        LOG_ERROR(module_name, "Unhandled exception: %s: %s", FL_REASON, details);
        fprintf(stderr, "Error: %s%s%s\n", FL_REASON, (details[0] != '\0') ? ": " : "",
                details);
        exit_code = 1;
    }
    FL_END_TRY;

    faultline_cli_session_free(&session);
    return exit_code;
}
