#ifndef FLA_TIMER_SERVICE_H_
#define FLA_TIMER_SERVICE_H_

/**
 * @file fla_timer_service.h
 * @author Douglas Cuthbertson
 * @brief The application side of the timer service
 * @version 0.1
 * @date 2026-06-18
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_timer_service.h> // FLTimerService, FLA_SET_TIMER_SERVICE_FN

#if defined(__cplusplus)
extern "C" {
#endif

extern FLTimerService g_fla_timer_service;

/**
 * @brief Set the timer service for an application.
 * @param svc  Address of a platform-owned timer service
 * @param size the size of the timer service in bytes
 */
FL_DECL_SPEC FLA_SET_TIMER_SERVICE_FN(fla_set_timer_service);

#if defined(__cplusplus)
}
#endif

#endif // FLA_TIMER_SERVICE_H_
