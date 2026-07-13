#ifndef FLA_FILE_SERVICE_H_
#define FLA_FILE_SERVICE_H_

/**
 * @file fla_file_service.h
 * @author Douglas Cuthbertson
 * @brief Consumer-side file service accessor for the core and suite DLLs.
 * @version 0.1
 * @date 2026-06-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_file_service.h> // FLFileService, FLA_SET_FILE_SERVICE_FN

#if defined(__cplusplus)
extern "C" {
#endif

extern FLFileService g_fla_file_service;

/**
 * @brief Install a platform-owned file service into the consumer.
 *
 * @param svc  Address of a platform-owned file service.
 * @param size The size of the file service in bytes.
 */
FL_DECL_SPEC FLA_SET_FILE_SERVICE_FN(fla_set_file_service);

#if defined(__cplusplus)
}
#endif

#endif // FLA_FILE_SERVICE_H_
