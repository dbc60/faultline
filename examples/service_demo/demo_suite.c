/**
 * @file demo_suite.c
 * @author Douglas Cuthbertson
 * @brief Application/DLL side of the platform/application service-split demo.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This translation unit is compiled into demo_suite.dll WITHOUT
 * FL_BUILD_DRIVER, so:
 *   - fl_log.h selects the application-side LOG_* macros (route through the
 *     injected g_fla_log_service),
 *   - fl_try.h selects the application-side FL_TRY/FL_CATCH/FL_THROW macros
 *     (route through the injected g_fla_exception_service),
 *   - fla_memory_service.h redefines malloc/free/etc. to call the injected
 *     g_fla_memory_service.
 *
 * The driver injects all three services after LoadLibrary() via the exported
 * fla_set_* entry points (declared FL_DECL_SPEC -> dllexport under DLL_BUILD),
 * then calls the demo_* workers below. None of the allocator, logger, or
 * exception machinery is compiled into this DLL — it is all borrowed from the
 * driver at run time. That is the whole point of the demo.
 */

/* CRT headers MUST precede fla_memory_service.h: that header redefines malloc,
 * calloc, free, realloc, and aligned_alloc as macros, and the CRT declarations
 * must be processed before the macros shadow them (see fla_memory_service.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <faultline/fl_macros.h>            // FL_SPEC_EXPORT
#include <faultline/fl_exception_service.h> // fl_invalid_value (shared reason)
#include <faultline/fl_try.h>               // FL_TRY/FL_CATCH/FL_THROW (fla_ side)
#include <faultline/fl_log.h>               // LOG_INFO/LOG_WARN/LOG_ERROR (fla_ side)
#include <faultline/fla_memory_service.h>   // malloc/free -> injected memory service

static char const *module = "demo-suite";

// A reason owned by this module. Because it is thrown and caught within the
// same translation unit, FL_CATCH (pointer identity) matches it.
static FLExceptionReason demo_reason = "demo: simulated recoverable error";

/**
 * @brief Happy-path worker: allocate through the memory service, log via the
 * log service, and throw/catch a module-local exception. All caught locally;
 * returns normally.
 */
FL_SPEC_EXPORT int demo_run(void) {
    LOG_INFO(module, "demo_run: start (executing inside the DLL)");

    // malloc() expands to g_fla_memory_service.fl_malloc(...), which the driver
    // wired to its arena-backed allocator. The bytes come from the driver.
    size_t const cap = 64;
    char        *buf = malloc(cap);
    if (buf == NULL) {
        LOG_ERROR(module, "demo_run: allocation failed");
        return 1;
    }
    snprintf(buf, cap, "%s", "allocated via the injected memory service");
    LOG_INFO(module, "demo_run: buffer contains \"%s\"", buf);

    free(buf); // routes through the driver's flp_free
    LOG_INFO(module, "demo_run: freed the buffer");

    // Throw and catch within this module: FL_CATCH matches by pointer identity.
    FL_TRY {
        LOG_INFO(module, "demo_run: throwing a module-local exception");
        FL_THROW(demo_reason);
    }
    FL_CATCH(demo_reason) {
        LOG_WARN(module, "demo_run: caught locally: %s", FL_REASON);
    }
    FL_END_TRY;

    LOG_INFO(module, "demo_run: done");
    return 0;
}

/**
 * @brief Throws a shared reason and does NOT catch it, so the exception
 * propagates across the DLL boundary into the driver's top-level handler
 * (longjmp into the driver's setjmp frame).
 */
FL_SPEC_EXPORT void demo_throw_uncaught(void) {
    LOG_INFO(module,
             "demo_throw_uncaught: throwing fl_invalid_value, NOT catching it");
    FL_THROW(fl_invalid_value);
}
