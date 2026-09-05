/**
 * @file fl_exception.c
 * @author Douglas Cuthbertson
 * @brief Exception reason constants and the push/pop/throw implementation.
 * @version 0.3
 * @date 2026-09-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Exceptions are not a service: there is no vtable and nothing is injected. Every
 * image compiles this file exactly once and fl_try.h calls fl_push, fl_pop and
 * fl_throw directly. The environment stack is static and thread-local, so each image
 * that compiles this file gets its own stack per thread: a throw inside a module
 * reaches an FL_TRY inside that same module, and two threads unwind independently.
 */
#include <faultline/fl_exception.h>        // FL_*_EXCEPTION_FN
#include <faultline/fl_exception_assert.h> // fl_throw_assertion
#include <faultline/fl_exception_types.h>  // FLExceptionEnvironment, FL_THROWN
#include <faultline/fl_macros.h>           // FL_THREAD_LOCAL
#include <setjmp.h>                        // longjmp
#include <stddef.h>                        // NULL
#include <stdio.h>                         // fprintf, fflush, stderr
#include <stdlib.h>                        // abort

#if defined(__cplusplus)
extern "C" {
#endif

//
// Exception reason constants
//

FLExceptionReason fl_expected_failure
    = "expected failure"; ///< test drivers catch this and do not report it as a failure
FLExceptionReason fl_unexpected_failure = "unexpected failure"; ///< something is wrong
FLExceptionReason fl_test_exception
    = "test exception"; ///< a test needs to throw and catch an exception
FLExceptionReason fl_not_implemented = "not implemented"; ///< useful in development
FLExceptionReason fl_invalid_value   = "invalid value";   ///< an argument is invalid
FLExceptionReason fl_internal_error  = "internal error";  ///< a bad state
FLExceptionReason fl_invalid_address = "invalid address"; ///< address not valid
FLExceptionReason fl_foreign_exception
    = "foreign exception"; ///< a non-FLException C++ exception reached an FL_TRY

//
// Environment stack
//

static FL_THREAD_LOCAL FLExceptionEnvironment *g_stack;

FL_PUSH_EXCEPTION_FN(fl_push) {
    env->next = g_stack;
    g_stack   = env;
}

FL_POP_EXCEPTION_FN(fl_pop) {
    FLExceptionEnvironment *env = g_stack;
    g_stack                     = env->next;
}

#if defined(FL_EXC_BACKEND_CXX)
// This backend keeps no jump buffer and no environment stack: C++ unwinding finds its
// own way back to the matching FL_CATCH, so fl_push and fl_pop are never reached. The
// throw leaves through a C-linkage function, which is why the build carries /EHs
// rather than /EHsc (config.cmd).
FL_THROW_EXCEPTION_FN(fl_throw) {
    throw FLException(reason, details, file, line);
}
#else
FL_THROW_EXCEPTION_FN(fl_throw) {
    FLExceptionEnvironment *env = g_stack;

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

    g_stack      = env->next;
    env->reason  = reason;
    env->details = details;
    env->file    = file;
    env->line    = line;
    longjmp(env->jmp, FL_THROWN);
}
#endif // FL_EXC_BACKEND_CXX

// (char const *expression, char const *details, char const *file, int line)
FL_THROW_EXCEPTION_FN(fl_throw_assertion) {
    fl_throw(reason, details, file, line);
}

#if defined(__cplusplus)
}
#endif
