/**
 * @file fla_log_service.c
 * @author Douglas Cuthbertson
 * @brief Self-contained default log service for standalone library use.
 * @version 0.2
 * @date 2026-05-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Provides a working default write function that routes log output to stderr.
 * No driver initialization is required; the library is usable standalone.
 *
 * A driver may still call fla_set_log_service() to replace the default
 * (e.g., to route output through the platform log infrastructure).
 */
#include <faultline/fla_log_service.h>
#include <faultline/fl_log_types.h> // FLLogService, FL_WRITE_LOG_FN, FLA_SET_LOG_SERVICE_FN
#include <faultline/fl_macros.h>    // FL_DECL_SPEC
#include <stdarg.h>                 // va_list, va_start, va_end
#include <stdio.h>                  // fprintf, fflush, stderr
#include <stdlib.h>                 // abort
#include <string.h>                 // strrchr

static char const *const s_level_names[]
    = {"FATAL", "ERROR", "WARN", "INFO", "VERBOSE", "DEBUG", "TRACE"};

static FLLogLevel s_level = LOG_LEVEL_INFO;

static char const *basename_of(char const *path) {
    char const *sep = strrchr(path, '/');
    char const *bs  = strrchr(path, '\\');
    if (bs > sep) {
        sep = bs;
    }
    return sep ? sep + 1 : path;
}

static FL_WRITE_LOG_FN(default_write) {
    if (level > s_level) {
        return;
    }
    char const *filename = (file && *file) ? basename_of(file) : "?";
    fprintf(stderr, "[%-7s] %s:%d", s_level_names[level], filename, line);
    if (id && *id) {
        fprintf(stderr, " [%s]", id);
    }
    fprintf(stderr, " ");
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

FLLogService g_fla_log_service = {
    .write = default_write,
};

FL_DECL_SPEC FLA_SET_LOG_SERVICE_FN(fla_set_log_service) {
    if (size < sizeof(FLLogService)) {
        fprintf(stderr, "invalid log service - expected %zu bytes, received %zu\n",
                sizeof(FLLogService), size);
        fflush(stderr);
        abort();
    }
    g_fla_log_service.write = svc->write;
}
