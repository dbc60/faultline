#ifndef FL_PLATFORM_API_H_
#define FL_PLATFORM_API_H_

/**
 * @file platform_api.h
 * @author Douglas Cuthbertson
 * @brief The contract between the Faultline platform layer (win32_faultline.exe)
 * and the application layer (faultline_core). The platform fills in an
 * FLPlatformAPI and hands it to faultline_app_main(); the application layer
 * reaches every OS capability — memory, exceptions, logging, timing, files, and
 * module loading — only through this struct. The application TU must never
 * include <Windows.h>; this header is its sole window onto the platform.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_memory_service.h>    // FLMemoryService
#include <faultline/fl_exception_service.h> // FLExceptionService
#include <faultline/fl_log_service.h>       // FLLogService, FLLogLevel
#include <faultline/fl_timer_service.h>     // FLTimerService
#include <faultline/fl_file_service.h>      // FLFileService, FLFile (opaque)
#include <faultline/arena.h>                // Arena
#include <faultline/fault_injector.h>       // FaultInjector
#include <faultline/fl_macros.h>            // FL_STR
#include <stdbool.h>
#include <stddef.h> // size_t

#if defined(__cplusplus)
extern "C" {
#endif

// Opaque module handle. (FLFile is defined by fl_file_service.h.) The
// application never sees an HMODULE; it holds this token and passes it back.
typedef struct FLModule FLModule;

// -- Module loading (wraps LoadLibraryA / GetProcAddress / FreeLibrary) --------
// Primitives, not policy: the application layer decides which suites to load, in
// what order, and what to do on failure. Only the mechanism lives in the platform.
#define FL_MODULE_LOAD_FN(name)   FLModule *name(char const *path)
#define FL_MODULE_SYMBOL_FN(name) void *name(FLModule *m, char const *symbol)
#define FL_MODULE_UNLOAD_FN(name) void name(FLModule *m)
typedef FL_MODULE_LOAD_FN(fl_module_load_fn);
typedef FL_MODULE_SYMBOL_FN(fl_module_symbol_fn);
typedef FL_MODULE_UNLOAD_FN(fl_module_unload_fn);

// -- Service injection into a loaded suite -------------------------------------
// Encapsulates the GetProcAddress("fla_set_*") + flp_init_*_service() dance that
// command_run.c does today. CRUCIALLY, this is where the FAULT-INJECTING memory
// service is wired in — it is injected only into suites, never exposed on this
// struct, so the framework can't fault-inject its own bookkeeping. The platform
// owns the flp_* impls and the fault-memory context, so injection stays
// entirely platform-side. Returns false if the suite is missing a *required*
// service (exception handling).
#define FL_INJECT_SERVICES_FN(name) bool name(FLModule *suite)
typedef FL_INJECT_SERVICES_FN(fl_inject_services_fn);

// -- Logging configuration -----------------------------------------------------
// The log sink (level + destination file) is a platform-owned resource. The
// application parses the user's --log-level/--log-file and hands them back here.
#define FL_CONFIGURE_LOG_FN(name) void name(FLLogLevel level, char const *path)
typedef FL_CONFIGURE_LOG_FN(fl_configure_log_fn);

// -- The platform API handed to the application layer --------------------------
typedef struct FLPlatformAPI {
    // Services installed into the core's own fla_ globals at faultline_app_main
    // entry. `memory` is the PLAIN, non-fault-injecting arena service: the
    // framework's own allocations must never be injected. (The fault-injecting
    // counterpart is reachable only via inject_services, above.)
    FLMemoryService    *memory;
    FLLogService       *log;
    FLExceptionService *exception;
    FLTimerService     *timer;
    FLFileService      *file;

    // Shared infrastructure the driver uses directly (both core-layer types):
    //   arena    — OS-backed application arena, owned by the platform.
    //   injector — the fault-injector instance that backs the fault-memory
    //              service; the driver drives this same instance
    //              (fctx->injector = platform->injector).
    Arena         *arena;
    FaultInjector *injector;

    // OS module loading + suite service injection.
    fl_module_load_fn     *load_module;
    fl_module_symbol_fn   *resolve_symbol;
    fl_module_unload_fn   *unload_module;
    fl_inject_services_fn *inject_services;

    // Platform-owned resource configuration.
    fl_configure_log_fn *configure_log;

    // Opaque pointer to the platform's own state (currently the
    // FLFaultMemoryContext). The application treats it as a token and never
    // dereferences it.
    void *platform;
} FLPlatformAPI;

// -- Application entry point (the only symbol the platform calls into) ----------
#define FL_APP_MAIN(name) int name(FLPlatformAPI const *platform, int argc, char **argv)
typedef FL_APP_MAIN(fl_app_main_fn);
FL_APP_MAIN(faultline_app_main);
#define FL_APP_MAIN_STR FL_STR(faultline_app_main)

#if defined(__cplusplus)
}
#endif

#endif // FL_PLATFORM_API_H_
