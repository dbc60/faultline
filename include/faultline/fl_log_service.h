#ifndef FL_LOG_SERVICE_H_
#define FL_LOG_SERVICE_H_

/**
 * @file fl_log_service.h
 * @author Douglas Cuthbertson
 * @brief Log service interface shared by platform and consumer code.
 * @version 0.1
 * @date 2026-02-09
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_macros.h> // FL_DECL_SPEC, FL_PRINTF_FORMAT
#include <stddef.h>              // size_t

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

/*
 * The write signature, in one place. FL_WRITE_LOG_FN spells the function (this
 * macro declares and defines it, and names its type); FL_WRITE_LOG_PTR spells a
 * pointer to it.
 *
 * FL_PRINTF_FORMAT(5, 6) marks `format` as parameter 5 with its arguments
 * starting at 6. Nothing else checks a LOG_* call site: the format is forwarded
 * to a variadic function rather than handed to a printf-family call the compiler
 * already knows, so without the attribute a wrong conversion is silent.
 *
 * It leads the function declaration because that is the only position GCC
 * accepts on a definition, and FL_WRITE_LOG_FN spells definitions too. It is
 * restated on the pointer because the attribute does not travel from a typedef
 * to a call made through a pointer of that type, and a consumer build reaches
 * the service only through the pointer.
 */
#define FL_WRITE_LOG_PARAMS                                                            \
    (FLLogLevel level, char const *file, int line, char const *id, char const *format, \
     ...)

#define FL_WRITE_LOG_FN(name)  FL_PRINTF_FORMAT(5, 6) void name FL_WRITE_LOG_PARAMS
#define FL_WRITE_LOG_PTR(name) void(*name) FL_WRITE_LOG_PARAMS FL_PRINTF_FORMAT(5, 6)

typedef FL_WRITE_LOG_FN(fl_write_log_fn);

typedef struct FLLogService {
    FL_WRITE_LOG_PTR(write);
} FLLogService;

// These definitions are common to both the platform and consumer implementations
#define FLA_SET_LOG_SERVICE_FN(name) void name(FLLogService *const svc, size_t size)
typedef FLA_SET_LOG_SERVICE_FN(fla_set_log_service_fn);
#define FLA_SET_LOG_SERVICE_STR FL_STR(fla_set_log_service)

#if defined(__cplusplus)
}
#endif

#endif // FL_LOG_SERVICE_H_
