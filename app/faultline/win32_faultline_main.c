/**
 * @file win32_faultline_main.c
 * @author Douglas Cuthbertson
 * @brief Thin Win32 platform host for the platform/app split.
 * @version 0.1
 * @date 2026-06-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * The platform layer's entry point. It owns the OS relationship: it creates the arena,
 * the fault injector, and the two memory bindings (plain + injecting), builds the
 * service vtables, assembles an FLPlatformAPI, and hands control to faultline_app_main()
 * in faultline_core.lib. All OS coupling stops here.
 *
 * PLAIN vs INJECTING split:
 *   - flp_memory_service.c (plain)        -> bound to `arena`, no injection.
 *     Exposed as platform.memory; the core installs it for its own allocations.
 *   - flp_fault_memory_service.c (inject) -> bound to `arena` + `injector`.
 *     Pushed into suite DLLs by flp_inject_services(); it is never installed into the
 *     core, so the framework can't fault-inject its own bookkeeping.
 * These live in separate TUs with separate static contexts, so binding one does not
 * disturb the other.
 *
 * Platform services assembled here and where they come from:
 *   - flp_timer_service.c  : flp_timer_now / flp_timer_elapsed_seconds
 *   - flp_file_service.c   : flp_file_open / read / write / close
 *   - flp_stream_service.c : flp_stream_open / write / close / console
 *   - flp_module_service.c : flp_load_module / flp_resolve_symbol /
 *                            flp_unload_module / flp_inject_services /
 *                            flp_module_service_init
 *   - flp_configure_log (small wrapper, defined below)
 */
#include <platform_api.h> // FLPlatformAPI, faultline_app_main

#include <faultline/arena.h>          // Arena, new_arena
#include <faultline/fault_injector.h> // FaultInjector, fault_injector_create
#include <faultline/fl_context.h>     // (FaultInjector forward use)

// Platform service interfaces + implementations (defined earlier in the unity TU)
#include <faultline/fl_file_service.h>   // FLFileService
#include <faultline/fl_log_service.h>    // FLLogService, FLLogLevel
#include <faultline/fl_stream_service.h> // FLStreamService

// The core lib's embedded consumer accessors (built with FL_EMBEDDED, so plain
// functions): the host installs log + exception into them before the first core
// call. This TU must also compile with /DFL_EMBEDDED so these declarations match.
#include <faultline/fla_log_service.h>          // fla_set_log_service
#include <faultline/fl_macros.h>                // FL_UNUSED
#include <faultline/fl_memory_service.h>        // FLMemoryService
#include <faultline/fl_timer_service.h>         // FLTimerService
#include <faultline/flp_fault_memory_context.h> // FLFaultMemoryContext
#include <faultline/flp_memory_context.h>       // FLMemoryContext
#include <flp_log_service.h>    // flp_log_init_custom, flp_write_log, flp_log_set_*
#include <flp_memory_service.h> // flp_init_memory_service, flp_malloc, ...
#include <flp_timer_service.h>  // flp_timer_now, flp_timer_elapsed_seconds
#include <flp_file_service.h>   // flp_file_open, flp_file_read, flp_file_write, ...
#include <flp_stream_service.h> // flp_stream_open, flp_stream_write, flp_stream_close, ...
#include <flp_module_service.h> // flp_load_module, flp_inject_services, ...
#include <faultline/fl_try.h>   // FL_TRY/FL_CATCH_ALL backstop
#include <faultline/fl_log.h>   // LOG_* (selects flp_ backend via FL_PLATFORM_BUILD)

#include <stdio.h> // NULL

static char const *module = "Faultline";

// No-op setter: flp_init_*_service() wants a non-NULL setter, but at host setup we only
// want the side effect of binding the static context; nothing to inject into yet (suites
// get injected later via flp_inject_services).
static FLA_SET_MEMORY_SERVICE_FN(noop_fla_set_memory_service) {
    FL_UNUSED(svc);
    FL_UNUSED(size);
}

// Logging level/destination are platform-owned; the core asks us to apply the user's
// parsed choices through this callback (FLPlatformAPI.configure_log).
static void flp_configure_log(FLLogLevel level, char const *path) {
    flp_log_set_level(level);
    if (path != NULL) {
        flp_log_set_output_path(path);
    }
}

int main(int argc, char **argv) {
    // new_arena below is core code, and core code logs through the core's own
    // g_fla_log_service. Install one before that call; faultline_app_main installs the
    // rest of the services.
    flp_log_init_custom(LOG_LEVEL_INFO, "faultline.log");
    flp_init_log_service(fla_set_log_service);

    Arena *arena = new_arena(0, 0);

    // -- Fault-injecting memory (for suites) ----------------------------------
    FaultInjector       *injector  = fault_injector_create(arena);
    FLFaultMemoryContext fault_ctx = {0};
    flp_init_fault_memory_context(&fault_ctx, arena, injector);
    flp_init_fault_memory_service(noop_fla_set_memory_service, &fault_ctx);
    flp_module_service_init(&fault_ctx); // so flp_inject_services() can wire suites

    // -- Plain memory (for the framework itself; separate static ctx) ----------
    FLMemoryContext plain_ctx = {0};
    flp_init_memory_context(&plain_ctx, arena);
    flp_init_memory_service(noop_fla_set_memory_service, &plain_ctx);

    // -- Service vtables exposed to the application layer ---------------------
    FLMemoryService plain_memory = {
        .ctx              = &plain_ctx,
        .fl_aligned_alloc = flp_aligned_alloc,
        .fl_calloc        = flp_calloc,
        .fl_free          = flp_free,
        .fl_malloc        = flp_malloc,
        .fl_realloc       = flp_realloc,
    };
    FLLogService   log   = {.write = flp_write_log};
    FLTimerService timer = {
        .now             = flp_timer_now,
        .elapsed_seconds = flp_timer_elapsed_seconds,
    };
    FLFileService file = {
        .open  = flp_file_open,
        .read  = flp_file_read,
        .write = flp_file_write,
        .close = flp_file_close,
    };
    FLStreamService stream = {
        .open    = flp_stream_open,
        .write   = flp_stream_write,
        .close   = flp_stream_close,
        .console = flp_stream_console,
    };

    FLPlatformAPI platform = {
        .memory   = &plain_memory,
        .log      = &log,
        .timer    = &timer,
        .file     = &file,
        .stream   = &stream,
        .arena    = arena,
        .injector = injector,

        .load_module     = flp_load_module,
        .resolve_symbol  = flp_resolve_symbol,
        .unload_module   = flp_unload_module,
        .inject_services = flp_inject_services,
        .configure_log   = flp_configure_log,

        .platform = &fault_ctx,
    };

    // -- Top-level backstop ----------------------------------------------------
    // This try catch block will handle any throw from the flp_ services.
    int volatile exit_code = 0;
    FL_TRY {
        exit_code = faultline_app_main(&platform, argc, argv);
    }
    FL_CATCH_ALL {
        LOG_ERROR(module, "Fatal: uncaught exception reached the platform host: %s",
                  FL_REASON);
        exit_code = 1;
    }
    FL_END_TRY;

    flp_log_cleanup();
    return exit_code;
}
