#ifndef FLA_LOG_SERVICE_H_
#define FLA_LOG_SERVICE_H_

/**
 * @file fla_log_service.h
 * @author Douglas Cuthbertson
 * @brief Application-side log service macros for test suite DLLs.
 * @version 0.1
 * @date 2026-02-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_log_service.h> // FLLogService, FLLogLevel

#if defined(__cplusplus)
extern "C" {
#endif

extern FLLogService g_fla_log_service;

/**
 * @brief Set the log service for an application.
 *
 * @param svc Address of a platform-owned log service
 * @param size the size of the log service in bytes
 */
FL_DECL_SPEC FLA_SET_LOG_SERVICE_FN(fla_set_log_service);

#if defined(__cplusplus)
}
#endif

#endif // FLA_LOG_SERVICE_H_
