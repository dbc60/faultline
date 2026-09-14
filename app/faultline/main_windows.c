/**
 * @file main_windows.c
 * @author Douglas Cuthbertson
 * @brief Faultline test driver entry point (Windows)
 * @version 0.2
 * @date 2024-12-14
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include "faultline_cli.h" // FLCliSession, faultline_cli_root, faultline_cli_run_once
#include <faultline/flp_fault_memory_context.h> // FLFaultMemoryContext, flp_init_fault_memory_context

#include <faultline/arena.h>             // Arena, new_arena
#include <faultline/fl_context.h>        // FLContext, faultline_initialize
#include <faultline/fault_injector.h>    // fault_injector_create
#include <faultline/fl_log.h>            // LOG_ERROR
#include <faultline/fl_try.h>            // FL_TRY / FL_CATCH / FL_REASON / FL_DETAILS
#include <faultline/fl_memory_service.h> // FLA_SET_MEMORY_SERVICE_FN
#include <flp_log_service.h>    // flp_log_init_custom/set_level/set_output_path/cleanup
#include <flp_memory_service.h> // flp_init_fault_memory_service

#include <stdio.h> // stdout, stderr, fprintf

static char const *module = "Faultline";

// No-op fla_set for the unity build: flp_init_memory_service requires a non-NULL
// setter, but DLL injection is handled per-suite in faultline_cli_commands_run.c. The
// main exe uses FL_PLATFORM_BUILD, so FL_MALLOC routes directly through flp_malloc.
static FLA_SET_MEMORY_SERVICE_FN(noop_fla_set_memory_service) {
    (void)svc;
    (void)size;
}

// Apply --log-level and --log-file to this driver's log.
static void flp_configure_log(FLLogLevel level, char const *path) {
    flp_log_set_level(level);
    if (path != NULL) {
        flp_log_set_output_path(path);
    }
}

int main(int argc, char **argv) {
    Arena               *arena   = new_arena(0, 0);
    FLContext            fctx    = {0};
    FLFaultMemoryContext mem_ctx = {0};
    int volatile exit_code       = 0;

    // static: it is changed inside FL_TRY and read after FL_END_TRY, and a non-volatile
    // automatic object changed between setjmp and longjmp is indeterminate afterwards
    // (C17 7.13.2.1p3). volatile would qualify every member, and &session is passed to
    // functions that take a plain FLCliSession *, and writing a volatile object through a
    // non-volatile pointer is undefined behavior (C17 6.7.3p7).
    static FLCliSession session = {0};

    flp_log_init_custom(LOG_LEVEL_INFO, "faultline.log");
    fctx.injector = fault_injector_create(arena);
    flp_init_fault_memory_context(&mem_ctx, arena, fctx.injector);
    flp_init_fault_memory_service(noop_fla_set_memory_service, &mem_ctx);

    FL_TRY {
        faultline_initialize(&fctx, NULL, arena);

        session.fctx          = &fctx;
        session.arena         = arena;
        session.mem_ctx       = &mem_ctx;
        session.configure_log = flp_configure_log;
        session.io.out        = stdout;
        session.io.err        = stderr;

        // argv[0] is the program name, so the command line starts at argv[1].
        exit_code
            = faultline_cli_run_once(&session, faultline_cli_root(), argc - 1, argv + 1);
    }
    FL_CATCH_ALL {
        char const *details = (FL_DETAILS != NULL) ? FL_DETAILS : "";
        LOG_ERROR(module, "Unhandled exception: %s: %s", FL_REASON, details);
        fprintf(stderr, "Error: %s%s%s\n", FL_REASON, (details[0] != '\0') ? ": " : "",
                details);
        exit_code = 1;
    }
    FL_END_TRY;

    faultline_cli_session_free(&session);
    flp_log_cleanup();
    return exit_code;
}
