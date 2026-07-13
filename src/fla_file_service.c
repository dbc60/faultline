/**
 * @file fla_file_service.c
 * @author Douglas Cuthbertson
 * @brief The consumer side of a file service.
 * @version 0.1
 * @date 2026-06-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fla_file_service.h> // fla_set_file_service declaration
#include <faultline/fl_file_service.h>  // FLFileService, FLA_SET_FILE_SERVICE_FN
#include <faultline/fl_macros.h>        // FL_UNUSED, FL_DECL_SPEC
#include <stddef.h>                     // NULL
#include <stdio.h>                      // fprintf, stderr
#include <stdlib.h>                     // abort

// Default stubs abort: using the file service before a platform provider injects one is
// a programming error, not a recoverable condition.

static FL_FILE_OPEN_FN(default_file_open) {
    FL_UNUSED(path);
    FL_UNUSED(mode);
    fprintf(stderr, "File service is uninitialized - no open function provided\n");
    fflush(stderr);
    abort();
}

static FL_FILE_READ_FN(default_file_read) {
    FL_UNUSED(f);
    FL_UNUSED(buf);
    FL_UNUSED(bytes);
    FL_UNUSED(offset);
    fprintf(stderr, "File service is uninitialized - no read function provided\n");
    fflush(stderr);
    abort();
}

static FL_FILE_WRITE_FN(default_file_write) {
    FL_UNUSED(f);
    FL_UNUSED(buf);
    FL_UNUSED(bytes);
    FL_UNUSED(offset);
    fprintf(stderr, "File service is uninitialized - no write function provided\n");
    fflush(stderr);
    abort();
}

static FL_FILE_CLOSE_FN(default_file_close) {
    FL_UNUSED(f);
    fprintf(stderr, "File service is uninitialized - no close function provided\n");
    fflush(stderr);
    abort();
}

FLFileService g_fla_file_service = {
    .open  = default_file_open,
    .read  = default_file_read,
    .write = default_file_write,
    .close = default_file_close,
};

FL_DECL_SPEC FLA_SET_FILE_SERVICE_FN(fla_set_file_service) {
    if (svc == NULL) {
        fprintf(stderr, "invalid file service - NULL service address\n");
        fflush(stderr);
        abort();
    }
    if (size < sizeof(FLFileService)) {
        fprintf(stderr, "invalid file service - expected %zu bytes, received %zu\n",
                sizeof(FLFileService), size);
        fflush(stderr);
        abort();
    }

    g_fla_file_service = *svc;
}
