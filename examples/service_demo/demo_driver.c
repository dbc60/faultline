/**
 * @file demo_driver.c
 * @author Douglas Cuthbertson
 * @brief Platform/driver side of the platform/application service-split demo.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Compiled WITH FL_PLATFORM_BUILD (so fl_try.h/fl_log.h select the self-contained
 * platform service macros) and FL_EMBEDDED. This binary owns the arena and the
 * platform implementations of all three services. It loads demo_suite.dll,
 * injects each service via the DLL's exported fla_set_* entry points then calls
 * the DLL's worker exports.
 *
 * Build inputs (from the unified imported tree): the flp_/fl_ sources of the
 * memory_service, log, and exception (exception_service) packages. The fla_
 * sources are deliberately NOT compiled here. They belong to the DLL, and
 * fla_/flp_ exception sources cannot coexist in one binary.
 */
#include <faultline/arena.h>              // Arena, new_arena
#include <faultline/flp_memory_context.h> // FLMemoryContext, flp_init_memory_context
#include <flp_memory_service.h>           // flp_init_memory_service
#include <flp_log_service.h> // flp_log_init / set_level / cleanup, flp_init_log_service
#include <faultline/fl_exception_service.h> // fla_set_exception_service_fn, FLA_SET_EXCEPTION_SERVICE_STR, fl_invalid_value
#include <faultline/fl_log_service.h> // fla_set_log_service_fn, FLA_SET_LOG_SERVICE_STR
#include <faultline/fl_memory_service.h> // fla_set_memory_service_fn, FLA_SET_MEMORY_SERVICE_STR
#include <faultline/fl_try.h> // FL_TRY/FL_CATCH (flp_ side), flp_init_exception_service
#include <faultline/fl_log.h> // LOG_INFO/LOG_ERROR (flp_ side)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h> // LoadLibraryA, GetProcAddress, FreeLibrary, HMODULE

static char const *module = "demo-driver";

// Worker exports we expect from the suite DLL.
typedef int (*demo_run_fn)(void);
typedef void (*demo_throw_uncaught_fn)(void);

int main(int argc, char **argv) {
    char const *dll_path   = (argc > 1) ? argv[1] : "demo_suite.dll";
    int volatile exit_code = 0;

    flp_log_init(); // stdout, LOG_INFO

    Arena          *arena   = new_arena(0, 0);
    FLMemoryContext mem_ctx = {0};
    flp_init_memory_context(&mem_ctx, arena);

    HMODULE suite = LoadLibraryA(dll_path);
    if (suite == NULL) {
        LOG_ERROR(module, "Failed to load \"%s\", error=%lu", dll_path, GetLastError());
        flp_log_cleanup();
        return 1;
    }

    // -- Inject the three services into the DLL (order mirrors command_run.c) --

    // Log service (optional): wire the DLL's g_fla_log_service to flp_write_log.
    fla_set_log_service_fn *fla_set_log
        = (fla_set_log_service_fn *)GetProcAddress(suite, FLA_SET_LOG_SERVICE_STR);
    if (fla_set_log != NULL) {
        flp_init_log_service(fla_set_log);
        LOG_INFO(module, "log service injected");
    }

    // Exception service (required): without it the DLL's throws would abort.
    fla_set_exception_service_fn *fla_set_exc
        = (fla_set_exception_service_fn *)GetProcAddress(suite,
                                                         FLA_SET_EXCEPTION_SERVICE_STR);
    if (fla_set_exc == NULL) {
        LOG_ERROR(module, "\"%s\" has no exception service", dll_path);
        FreeLibrary(suite);
        flp_log_cleanup();
        return 1;
    }
    flp_init_exception_service(fla_set_exc);
    LOG_INFO(module, "exception service injected");

    // Memory service (optional): point the DLL's malloc/free at our arena.
    fla_set_memory_service_fn *fla_set_mem
        = (fla_set_memory_service_fn *)GetProcAddress(suite, FLA_SET_MEMORY_SERVICE_STR);
    if (fla_set_mem != NULL) {
        flp_init_memory_service(fla_set_mem, &mem_ctx);
        LOG_INFO(module, "memory service injected");
    }

    demo_run_fn            demo_run = (demo_run_fn)GetProcAddress(suite, "demo_run");
    demo_throw_uncaught_fn demo_throw_uncaught
        = (demo_throw_uncaught_fn)GetProcAddress(suite, "demo_throw_uncaught");

    // flp_throw requires a live try frame to pop, so the driver wraps every DLL
    // call that may throw in a top-level try/catch.
    FL_TRY {
        if (demo_run != NULL) {
            LOG_INFO(module, "calling demo_run() in the DLL");
            int rc = demo_run();
            LOG_INFO(module, "demo_run() returned %d", rc);
        }

        if (demo_throw_uncaught != NULL) {
            LOG_INFO(module, "calling demo_throw_uncaught() in the DLL");
            demo_throw_uncaught();
            LOG_ERROR(module, "demo_throw_uncaught() returned without throwing");
        }
    }
    // The reason was thrown in the DLL, where fl_invalid_value lives at a
    // different address than the driver's copy (each module links its own
    // fl_exception_service.c). Match by string, not pointer. See FL_CATCH_STR.
    FL_CATCH_STR(fl_invalid_value) {
        LOG_INFO(module, "driver caught exception propagated from the DLL: %s",
                 FL_REASON);
    }
    FL_CATCH_ALL {
        LOG_ERROR(module, "driver caught unexpected exception: %s", FL_REASON);
        exit_code = 1;
    }
    FL_END_TRY;

    FreeLibrary(suite);
    flp_log_cleanup();
    return exit_code;
}
