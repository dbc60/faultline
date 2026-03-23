#if defined(__clang__) || defined(__GNUC__)
#include <sec_api/string_s.h> // IWYU pragma: keep for strncpy_s
#endif
#include <string.h>                                // strlen, strncpy_s
#include <time.h>                                  // time_t, struct tm, time, strftime
#include <faultline/arena.h>                       // Arena
#include <faultline/fl_context.h>                  // FLContext
#include <faultline/fl_exception_service_assert.h> // for FL_ASSERT_NOT_NULL, FL_ASSE...
#include <faultline/fl_log.h>                      // LOG_ERROR, LOG_WARN, LOG_INFO
#include <faultline/fl_macros.h>                   // FL_UNUSED
#include <flp_exception_service.h>                 // FL_THROW
#include <stdio.h>                                 // FILE, fputs, fputc, FILENAME_MAX
#include <cwalk/include/cwalk.h>                   // cwk_path_normalize, cwk_path_*
#include "flp_time.h"                              // fl_gmtime
#include "output_junit.h"                          // JUnitXML

#define TIMESTAMP_SIZE   32
#define TIMESTAMP_FORMAT "%Y-%m-%dT%H:%M:%SZ" // ISO 8601 for XML content
#define JUNIT_MODULE     "JUNIT XML"

FLExceptionReason const junit_open_failure = "failed to open junit file";

static void xml_write_escaped(FILE *file, char const *str) {
    if (!str) {
        return;
    }

    for (; *str; ++str) {
        switch (*str) {
        case '&':
            fputs("&amp;", file);
            break;
        case '<':
            fputs("&lt;", file);
            break;
        case '>':
            fputs("&gt;", file);
            break;
        case '"':
            fputs("&quot;", file);
            break;
        default:
            fputc(*str, file);
            break;
        }
    }
}

static void junit_open(JUnitXML *junit) {
    if (junit->file != NULL) {
        // already opened
        return;
    }

    int err = fopen_s(&junit->file, junit->path, "a");
    if (err != 0) {
        LOG_ERROR(JUNIT_MODULE, "error opening file \"%s\": %d", junit->path, err);
        FL_THROW(junit_open_failure);
    }
}

static void junit_close(JUnitXML *junit) {
    FL_ASSERT_NOT_NULL(junit->file);
    (void)fclose(junit->file);
    junit->file = NULL;
}

static void initialize(JUnitXML *junit, Arena *arena, char const *path, char *buf,
                       size_t buf_size, JUnitReleaseType release) {
    junit->arena   = arena;
    junit->file    = NULL;
    junit->path    = buf;
    junit->size    = buf_size;
    junit->release = release;
    cwk_path_normalize(path, buf, buf_size);
}

void junit_init(JUnitXML *junit, Arena *arena, char const *path) {
    FL_ASSERT_NOT_NULL(path);

    size_t norm_len = cwk_path_normalize(path, NULL, 0);
    size_t buf_size = norm_len + 1;
    char  *buf      = ARENA_CALLOC_THROW(arena, 1, buf_size);
    initialize(junit, arena, path, buf, buf_size, JUNIT_PATH_ONLY);
}

JUnitXML *new_junit(Arena *arena, char const *path) {
    FL_ASSERT_NOT_NULL(path);

    size_t    norm_len = cwk_path_normalize(path, NULL, 0);
    size_t    buf_size = norm_len + 1;
    JUnitXML *junit    = ARENA_CALLOC_THROW(arena, 1, sizeof(JUnitXML));
    char     *buf      = ARENA_CALLOC_THROW(arena, 1, buf_size);
    initialize(junit, arena, path, buf, buf_size, JUNIT_ALL);
    return junit;
}

void destroy_junit(JUnitXML *junit) {
    FL_ASSERT_NOT_NULL(junit->path);
    ARENA_FREE_THROW(junit->arena, junit->path);
    junit->path = NULL;
    if (junit->release == JUNIT_ALL) {
        ARENA_FREE_THROW(junit->arena, junit);
    }
}

// See
// https://github.com/testmoapp/junitxml?tab=readme-ov-file#complete-junit-xml-example
int junit_begin(JUnitXML *junit) {
    int rc = 0;
    if (junit->path != NULL) {
        remove(junit->path); // delete any existing file so we start fresh
        junit_open(junit);
        char const xml_decl[]       = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        char const testsuites_tag[] = "<testsuites>\n";
        size_t     length           = sizeof xml_decl - 1;
        size_t     written = fwrite(xml_decl, sizeof(char), length, junit->file);
        if (written < length) {
            LOG_ERROR(JUNIT_MODULE, "wrote %zu bytes, expected %zu", written, length);
            junit_close(junit);
            rc = -1;
        }

        if (rc == 0) {
            length  = sizeof testsuites_tag - 1;
            written = fwrite(testsuites_tag, sizeof(char), length, junit->file);
            if (written < length) {
                LOG_ERROR(JUNIT_MODULE, "wrote %zu bytes, expected %zu", written,
                          length);
                junit_close(junit);
                rc = -1;
            }
        }

        junit_close(junit);
    }

    return rc;
}

int junit_end(JUnitXML *junit) {
    int rc = 0;
    if (junit->path != NULL) {
        junit_open(junit);
        char const close_testsuites_tag[] = "</testsuites>\n";
        size_t     length                 = sizeof close_testsuites_tag - 1;
        size_t written = fwrite(close_testsuites_tag, sizeof(char), length, junit->file);
        if (written != length) {
            LOG_ERROR(JUNIT_MODULE, "wrote %zu bytes, expected %zu", written, length);
            rc = -1;
        }

        junit_close(junit);
    }

    return rc;
}

int junit_write(JUnitXML *junit, FLContext *fctx) {
    if (junit->path != NULL) {
        junit_open(junit);
        FLTestSuite *ts                    = fctx->ts;
        double       total_elapsed_seconds = 0.0;
        size_t       results_count         = faultline_get_results_count(fctx);
        for (size_t i = 0; i < results_count; i++) {
            total_elapsed_seconds
                += faultline_test_summary_buffer_get(&fctx->results, i)->elapsed_seconds;
        }
        char       timestamp[TIMESTAMP_SIZE];
        struct tm  tm_result;
        struct tm *tm_info = fl_gmtime(&fctx->run_start_time, &tm_result);
        if (tm_info != NULL) {
            strftime(timestamp, sizeof(timestamp), TIMESTAMP_FORMAT, tm_info);
        } else {
            timestamp[0] = '\0';
        }
        fputs("    <testsuite name=\"", junit->file);
        xml_write_escaped(junit->file, ts->name);
        fputs("\"\n               classname=\"", junit->file);
        xml_write_escaped(junit->file, ts->name);
        fprintf(junit->file,
                "\"\n"
                "               tests=\"%zu\"\n"
                "               failures=\"%zu\"\n"
                "               errors=\"%zu\"\n"
                "               time=\"%.3f\"\n"
                "               timestamp=\"%s\">\n"
                "    </testsuite>\n",
                fctx->tests_run, fctx->tests_failed,
                fctx->setups_failed + fctx->cleanups_failed, total_elapsed_seconds,
                timestamp);

        junit_close(junit);
    }

    return 0;
}
