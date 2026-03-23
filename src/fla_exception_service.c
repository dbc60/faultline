/**
 * @file fla_exception_service.c
 * @author Douglas Cuthbertson
 * @brief TLS-based exception service accessor implementations.
 * @version 0.1
 * @date 2026-01-29
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This file provides the TLS storage and accessor functions for the exception
 * service. Each DLL that includes this file gets its own copy of the TLS
 * variable (due to static linkage), allowing the driver to inject a service
 * specific to that DLL.
 *
 * This file should be compiled into each test DLL (typically via unity build).
 */
#include <faultline/fla_exception_service.h>
#include <faultline/fl_exception_service_assert.h>  // for fl_throw_assertion
#include <faultline/fl_macros.h>                    // for FL_UNUSED, FL_DECL_SPEC
#include <stdio.h>                        // for fflush, fprintf, stderr
#include <stdlib.h>                       // for abort
#include <faultline/fl_exception_service.h>         // for FLExceptionService, FL_THRO...

static FL_PUSH_EXCEPTION_SERVICE_FN(default_push) {
    FL_UNUSED(env);
    fprintf(stderr, "Exception service is uninitialized - no push function provided\n");
    fflush(stderr);
    abort();
}

static FL_POP_EXCEPTION_SERVICE_FN(default_pop) {
    fprintf(stderr, "Exception service is uninitialized - no pop function provided\n");
    fflush(stderr);
    abort();
}

static FL_THROW_EXCEPTION_SERVICE_FN(default_throw) {
    FL_UNUSED(reason);
    FL_UNUSED(details);
    FL_UNUSED(file);
    FL_UNUSED(line);
    fprintf(stderr, "Exception service is uninitialized - no throw function provided\n");
    fflush(stderr);
    abort();
}

// FIXME: revisit FL_THREAD_LOCAL and consider either removing it and adding a mutex, or
// ensuring the set function gets called for each new thread.
FLExceptionService g_fla_exception_service = {
    .push_env  = default_push,
    .pop_env   = default_pop,
    .throw_exc = default_throw,
};

FL_DECL_SPEC FLA_SET_EXCEPTION_SERVICE_FN(fla_set_exception_service) {
    if (size < sizeof(FLExceptionService)) {
        fprintf(stderr, "invalid exception service - expected %zu bytes, received %zu\n",
                sizeof(FLExceptionService), size);
        fflush(stderr);
        abort();
    }
    g_fla_exception_service = *svc;
}

// (char const *expression, char const *details, char const *file, int line)
FL_THROW_EXCEPTION_SERVICE_FN(fl_throw_assertion) {
    g_fla_exception_service.throw_exc(reason, details, file, line);
}
