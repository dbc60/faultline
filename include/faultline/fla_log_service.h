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
#include <faultline/fl_log_types.h> // FLLogService, FLLogLevel

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

// Convenience macros
#define FL_LOG_WRITE(level, file, line, id, msg, ...) \
    g_fla_log_service.write(level, file, line, id, msg, ##__VA_ARGS__)

#define LOG_FATAL(ID, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_FATAL, __FILE__, __LINE__, (ID), (MSG), ##__VA_ARGS__)
#define LOG_ERROR(ID, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_ERROR, __FILE__, __LINE__, (ID), (MSG), ##__VA_ARGS__)
#define LOG_WARN(ID, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_WARN, __FILE__, __LINE__, (ID), (MSG), ##__VA_ARGS__)
#define LOG_INFO(ID, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_INFO, __FILE__, __LINE__, (ID), (MSG), ##__VA_ARGS__)
#define LOG_VERBOSE(ID, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_VERBOSE, __FILE__, __LINE__, (ID), (MSG), ##__VA_ARGS__)
#define LOG_DEBUG(ID, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_DEBUG, __FILE__, __LINE__, (ID), (MSG), ##__VA_ARGS__)
#define LOG_TRACE(ID, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_TRACE, __FILE__, __LINE__, (ID), (MSG), ##__VA_ARGS__)

#define LOG_FATAL_FILE_LINE(ID, FILE, LINE, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_FATAL, (FILE), (LINE), (ID), (MSG), ##__VA_ARGS__)
#define LOG_ERROR_FILE_LINE(ID, FILE, LINE, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_ERROR, (FILE), (LINE), (ID), (MSG), ##__VA_ARGS__)
#define LOG_WARN_FILE_LINE(ID, FILE, LINE, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_WARN, (FILE), (LINE), (ID), (MSG), ##__VA_ARGS__)
#define LOG_INFO_FILE_LINE(ID, FILE, LINE, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_INFO, (FILE), (LINE), (ID), (MSG), ##__VA_ARGS__)
#define LOG_VERBOSE_FILE_LINE(ID, FILE, LINE, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_VERBOSE, (FILE), (LINE), (ID), (MSG), ##__VA_ARGS__)
#define LOG_DEBUG_FILE_LINE(ID, FILE, LINE, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_DEBUG, (FILE), (LINE), (ID), (MSG), ##__VA_ARGS__)
#define LOG_TRACE_FILE_LINE(ID, FILE, LINE, MSG, ...) \
    FL_LOG_WRITE(LOG_LEVEL_TRACE, (FILE), (LINE), (ID), (MSG), ##__VA_ARGS__)

#if defined(__cplusplus)
}
#endif

#endif // FLA_LOG_SERVICE_H_
