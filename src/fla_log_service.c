/**
 * @file fla_log_service.c
 * @author Douglas Cuthbertson
 * @brief The application side of a log service.
 * @version 0.1
 * @date 2026-02-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_log_types.h> // for FLLogService, FLA_SET_LOG_SERVICE_FN
#include <faultline/fl_macros.h>    // for FL_UNUSED, FL_DECL_SPEC
#include <stdio.h>                  // for fprintf, stderr
#include <stdlib.h>                 // for abort

static FL_WRITE_LOG_FN(default_write) {
    FL_UNUSED(level);
    FL_UNUSED(file);
    FL_UNUSED(line);
    FL_UNUSED(id);
    FL_UNUSED(format);
    fprintf(stderr, "Log service is uninitialized - no write function provided\n");
    fflush(stderr);
    abort();
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
