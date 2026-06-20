/**
 * @file win32_faultline_main.c
 * @author Douglas Cuthbertson
 * @brief Thin Win32 platform host for the platform/app split.
 * @version 0.1
 * @date 2026-06-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * The platform layer's entry point. It owns the OS relationship: it creates the
 * arena, the fault injector, and the two memory bindings (plain + injecting),
 * builds the service vtables, assembles an FLPlatformAPI, and hands control to
 * faultline_app_main() in faultline_core.lib. All OS coupling stops here.
 *
 * PLAIN vs INJECTING split (the part not to overlook):
 *   - flp_memory_service.c (plain)        -> bound to `arena`, no injection.
 *     Exposed as platform.memory; the core installs it for its OWN allocations.
 *   - flp_fault_memory_service.c (inject) -> bound to `arena` + `injector`.
 *     Pushed into suite DLLs by flp_inject_services(); NEVER installed into the
 *     core, so the framework can't fault-inject its own bookkeeping.
 * These live in separate TUs with separate static contexts, so binding one does
 * not disturb the other.
 *
 * PREREQUISITES (new platform files referenced here):
 *   - flp_timer_service.c  : flp_timer_now / flp_timer_elapsed_seconds
 *   - flp_file_service.c   : flp_file_open / read / write / close
 *   - flp_module_service.c : flp_load_module / flp_resolve_symbol /
 *                            flp_unload_module / flp_inject_services /
 *                            flp_module_service_init
 *   - include/platform_api.h : FLPlatformAPI, faultline_app_main
 *   - flp_configure_log (small wrapper, defined below)
 */
#include <platform_api.h> // FLPlatformAPI, faultline_app_main

#include <faultline/arena.h>          // Arena, new_arena
#include <faultline/fault_injector.h> // FaultInjector, fault_injector_create
#include <faultline/fl_context.h>     // (FaultInjector forward use)

// Platform service interfaces + implementations (defined earlier in the unity TU)
#include <faultline/fl_memory_service.h>        // FLMemoryService
#include <faultline/fl_exception_service.h>     // FLExceptionService
#include <faultline/fl_log_service.h>           // FLLogService, FLLogLevel
#include <faultline/fl_timer_service.h>         // FLTimerService
#include <faultline/fl_file_service.h>          // FLFileService
#include <faultline/flp_fault_memory_context.h> // FLFaultMemoryContext
#include <faultline/flp_memory_context.h>       // FLMemoryContext
#include <flp_memory_service.h>    // flp_init_memory_service, flp_malloc, ...
#include <flp_log_service.h>       // flp_log_init_custom, flp_write_log, flp_log_set_*
#include <flp_exception_service.h> // flp_push/pop/throw declarations
#include <faultline/fl_try.h>      // FL_TRY/FL_CATCH_ALL (selects flp_ backstop)
#include <faultline/fl_log.h>      // LOG_* (selects flp_ backend via FL_PLATFORM_BUILD)

#include <stdio.h> // NULL

static char const *module = "Faultline";

// No-op setter: flp_init_*_service() wants a non-NULL setter, but at host setup
// we only want the side effect of binding the static context; nothing to inject
// into yet (suites get injected later via flp_inject_services).
static FLA_SET_MEMORY_SERVICE_FN(noop_fla_set_memory_service) {
    (void)svc;
    (void)size;
}

// Logging level/destination are platform-owned; the core asks us to apply the
// user's parsed choices through this callback (FLPlatformAPI.configure_log).
static void flp_configure_log(FLLogLevel level, char const *path) {
    flp_log_set_level(level);
    if (path != NULL) {
        flp_log_set_output_path(path);
    }
}

int main(int argc, char **argv) {
    Arena *arena = new_arena(0, 0);

    flp_log_init_custom(LOG_LEVEL_INFO, "faultline.log");

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
    FLExceptionService exception = {
        .push_env  = flp_push,
        .pop_env   = flp_pop,
        .throw_exc = flp_throw,
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

    FLPlatformAPI platform = {
        .memory    = &plain_memory,
        .log       = &log,
        .exception = &exception,
        .timer     = &timer,
        .file      = &file,
        .arena     = arena,
        .injector  = injector,

        .load_module     = flp_load_module,
        .resolve_symbol  = flp_resolve_symbol,
        .unload_module   = flp_unload_module,
        .inject_services = flp_inject_services,
        .configure_log   = flp_configure_log,

        .platform = &fault_ctx,
    };

    // -- Top-level backstop ----------------------------------------------------
    // flp_throw requires a top-level environment to pop (see flp_exception_
    // service.h). This flp_ FL_TRY establishes it; the core's fla_ FL_TRY nests
    // on the same platform stack (after install, g_fla_*.push_env == flp_push),
    // so an uncaught throw from the core or a suite rethrows up to here.
    int exit_code = 0;
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
