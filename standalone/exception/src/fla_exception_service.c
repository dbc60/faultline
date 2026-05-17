/**
 * @file fla_exception_service.c
 * @author Douglas Cuthbertson
 * @brief Self-contained TLS-based exception service for standalone library use.
 * @version 0.2
 * @date 2026-05-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Provides working push/pop/throw implementations backed by a per-thread
 * linked-list stack of exception environments. No driver initialization is
 * required; the library is fully functional as a standalone static library.
 *
 * A driver may still call fla_set_exception_service() to replace the default
 * implementation (e.g., to share a single TLS stack across multiple loaded
 * DLLs).
 */
#include <faultline/fla_exception_service.h>
#include <faultline/fl_exception_service_assert.h> // for fl_throw_assertion
#include <faultline/fl_exception_service.h>        // for FLExceptionService
#include <faultline/fl_exception_types.h> // for FLExceptionEnvironment, FL_THROWN
#include <faultline/fl_macros.h>          // for FL_THREAD_LOCAL, FL_DECL_SPEC
#include <setjmp.h>                       // for longjmp
#include <stdio.h>                        // for fflush, fprintf, stderr
#include <stdlib.h>                       // for abort

static FL_THREAD_LOCAL FLExceptionEnvironment *g_stack;

static FL_PUSH_EXCEPTION_SERVICE_FN(standalone_push) {
    env->next = g_stack;
    g_stack   = env;
}

static FL_POP_EXCEPTION_SERVICE_FN(standalone_pop) {
    FLExceptionEnvironment *env = g_stack;
    g_stack                     = env->next;
}

static FL_THROW_EXCEPTION_SERVICE_FN(standalone_throw) {
    FLExceptionEnvironment *env = g_stack;
    if (env == NULL) {
        fprintf(stderr, "Unhandled exception: %s", reason ? reason : "(unknown)");
        if (details) {
            fprintf(stderr, " - %s", details);
        }
        if (file) {
            fprintf(stderr, " at %s:%d", file, line);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
        abort();
    }
    g_stack      = env->next;
    env->reason  = reason;
    env->details = details;
    env->file    = file;
    env->line    = line;
    longjmp(env->jmp, FL_THROWN);
}

FLExceptionService g_fla_exception_service = {
    .push_env  = standalone_push,
    .pop_env   = standalone_pop,
    .throw_exc = standalone_throw,
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

FL_THROW_EXCEPTION_SERVICE_FN(fl_throw_assertion) {
    g_fla_exception_service.throw_exc(reason, details, file, line);
}
