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
#include <faultline/fl_log_service.h>    // FLLogLevel, FL_WRITE_LOG_FN
#include <faultline/fl_stream_service.h> // FLConsoleStream

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
 * The file is opened through the stream service (append semantics). On failure
 * the logger falls back to stdout and prints a warning to stderr.
 *
 * @param path File path to open.
 */
void flp_log_set_output_path(char const *path);

/**
 * @brief Direct log output to one of the process's console streams.
 *
 * Opt-in only. Nothing calls this automatically. The default output stays raw stdio (see
 * flp_log_set_output); this routes writes through the stream service instead, making
 * them fault-injectable like the path-based output. On failure the logger falls back to
 * stdout and prints a warning to stderr.
 *
 * @param which Which console stream (stdout or stderr) to direct output to.
 */
void flp_log_set_output_console(FLConsoleStream which);

/**
 * @brief Platform-side write implementation for the log service.
 */
FL_WRITE_LOG_FN(flp_write_log);

#if defined(__cplusplus)
}
#endif

#endif // FLP_LOG_SERVICE_H_
