#ifndef FLA_STREAM_SERVICE_H_
#define FLA_STREAM_SERVICE_H_

/**
 * @file fla_stream_service.h
 * @author Douglas Cuthbertson
 * @brief Consumer-side stream service accessor for the core and suite DLLs.
 * @version 0.1
 * @date 2026-07-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_stream_service.h> // FLStreamService, FLA_SET_STREAM_SERVICE_FN

#if defined(__cplusplus)
extern "C" {
#endif

extern FLStreamService g_fla_stream_service;

/**
 * @brief Install a platform-owned stream service into the consumer.
 *
 * @param svc  Address of a platform-owned stream service.
 * @param size The size of the stream service in bytes.
 */
FL_DECL_SPEC FLA_SET_STREAM_SERVICE_FN(fla_set_stream_service);

#if defined(__cplusplus)
}
#endif

#endif // FLA_STREAM_SERVICE_H_
