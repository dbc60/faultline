#ifndef FLP_EXCEPTION_SERVICE_H_
#define FLP_EXCEPTION_SERVICE_H_

/**
 * @file flp_exception_service.h
 * @author Douglas Cuthbertson
 * @brief Platform-side exception service: the concrete push/pop/throw provider.
 * @version 0.2
 * @date 2026-01-29
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * The driver creates and initializes an FLExceptionService, then injects it into
 * test DLLs via fla_set_exception_service(). The FL_TRY/FL_CATCH/FL_THROW macros
 * live in fl_try.h, which selects this platform provider (flp_push/pop/throw) when
 * FL_PLATFORM_BUILD is defined.
 */
#include <faultline/fl_exception_service.h> // FL_*_EXCEPTION_SERVICE_FN, fla_set_exception_service_fn

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Connect the platform and consumer implementations of the exception service.
 */
#define FLP_INIT_EXCEPTION_SERVICE_FN(name) \
    void name(fla_set_exception_service_fn *fla_set)
typedef FLP_INIT_EXCEPTION_SERVICE_FN(flp_init_exception_service_fn);
FLP_INIT_EXCEPTION_SERVICE_FN(flp_init_exception_service);

/**
 * @brief push an FLExceptionEnvironment on to a stack. This is done by FL_TRY.
 */
FL_PUSH_EXCEPTION_SERVICE_FN(flp_push);

/**
 * @brief pop an FLExceptionEnvironment from a stack. This is done by FL_CATCH,
 * FL_CATCH_ALL, FL_FINALLY, and FL_END_TRY.
 */
FL_POP_EXCEPTION_SERVICE_FN(flp_pop);

/**
 * @brief pop an FLExceptionEnvironment from a stack and use it to throw an exception
 * (via longjmp).
 *
 * Precondition: for the platform (FLP) implementation, the driver must have a top-level
 * try-catch block to ensure there is always an exception environment to pop.
 */
FL_THROW_EXCEPTION_SERVICE_FN(flp_throw);

#if defined(__cplusplus)
}
#endif

#endif // FLP_EXCEPTION_SERVICE_H_
