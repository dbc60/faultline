#ifndef FLP_STREAM_SERVICE_H_
#define FLP_STREAM_SERVICE_H_

/**
 * @file flp_stream_service.h
 * @author Douglas Cuthbertson
 * @brief Platform-side stream service declarations.
 * @version 0.1
 * @date 2026-07-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_stream_service.h> // FL_STREAM_*_FN, fla_set_stream_service_fn

#if defined(__cplusplus)
extern "C" {
#endif

#define FLP_INIT_STREAM_SERVICE_FN(name) void name(fla_set_stream_service_fn *fla_set)
typedef FLP_INIT_STREAM_SERVICE_FN(flp_init_stream_service_fn);

/**
 * @brief Connect the platform and consumer implementations of the stream service.
 *
 * Fills the platform-owned service struct and hands it to fla_set so the consumer (the
 * core itself, or a loaded suite DLL) installs it into its g_fla_stream_service.
 */
FLP_INIT_STREAM_SERVICE_FN(flp_init_stream_service);

/**
 * @brief Platform-side implementations for the stream service.
 *
 * write has no offset parameter: an append-opened handle has no addressable
 * position, and neither does a console handle. close() is a no-op on a handle
 * obtained from console() (see fl_stream_service.h).
 */
FL_STREAM_OPEN_FN(flp_stream_open);
FL_STREAM_WRITE_FN(flp_stream_write);
FL_STREAM_CLOSE_FN(flp_stream_close);
FL_STREAM_CONSOLE_FN(flp_stream_console);

#if defined(__cplusplus)
}
#endif

#endif // FLP_STREAM_SERVICE_H_
