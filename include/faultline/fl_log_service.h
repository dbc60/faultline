#ifndef FL_LOG_TYPES_H_
#define FL_LOG_TYPES_H_

/**
 * @file fl_log_types.h
 * @author Douglas Cuthbertson
 * @brief Log service interface shared by platform and application code.
 * @version 0.1
 * @date 2026-02-09
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_macros.h> // FL_DECL_SPEC

#if defined(__cplusplus)
extern "C" {
#endif

// Log levels (hierarchical)
typedef enum {
    LOG_LEVEL_FATAL   = 0,
    LOG_LEVEL_ERROR   = 1,
    LOG_LEVEL_WARN    = 2,
    LOG_LEVEL_INFO    = 3,
    LOG_LEVEL_VERBOSE = 4,
    LOG_LEVEL_DEBUG   = 5,
    LOG_LEVEL_TRACE   = 6
} FLLogLevel;

#define FL_WRITE_LOG_FN(name)                                               \
    void name(FLLogLevel level, char const *file, int line, char const *id, \
              char const *format, ...)
typedef FL_WRITE_LOG_FN(fl_write_log_fn);

typedef struct FLLogService {
    fl_write_log_fn *write;
} FLLogService;

// These definitions and declarations are common to both the platform and application
// implementations
#define FLA_SET_LOG_SERVICE_FN(name) void name(FLLogService *const svc, size_t size)
typedef FLA_SET_LOG_SERVICE_FN(fla_set_log_service_fn);
#define FLA_SET_LOG_SERVICE_STR FL_STR(fla_set_log_service)

#if defined(__cplusplus)
}
#endif

#endif // FL_LOG_TYPES_H_
