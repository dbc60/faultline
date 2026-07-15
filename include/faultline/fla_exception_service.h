#ifndef FLA_EXCEPTION_SERVICE_H_
#define FLA_EXCEPTION_SERVICE_H_

/**
 * @file fla_exception_service.h
 * @author Douglas Cuthbertson
 * @brief Consumer-side exception service: the injected g_fla_exception_service.
 * @version 0.1
 * @date 2026-02-01
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * The FL_TRY/FL_CATCH/FL_THROW macros live in fl_try.h, which routes them through
 * g_fla_exception_service (this consumer accessor) when FL_PLATFORM_BUILD is not
 * defined.
 */
#include <faultline/fl_exception_service.h> // FLExceptionService, FLA_SET_EXCEPTION_SERVICE_FN
#include <faultline/fl_macros.h>            // FL_DECL_SPEC

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Exception service.
 *
 * Each DLL gets its own copy of this variable (static linkage). The driver sets it via
 * fla_set_exception_service() after loading the DLL so its push, pop, and throw fields
 * point to the driver's implementation. The FL_TRY*, FL_CATCH*, and FL_END_TRY* macros
 * (in fl_try.h) use it to manage the stack of exception environments.
 */
extern FLExceptionService g_fla_exception_service;

/**
 * @brief Set the exception service for a consumer.
 *
 * @param svc Address of a platform-owned exception service
 * @param size the size of the exception service in bytes
 */
FL_DECL_SPEC FLA_SET_EXCEPTION_SERVICE_FN(fla_set_exception_service);

#if defined(__cplusplus)
}
#endif

#endif // FLA_EXCEPTION_SERVICE_H_
