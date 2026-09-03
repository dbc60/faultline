/**
 * @file fla_exception_service.c
 * @author Douglas Cuthbertson
 * @brief Consumer-side exception service: push/pop/throw over a module-local stack.
 * @version 0.2
 * @date 2026-01-29
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Each module that compiles this file gets its own g_fla_exception_service and its own
 * environment stack, both with static linkage, so a throw inside the module reaches an
 * FL_TRY inside that same module without any help from its host. The stack is
 * thread-local, so two threads in one module unwind independently.
 *
 * A host may replace the service through fla_set_exception_service, which points the
 * module at the host's own push/pop/throw instead.
 *
 * This file is compiled into each test module, typically through a unity build.
 */
#include <faultline/fla_exception_service.h>
#include <faultline/fl_exception_service.h>        // FLExceptionService, FL_*_SERVICE_FN
#include <faultline/fl_exception_service_assert.h> // fl_throw_assertion
#include <faultline/fl_exception_types.h>          // FLExceptionEnvironment, FL_THROWN
#include <faultline/fl_macros.h>                   // FL_THREAD_LOCAL, FL_DECL_SPEC
#include <setjmp.h>                                // longjmp
#include <stddef.h>                                // NULL
#include <stdio.h>                                 // fprintf, fflush, stderr
#include <stdlib.h>                                // abort

static FL_THREAD_LOCAL FLExceptionEnvironment *g_fla_stack;

static FL_PUSH_EXCEPTION_SERVICE_FN(fla_push) {
    env->next   = g_fla_stack;
    g_fla_stack = env;
}

static FL_POP_EXCEPTION_SERVICE_FN(fla_pop) {
    FLExceptionEnvironment *env = g_fla_stack;
    g_fla_stack                 = env->next;
}

#if defined(FL_EXC_BACKEND_CXX)
// This backend keeps no jump buffer and no environment stack: C++ unwinding finds its
// own way back to the matching FL_CATCH, so fla_push and fla_pop are never reached.
// The throw leaves through a C-linkage function pointer, which is why the build carries
// /EHs rather than /EHsc (config.cmd).
static FL_THROW_EXCEPTION_SERVICE_FN(fla_throw) {
    throw FLException(reason, details, file, line);
}
#else
static FL_THROW_EXCEPTION_SERVICE_FN(fla_throw) {
    FLExceptionEnvironment *env = g_fla_stack;

    // The stack is thread-local, so this fires both for a throw with no enclosing FL_TRY
    // and for a throw on a thread whose stack was never established. Report the throw
    // site before dying; a bare null dereference would discard it.
    if (env == NULL) {
        fprintf(stderr, "FL_THROW with no enclosing FL_TRY on this thread: %s (%s:%d)\n",
                reason, file, line);
        if (details != NULL) {
            fprintf(stderr, "  details: %s\n", details);
        }
        fflush(stderr);
        abort();
    }

    g_fla_stack  = env->next;
    env->reason  = reason;
    env->details = details;
    env->file    = file;
    env->line    = line;
    longjmp(env->jmp, FL_THROWN);
}
#endif // FL_EXC_BACKEND_CXX

// FIXME: revisit FL_THREAD_LOCAL and consider either removing it and adding a mutex, or
// ensuring the set function gets called for each new thread.
FLExceptionService g_fla_exception_service = {
    .push_env  = fla_push,
    .pop_env   = fla_pop,
    .throw_exc = fla_throw,
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
