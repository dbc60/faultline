/**
 * @file fl_sa_exception_service.c
 * @author Douglas Cuthbertson
 * @brief Standalone arena exception service — direct TLS setjmp/longjmp implementation.
 *
 * Provides fl_sa_push, fl_sa_pop, and fl_sa_throw, the three hooks used by the
 * FL_EXC_PUSH/POP/THROW macros in the standalone override headers.  No service
 * struct, no injection, no DLL boundary.
 *
 * fl_sa_throw aborts (via the unhandled path below) if the TLS stack is empty,
 * which means an exception escaped every FL_TRY block — a programmer error.
 * @version 0.1
 * @date 2026-05-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_exception_service.h> // FL_PUSH/POP/THROW_EXCEPTION_SERVICE_FN
#include <faultline/fl_exception_types.h>   // FLExceptionEnvironment, FLExceptionState
#include <faultline/fl_macros.h>            // FL_THREAD_LOCAL
#include <setjmp.h>                         // longjmp
#include <stdio.h>                          // fprintf
#include <stdlib.h>                         // abort

static FL_THREAD_LOCAL FLExceptionEnvironment *g_stack;

FL_PUSH_EXCEPTION_SERVICE_FN(fl_sa_push) {
    env->next = g_stack;
    g_stack   = env;
}

FL_POP_EXCEPTION_SERVICE_FN(fl_sa_pop) {
    FLExceptionEnvironment *env = g_stack;
    g_stack                     = env->next;
}

FL_THROW_EXCEPTION_SERVICE_FN(fl_sa_throw) {
    if (g_stack == NULL) {
        fprintf(stderr, "fl_sa_throw: unhandled exception \"%s\" at %s:%d\n",
                reason ? reason : "(null)", file ? file : "?", line);
        abort();
    }
    FLExceptionEnvironment *env = g_stack;
    g_stack                     = env->next;
    env->reason                 = reason;
    env->details                = details;
    env->file                   = file;
    env->line                   = (uint32_t)line;
    longjmp(env->jmp, FL_THROWN);
}
