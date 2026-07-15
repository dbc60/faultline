#ifndef FLP_FILE_SERVICE_H_
#define FLP_FILE_SERVICE_H_

/**
 * @file flp_file_service.h
 * @author Douglas Cuthbertson
 * @brief Platform-side file service declarations.
 * @version 0.1
 * @date 2026-06-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_file_service.h> // FL_FILE_*_FN, fla_set_file_service_fn

#if defined(__cplusplus)
extern "C" {
#endif

#define FLP_INIT_FILE_SERVICE_FN(name) void name(fla_set_file_service_fn *fla_set)
typedef FLP_INIT_FILE_SERVICE_FN(flp_init_file_service_fn);

/**
 * @brief Connect the platform and consumer implementations of the file service.
 *
 * Fills the platform-owned service struct and hands it to fla_set so the consumer (the
 * core itself, or a loaded suite DLL) installs it into its g_fla_file_service.
 */
FLP_INIT_FILE_SERVICE_FN(flp_init_file_service);

/**
 * @brief Platform-side implementations for the file service.
 *
 * read/write are positional: each names the byte offset to act on and leaves the
 * handle's file pointer untouched, so a handle is safe to use from several threads.
 * An FL_FILE_APPEND handle is the exception: it ignores the write offset and
 * appends at end of file (see fl_file_service.h).
 */
FL_FILE_OPEN_FN(flp_file_open);
FL_FILE_READ_FN(flp_file_read);
FL_FILE_WRITE_FN(flp_file_write);
FL_FILE_CLOSE_FN(flp_file_close);

#if defined(__cplusplus)
}
#endif

#endif // FLP_FILE_SERVICE_H_
