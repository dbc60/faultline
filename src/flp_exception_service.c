/**
 * @file flp_exception_service.c
 * @author Douglas Cuthbertson
 * @brief Platform-side exception service: push/pop/throw via TLS exception stack.
 * @version 0.1
 * @date 2026-02-02
 *
 * This file provides the default implementations for the FLExceptionService
 * function pointers (push, pop, throw) and a convenience initializer.
 *
 * The driver creates an FLExceptionService, calls fl_exception_service_init()
 * to populate it with default implementations, then injects it into test DLLs
 * via fla_set_exception_service().
 *
 * Test code uses FL_TRY macros which access the service via g_fla_exception_service, a
 * TLS-based global instance of FLExceptionService.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <flp_exception_service.h>
#include <faultline/fl_exception_service.h>        // for FLExceptionService, FL_THRO...
#include <faultline/fl_exception_service_assert.h> // for fl_throw_assertion
#include <faultline/fl_exception_types.h>          // for FLExceptionEnvironment, FL_...
#include <faultline/fl_macros.h>                   // FL_THREAD_LOCAL
#include <setjmp.h>                      // for longjmp

static FL_THREAD_LOCAL FLExceptionEnvironment *g_stack;

static FLExceptionService g_exception_service = {
    .pop_env   = flp_pop,
    .push_env  = flp_push,
    .throw_exc = flp_throw,
};

FL_PUSH_EXCEPTION_SERVICE_FN(flp_push) {
    env->next = g_stack;
    g_stack   = env;
}

FL_POP_EXCEPTION_SERVICE_FN(flp_pop) {
    FLExceptionEnvironment *env = g_stack;
    g_stack                     = env->next;
}

FL_THROW_EXCEPTION_SERVICE_FN(flp_throw) {
    FLExceptionEnvironment *env = g_stack;
    g_stack                     = env->next;
    env->reason                 = reason;
    env->details                = details;
    env->file                   = file;
    env->line                   = line;
    longjmp(env->jmp, FL_THROWN);
}

FL_THROW_EXCEPTION_SERVICE_FN(fl_throw_assertion) {
    flp_throw(reason, details, file, line);
}

/**
 * @brief Connect the platform and application side of the exception service
 *
 */
FLP_INIT_EXCEPTION_SERVICE_FN(flp_init_exception_service) {
    fla_set(&g_exception_service, sizeof g_exception_service);
}
