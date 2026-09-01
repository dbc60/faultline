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
#include <faultline/fl_exception_service_assert.h> // for fl_throw_assertion
#include <faultline/fl_macros.h>                   // for FL_UNUSED, FL_DECL_SPEC
#include <stddef.h>                                // for NULL
#include <stdio.h>                                 // for fflush, fprintf, stderr
#include <stdlib.h>                                // for abort
#include <faultline/fl_exception_service.h>        // for FLExceptionService, FL_THRO...

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

#if defined(FL_EXC_BACKEND_CXX)
// Under the setjmp backend this is effectively unreachable: FL_TRY calls FL_EXC_PUSH
// before anything can throw, and an uninjected service means push_env is still
// default_push, which aborts first. There is no such ordering here. The C++ backend's
// FL_TRY does not call FL_EXC_PUSH at all. so an uninjected throw_exc really is called,
// and a throw here reaches whatever FL_TRY encloses the call. That changes the failure
// mode: a suite whose own FL_CATCH_ALL happens to enclose the call now reports an
// uninjected service as an ordinary caught exception rather than a hard abort. The
// stderr line below is the only diagnostic guaranteed to survive that.
static FL_THROW_EXCEPTION_SERVICE_FN(default_throw) {
    fprintf(stderr, "Exception service is uninitialized - no throw function provided\n");
    fflush(stderr);
    throw FLException(reason, details, file, line);
}
#else
static FL_THROW_EXCEPTION_SERVICE_FN(default_throw) {
    FL_UNUSED(reason);
    FL_UNUSED(details);
    FL_UNUSED(file);
    FL_UNUSED(line);
    fprintf(stderr, "Exception service is uninitialized - no throw function provided\n");
    fflush(stderr);
    abort();
}
#endif // FL_EXC_BACKEND_CXX

// FIXME: revisit FL_THREAD_LOCAL and consider either removing it and adding a mutex, or
// ensuring the set function gets called for each new thread.
FLExceptionService g_fla_exception_service = {
    .push_env  = default_push,
    .pop_env   = default_pop,
    .throw_exc = default_throw,
};

FL_DECL_SPEC FLA_SET_EXCEPTION_SERVICE_FN(fla_set_exception_service) {
    if (svc == NULL) {
        fprintf(stderr, "invalid exception service - NULL service address\n");
        fflush(stderr);
        abort();
    }
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
