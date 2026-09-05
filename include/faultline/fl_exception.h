#ifndef FL_EXCEPTION_H_
#define FL_EXCEPTION_H_

/**
 * @file fl_exception.h
 * @author Douglas Cuthbertson
 * @brief Exception reasons and the push/pop/throw operations behind FL_TRY/FL_THROW.
 * @version 0.3
 * @date 2026-09-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Exceptions are not a service: there is no vtable, no setter and no injection. Every
 * image compiles fl_exception.c exactly once, whether it is platform code or consumer
 * code, and fl_try.h calls these three directly.
 */
#include <faultline/fl_exception_types.h> // FLExceptionEnvironment, FLExceptionReason
#include <faultline/fl_macros.h>          // FL_STR
#include <stddef.h>                       // size_t

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief fl_expected_failure is meant to be thrown from test cases, but not caught
 * within the test. The test driver should catch it and NOT report it as a failed test
 * case.
 */
extern FLExceptionReason fl_expected_failure;

/**
 * @brief fl_test_exception is useful during testing. It's meant to be thrown and caught
 * within test cases. If it is not caught, then the test driver will report it as an
 * uncaught exception. It's mostly used within the test suite for the exception library.
 */
extern FLExceptionReason fl_test_exception;

/**
 * @brief fl_not_implemented is useful during development to track unimplemented
 * features.
 */
extern FLExceptionReason fl_not_implemented;

/**
 * @brief fl_invalid_value can be thrown by a function that has been passed an argument
 * with an invalid value.
 */
extern FLExceptionReason fl_invalid_value;

/**
 * @brief fl_internal_error can be thrown by a function that is in a bad state. This
 * probably means there's a bug in its component/library.
 */
extern FLExceptionReason fl_internal_error;

/**
 * @brief fl_invalid_address can be thrown by a function that has been passed an invalid
 * address, such as one that has already been freed or one that is not within the bounds
 * of a valid memory region.
 */
extern FLExceptionReason fl_invalid_address;

/**
 * @brief fl_foreign_exception is the reason FL_CATCH_ALL/FL_CATCH_ALL_RETHROW report for
 * a std::exception (or other C++ exception) that reached an FL_TRY without going through
 * FL_THROW. Only meaningful under the C++ exception backend (FL_EXC_BACKEND_CXX); the
 * setjmp backend has no way for a foreign exception to arrive.
 */
extern FLExceptionReason fl_foreign_exception;

/**
 * @brief The three operations the exception implementation provides.
 */
#define FL_PUSH_EXCEPTION_FN(name) void name(FLExceptionEnvironment *env)
#define FL_POP_EXCEPTION_FN(name)  void name(void)
#define FL_THROW_EXCEPTION_FN(name) \
    void name(FLExceptionReason reason, char const *details, char const *file, int line)

/**
 * @brief Push an FLExceptionEnvironment on to this image's stack. FL_TRY does this.
 */
FL_PUSH_EXCEPTION_FN(fl_push);

/**
 * @brief Pop an FLExceptionEnvironment from this image's stack. FL_CATCH, FL_CATCH_ALL,
 * FL_FINALLY and FL_END_TRY do this.
 */
FL_POP_EXCEPTION_FN(fl_pop);

/**
 * @brief Pop an environment and jump to it, by longjmp under the setjmp backend or by
 * throwing FLException under the C++ one.
 *
 * Precondition: an enclosing FL_TRY on this thread. Without one there is no environment
 * to pop, which is a programming error: the throw site goes to stderr and the process
 * aborts.
 */
FL_THROW_EXCEPTION_FN(fl_throw);

#define FL_REASON  (fl_env_.reason)
#define FL_DETAILS (fl_env_.details)
#define FL_FILE    (fl_env_.file)
#define FL_LINE    (fl_env_.line)

#if defined(__cplusplus)
}
#endif

#endif // FL_EXCEPTION_H_
