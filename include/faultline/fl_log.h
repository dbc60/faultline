#ifndef FL_LOG_H_
#define FL_LOG_H_

/**
 * @file fl_log.h
 * @brief Unified LOG_* macros that work in both platform and application code.
 *
 * This header is the single definition site for the LOG_ERROR/LOG_INFO/... family.
 * It selects the log-service backend -- platform (flp_write_log) or application
 * (g_fla_log_service.write) -- by whether FL_BUILD_DRIVER is defined, then defines
 * the LOG_* set once over that backend through FL_LOG_WRITE. A translation unit
 * gets exactly one backend, and the call-site names never carry a platform/
 * application prefix: the side is fixed per TU by the build, not per call.
 */
#include <faultline/fl_log_service.h> // FLLogLevel, LOG_LEVEL_*

#if defined(FL_BUILD_DRIVER)
#include <flp_log_service.h> // IWYU pragma: export -- flp_write_log
#define FL_LOG_WRITE(level, file, line, id, msg, ...) \
    flp_write_log(level, file, line, id, msg, ##__VA_ARGS__)
#else                                  // application / DLL build
#include <faultline/fla_log_service.h> // IWYU pragma: export -- g_fla_log_service
#define FL_LOG_WRITE(level, file, line, id, msg, ...) \
    g_fla_log_service.write(level, file, line, id, msg, ##__VA_ARGS__)
#endif // FL_BUILD_DRIVER

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

#endif // FL_LOG_H_
