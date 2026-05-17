#ifndef FL_EXCEPTION_SERVICE_H_
#define FL_EXCEPTION_SERVICE_H_

/**
 * @file fl_exception_service.h
 * @author Douglas Cuthbertson
 * @brief define an exception service interface
 * @version 0.1
 * @date 2026-02-01
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_exception_types.h> // FLExceptionEnvironment, FLExceptionReason, fl_exception_handler_fn
#include <faultline/fl_macros.h> // FL_STR

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
 * @brief Exception handling service provided by the test driver.
 *
 * The driver owns this struct and populates it with:
 * - stack: linked list of exception environments (initially NULL)
 * - handler: function to call when exception stack is empty (unhandled exception)
 * - ctx: driver's opaque context passed to handler
 * - Function pointers for push/pop/throw operations
 */
typedef struct FLExceptionService FLExceptionService;

#define FL_PUSH_EXCEPTION_SERVICE_FN(name) void name(FLExceptionEnvironment *env)
typedef FL_PUSH_EXCEPTION_SERVICE_FN(fl_push_exception_service_fn);

#define FL_POP_EXCEPTION_SERVICE_FN(name) void name(void)
typedef FL_POP_EXCEPTION_SERVICE_FN(fl_pop_exception_service_fn);

#define FL_THROW_EXCEPTION_SERVICE_FN(name) \
    void name(FLExceptionReason reason, char const *details, char const *file, int line)
typedef FL_THROW_EXCEPTION_SERVICE_FN(fl_throw_exception_service_fn);

#define FL_REASON  (fl_env_.reason)
#define FL_DETAILS (fl_env_.details)
#define FL_FILE    (fl_env_.file)
#define FL_LINE    (fl_env_.line)

/**
 * @brief FLExceptionService is the interface between the platform/test-driver code and
 * the application/test-suite code.
 *
 * @param push push an exception environment on to a stack
 * @param pop pop and return an exception environment from the top of a stack
 * @param throw pop an exception environment from the top of a stack and jump to its
 * location.
 */
struct FLExceptionService {
    fl_push_exception_service_fn *push_env;
    fl_pop_exception_service_fn  *pop_env;
    // volatile prevents LLVM from devirtualizing this pointer to its initial value
    // (default_throw, which calls abort()) and incorrectly inferring it is noreturn.
    // Without volatile, LLVM eliminates FL_CATCH body code under -O1/-O2 because it
    // treats every call through throw_exc as noreturn and dead-code-eliminates the
    // continuation. volatile forces a memory load at each call site, blocking the
    // interprocedural noreturn inference.
    fl_throw_exception_service_fn *volatile throw_exc;
};

// These definitions are common to both the platform and application implementations
#define FLA_SET_EXCEPTION_SERVICE_FN(name) \
    void name(FLExceptionService *const svc, size_t size)
typedef FLA_SET_EXCEPTION_SERVICE_FN(fla_set_exception_service_fn);
#define FLA_SET_EXCEPTION_SERVICE_STR FL_STR(fla_set_exception_service)

#if defined(__cplusplus)
}
#endif

#endif // FL_EXCEPTION_SERVICE_H_
