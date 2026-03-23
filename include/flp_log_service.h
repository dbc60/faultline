#ifndef FLP_LOG_SERVICE_H_
#define FLP_LOG_SERVICE_H_

/**
 * @file flp_log_service.h
 * @author Douglas Cuthbertson
 * @brief Platform-side log service declarations and convenience macros.
 * @version 0.1
 * @date 2026-02-09
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_log_types.h> // FLLogLevel, FL_WRITE_LOG_FN

#include <stdio.h> // FILE

#if defined(__cplusplus)
extern "C" {
#endif

#define FLP_INIT_LOG_SERVICE_FN(name) void name(fla_set_log_service_fn *fla_set)
typedef FLP_INIT_LOG_SERVICE_FN(flp_init_log_service_fn);

/**
 * @brief Connect the platform and application implementations of the exception service
 */
FLP_INIT_LOG_SERVICE_FN(flp_init_log_service);

void flp_log_init_custom(FLLogLevel level, char const *output);
/**
 * @brief Initialize the platform logger.
 *
 * Sets up the mutex, output to stdout, enabled=true, level=LOG_INFO.
 * Call once at driver startup.
 */
void flp_log_init(void);

/**
 * @brief Shut down the platform logger.
 *
 * Flushes output, closes the file if owned, and destroys the mutex.
 * Call once at driver shutdown.
 */
void flp_log_cleanup(void);

/**
 * @brief Set the minimum log level.
 *
 * @param level Messages above this level are suppressed.
 */
void flp_log_set_level(FLLogLevel level);

/**
 * @brief Set the log output to an already-open FILE.
 *
 * @param file Output stream (NULL defaults to stdout). The caller retains
 *             ownership; the logger will NOT close this file.
 */
void flp_log_set_output(FILE *file);

/**
 * @brief Open a file by path and direct log output to it.
 *
 * The file is opened in append mode. On failure the logger falls back to
 * stdout and prints a warning to stderr.
 *
 * @param path File path to open.
 */
void flp_log_set_output_path(char const *path);

/**
 * @brief Platform-side write implementation for the log service.
 */
FL_WRITE_LOG_FN(flp_write_log);

// Convenience macros
#define FL_LOG_WRITE(level, file, line, id, msg, ...) \
    flp_write_log(level, file, line, id, msg, ##__VA_ARGS__)

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

#endif // FLP_LOG_SERVICE_H_
