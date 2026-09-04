/**
 * @file flp_module_service.c
 * @author Douglas Cuthbertson
 * @brief Platform side of module loading and suite service injection (Win32).
 * @version 0.1
 * @date 2026-07-11
 *
 * Wraps LoadLibraryA / GetProcAddress / FreeLibrary behind the FLPlatformAPI module
 * primitives, and implements service injection: resolve each fla_set_*_service symbol a
 * loaded suite exports and call the matching flp_init_*_service so the platform's
 * services cross the DLL boundary. This is where the FAULT-INJECTING memory service is
 * wired into suites; it is never installed into the core, so the framework cannot
 * fault-inject its own bookkeeping.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <flp_module_service.h> // FLP_MODULE_SERVICE_INIT_FN, FL_MODULE_*_FN

#include <faultline/fl_exception_service.h> // fla_set_exception_service_fn
#include <faultline/fl_file_service.h>      // fla_set_file_service_fn
#include <faultline/fl_log.h>               // LOG_ERROR (flp backend: FL_PLATFORM_BUILD)
#include <faultline/fl_log_service.h>       // fla_set_log_service_fn
#include <faultline/fl_memory_service.h>    // fla_set_memory_service_fn
#include <faultline/fl_stream_service.h>    // fla_set_stream_service_fn
#include <faultline/fl_timer_service.h>     // fla_set_timer_service_fn
#include <flp_exception_service.h>          // flp_init_exception_service
#include <flp_file_service.h>               // flp_init_file_service
#include <flp_log_service.h>                // flp_init_log_service
#include <flp_memory_service.h> // flp_init_fault_memory_service, FLFaultMemoryContext
#include <flp_stream_service.h> // flp_init_stream_service
#include <flp_timer_service.h>  // flp_init_timer_service

#include <stdbool.h> // false, true
#include <stddef.h>  // NULL
#include <stdint.h>  // uintptr_t

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // LoadLibraryA, GetProcAddress, FreeLibrary, GetLastError

#define FAULTLINE_MODULE_NAME "Faultline Platform"

// The opaque FLModule is the Win32 HMODULE itself: a loaded module's handle is
// never NULL, so a NULL return cleanly signals load failure without a separate
// wrapper allocation.

// The fault-injecting memory context that flp_inject_services() binds into test suites
// that export a memory-service setter.
static FLFaultMemoryContext *g_fault_memory_ctx = NULL;

FLP_MODULE_SERVICE_INIT_FN(flp_module_service_init) {
    g_fault_memory_ctx = fault_ctx;
}

FL_MODULE_LOAD_FN(flp_load_module) {
    static char const module[] = FAULTLINE_MODULE_NAME;

    HMODULE m = LoadLibraryA(path);
    if (m == NULL) {
        // The caller cannot reach GetLastError, so record the OS detail here;
        // the caller decides what the failure means.
        LOG_ERROR(module, "Failed to load \"%s\", error=%lu", path, GetLastError());
    }
    return (FLModule *)m;
}

FL_MODULE_SYMBOL_FN(flp_resolve_symbol) {
    // The uintptr_t intermediate performs the function-to-data pointer conversion that
    // FARPROC requires (C4152 under /W4 otherwise).
    return (void *)(uintptr_t)GetProcAddress((HMODULE)m, symbol);
}

FL_MODULE_UNLOAD_FN(flp_unload_module) {
    if (m != NULL) {
        FreeLibrary((HMODULE)m);
    }
}

FL_INJECT_SERVICES_FN(flp_inject_services) {
    static char const module[] = FAULTLINE_MODULE_NAME;

    if (suite == NULL) {
        LOG_ERROR(module, "no suite module to inject services into");
        return;
    }
    HMODULE m = (HMODULE)suite;

    // Log service (optional)
    fla_set_log_service_fn *fla_set_log
        = (fla_set_log_service_fn *)GetProcAddress(m, FLA_SET_LOG_SERVICE_STR);
    if (fla_set_log != NULL) {
        flp_init_log_service(fla_set_log);
    }

    // Memory service (optional): the FAULT-INJECTING binding, never the plain one.
    fla_set_memory_service_fn *fla_set_mem
        = (fla_set_memory_service_fn *)GetProcAddress(m, FLA_SET_MEMORY_SERVICE_STR);
    if (fla_set_mem != NULL) {
        if (g_fault_memory_ctx != NULL) {
            flp_init_fault_memory_service(fla_set_mem, g_fault_memory_ctx);
        } else {
            // Host programming error: flp_module_service_init() was not called.
            // The suite still runs, just without fault injection.
            LOG_ERROR(module, "no fault-memory context bound; suite runs without fault "
                              "injection");
        }
    }

    // Timer service (optional)
    fla_set_timer_service_fn *fla_set_timer
        = (fla_set_timer_service_fn *)GetProcAddress(m, FLA_SET_TIMER_SERVICE_STR);
    if (fla_set_timer != NULL) {
        flp_init_timer_service(fla_set_timer);
    }

    // File service (optional)
    fla_set_file_service_fn *fla_set_file
        = (fla_set_file_service_fn *)GetProcAddress(m, FLA_SET_FILE_SERVICE_STR);
    if (fla_set_file != NULL) {
        flp_init_file_service(fla_set_file);
    }

    // Stream service (optional)
    fla_set_stream_service_fn *fla_set_stream
        = (fla_set_stream_service_fn *)GetProcAddress(m, FLA_SET_STREAM_SERVICE_STR);
    if (fla_set_stream != NULL) {
        flp_init_stream_service(fla_set_stream);
    }
}
