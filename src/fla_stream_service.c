/**
 * @file fla_stream_service.c
 * @author Douglas Cuthbertson
 * @brief The consumer side of a stream service.
 * @version 0.1
 * @date 2026-07-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fla_stream_service.h> // fla_set_stream_service declaration
#include <faultline/fl_stream_service.h>  // FLStreamService, FLA_SET_STREAM_SERVICE_FN
#include <faultline/fl_macros.h>          // FL_UNUSED, FL_DECL_SPEC
#include <stddef.h>                       // NULL
#include <stdio.h>                        // fprintf, stderr
#include <stdlib.h>                       // abort

// Default stubs abort: using the stream service before a platform provider injects one
// is a programming error, not a recoverable condition.

static FL_STREAM_OPEN_FN(default_stream_open) {
    FL_UNUSED(path);
    fprintf(stderr, "Stream service is uninitialized - no open function provided\n");
    fflush(stderr);
    abort();
}

static FL_STREAM_WRITE_FN(default_stream_write) {
    FL_UNUSED(f);
    FL_UNUSED(buf);
    FL_UNUSED(bytes);
    fprintf(stderr, "Stream service is uninitialized - no write function provided\n");
    fflush(stderr);
    abort();
}

static FL_STREAM_CLOSE_FN(default_stream_close) {
    FL_UNUSED(f);
    fprintf(stderr, "Stream service is uninitialized - no close function provided\n");
    fflush(stderr);
    abort();
}

static FL_STREAM_CONSOLE_FN(default_stream_console) {
    FL_UNUSED(which);
    fprintf(stderr, "Stream service is uninitialized - no console function provided\n");
    fflush(stderr);
    abort();
}

FLStreamService g_fla_stream_service = {
    .open    = default_stream_open,
    .write   = default_stream_write,
    .close   = default_stream_close,
    .console = default_stream_console,
};

FL_DECL_SPEC FLA_SET_STREAM_SERVICE_FN(fla_set_stream_service) {
    if (svc == NULL) {
        fprintf(stderr, "invalid stream service - NULL service address\n");
        fflush(stderr);
        abort();
    }
    if (size < sizeof(FLStreamService)) {
        fprintf(stderr, "invalid stream service - expected %zu bytes, received %zu\n",
                sizeof(FLStreamService), size);
        fflush(stderr);
        abort();
    }

    g_fla_stream_service = *svc;
}
