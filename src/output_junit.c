#include <string.h>                                // strlen, memcpy
#include <time.h>                                  // time_t, struct tm, time, strftime
#include <faultline/arena.h>                       // Arena
#include <faultline/fl_context.h>                  // FLContext
#include <faultline/fl_exception_service_assert.h> // for FL_ASSERT_NOT_NULL, FL_ASSE...
#include <faultline/fl_log.h>                      // LOG_ERROR, LOG_WARN, LOG_INFO
#include <faultline/fl_macros.h>                   // FL_UNUSED, FL_PRINTF_FORMAT
#include <faultline/fl_result_codes.h>             // FL_PASS
#include <faultline/fl_test.h>                     // FLTestSuite, FLTestCase
#include <faultline/fl_test_summary.h> // FLTestSummary, faultline_test_summary_buffer_get
#include <faultline/fl_try.h>          // FL_THROW (selects flp_ backstop)
#include <faultline/fl_file.h>         // FL_FILE_OPEN/WRITE/CLOSE (selects backend)
#include <faultline/fl_file_types.h>   // FLFile, FL_FILE_WRITE
#include <faultline/fl_stream.h>       // FL_STREAM_OPEN/WRITE/CLOSE (selects backend)
#include <stdarg.h>                    // va_list, va_start, va_end
#include <stdbool.h>                   // bool, true, false
#include <stdint.h>                    // uint64_t
#include <stdio.h>                     // vsnprintf
#include <cwalk/include/cwalk.h>       // cwk_path_normalize, cwk_path_*
#include "flp_time.h"                  // fl_gmtime
#include "output_junit.h"              // JUnitXML

#define TIMESTAMP_SIZE   32
#define TIMESTAMP_FORMAT "%Y-%m-%dT%H:%M:%SZ" // ISO 8601 for XML content
#define JUNIT_MODULE     "JUNIT XML"

#if defined(__cplusplus)
extern "C" {
#endif

FLExceptionReason junit_open_failure = "failed to open junit file";

#if defined(__cplusplus)
}
#endif


// -- Buffered writer over the file / stream services -----------------------------------
// Neither service has a formatted-output primitive and each of their writes is a whole
// service call, so this small accumulator batches the many short XML fragments into one
// write per JUNIT_OUT_CAP bytes. junit_begin truncates/creates the file via the
// positional (offset-tracking) file service; junit_write/junit_end append to it via the
// stream service, which has no offset parameter at all.
#define JUNIT_OUT_CAP 1024

typedef enum {
    JUNIT_BACKEND_FILE,   // positional write via the file service (FL_FILE_WRITE mode)
    JUNIT_BACKEND_STREAM, // offset-less append write via the stream service
} JUnitBackend;

typedef struct JUnitOut {
    FLFile      *file;
    JUnitBackend backend;
    uint64_t offset; // next write position; used only when backend == JUNIT_BACKEND_FILE
    size_t   len;    // bytes buffered
    bool     failed; // a service write came up short
    char     buf[JUNIT_OUT_CAP];
} JUnitOut;

static void out_init(JUnitOut *out, FLFile *file, JUnitBackend backend) {
    out->file    = file;
    out->backend = backend;
    out->offset  = 0;
    out->len     = 0;
    out->failed  = false;
}

static void out_flush(JUnitOut *out) {
    if (out->len > 0) {
        size_t written;
        if (out->backend == JUNIT_BACKEND_FILE) {
            written = FL_FILE_WRITE(out->file, out->buf, out->len, out->offset);
            out->offset += written;
        } else {
            written = FL_STREAM_WRITE(out->file, out->buf, out->len);
        }
        if (written < out->len) {
            LOG_ERROR(JUNIT_MODULE, "wrote %zu bytes, expected %zu", written, out->len);
            out->failed = true;
        }
        out->len = 0;
    }
}

static void out_write(JUnitOut *out, char const *data, size_t size) {
    while (size > 0) {
        if (out->len == JUNIT_OUT_CAP) {
            out_flush(out);
        }
        size_t space = JUNIT_OUT_CAP - out->len;
        size_t chunk = size < space ? size : space;
        memcpy(out->buf + out->len, data, chunk);
        out->len += chunk;
        data += chunk;
        size -= chunk;
    }
}

static void out_puts(JUnitOut *out, char const *str) {
    out_write(out, str, strlen(str));
}

static void out_char(JUnitOut *out, char c) {
    out_write(out, &c, 1);
}

// Formatted output for bounded content (numbers, timestamps, fixed text). Unbounded
// strings (test names, failure details) go through out_puts / xml_write_escaped.
static FL_PRINTF_FORMAT(2, 3) void out_printf(JUnitOut *out, char const *fmt, ...) {
    char    tmp[512];
    va_list args;

    va_start(args, fmt);
    int needed = vsnprintf(tmp, sizeof tmp, fmt, args);
    va_end(args);

    FL_ASSERT_GT_INT(needed, -1);
    FL_ASSERT_LT_INT(needed, (int)sizeof tmp);
    out_write(out, tmp, (size_t)needed);
}

static void xml_write_escaped(JUnitOut *out, char const *str) {
    if (!str) {
        return;
    }

    for (; *str; ++str) {
        switch (*str) {
        case '&':
            out_puts(out, "&amp;");
            break;
        case '<':
            out_puts(out, "&lt;");
            break;
        case '>':
            out_puts(out, "&gt;");
            break;
        case '"':
            out_puts(out, "&quot;");
            break;
        default:
            out_char(out, *str);
            break;
        }
    }
}

static void junit_open(JUnitXML *junit, JUnitBackend backend) {
    if (junit->file != NULL) {
        // already opened
        return;
    }

    junit->file = (backend == JUNIT_BACKEND_FILE)
                      ? FL_FILE_OPEN(junit->path, FL_FILE_WRITE)
                      : FL_STREAM_OPEN(junit->path);
    if (junit->file == NULL) {
        LOG_ERROR(JUNIT_MODULE, "error opening file \"%s\"", junit->path);
        FL_THROW(junit_open_failure);
    }
}

static void junit_close(JUnitXML *junit, JUnitBackend backend) {
    FL_ASSERT_NOT_NULL(junit->file);
    if (backend == JUNIT_BACKEND_FILE) {
        FL_FILE_CLOSE(junit->file);
    } else {
        FL_STREAM_CLOSE(junit->file);
    }
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
    char  *buf      = (char *)ARENA_CALLOC_THROW(arena, 1, buf_size);
    initialize(junit, arena, path, buf, buf_size, JUNIT_PATH_ONLY);
}

JUnitXML *new_junit(Arena *arena, char const *path) {
    FL_ASSERT_NOT_NULL(path);

    size_t    norm_len = cwk_path_normalize(path, NULL, 0);
    size_t    buf_size = norm_len + 1;
    JUnitXML *junit    = (JUnitXML *)ARENA_CALLOC_THROW(arena, 1, sizeof(JUnitXML));
    char     *buf      = (char *)ARENA_CALLOC_THROW(arena, 1, buf_size);
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
        // FL_FILE_WRITE truncates on open (create-always), so a stale file from a
        // previous run is discarded without a separate delete step.
        junit_open(junit, JUNIT_BACKEND_FILE);
        JUnitOut out;
        out_init(&out, junit->file, JUNIT_BACKEND_FILE);
        out_puts(&out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        out_puts(&out, "<testsuites>\n");
        out_flush(&out);
        junit_close(junit, JUNIT_BACKEND_FILE);
        rc = out.failed ? -1 : 0;
    }

    return rc;
}

int junit_end(JUnitXML *junit) {
    int rc = 0;
    if (junit->path != NULL) {
        junit_open(junit, JUNIT_BACKEND_STREAM);
        JUnitOut out;
        out_init(&out, junit->file, JUNIT_BACKEND_STREAM);
        out_puts(&out, "</testsuites>\n");
        out_flush(&out);
        junit_close(junit, JUNIT_BACKEND_STREAM);
        rc = out.failed ? -1 : 0;
    }

    return rc;
}

int junit_write(JUnitXML *junit, FLContext *fctx) {
    int rc = 0;
    if (junit->path != NULL) {
        junit_open(junit, JUNIT_BACKEND_STREAM);
        JUnitOut out;
        out_init(&out, junit->file, JUNIT_BACKEND_STREAM);
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
        out_puts(&out, "    <testsuite name=\"");
        xml_write_escaped(&out, ts->name);
        out_puts(&out, "\"\n               classname=\"");
        xml_write_escaped(&out, ts->name);
        out_printf(&out,
                   "\"\n"
                   "               tests=\"%zu\"\n"
                   "               failures=\"%zu\"\n"
                   "               errors=\"%zu\"\n"
                   "               time=\"%.3f\"\n"
                   "               timestamp=\"%s\">\n",
                   fctx->tests_run, fctx->tests_failed,
                   fctx->setups_failed + fctx->cleanups_failed, total_elapsed_seconds,
                   timestamp);

        // Emit one <testcase> per result. JUnit consumers (the CI test reporter)
        // count individual <testcase> elements, not the testsuite's tests=
        // attribute, so a suite with only attributes is reported as "no tests
        // found". A non-FL_PASS result carries a <failure> child.
        for (size_t i = 0; i < results_count; i++) {
            FLTestSummary const *summary
                = faultline_test_summary_buffer_get(&fctx->results, i);
            char const *case_name = (summary->index < ts->count)
                                        ? ts->test_cases[summary->index]->name
                                        : "";

            out_puts(&out, "        <testcase name=\"");
            xml_write_escaped(&out, case_name);
            out_puts(&out, "\" classname=\"");
            xml_write_escaped(&out, ts->name);
            out_printf(&out, "\" time=\"%.3f\"", summary->elapsed_seconds);

            if (summary->code == FL_PASS) {
                out_puts(&out, "/>\n");
            } else {
                out_puts(&out, ">\n            <failure message=\"");
                xml_write_escaped(&out,
                                  summary->reason ? summary->reason : "test failed");
                out_printf(&out, "\" type=\"FLResultCode %d\">", (int)summary->code);
                xml_write_escaped(&out, summary->details);
                out_puts(&out, "</failure>\n        </testcase>\n");
            }
        }

        out_puts(&out, "    </testsuite>\n");
        out_flush(&out);

        junit_close(junit, JUNIT_BACKEND_STREAM);
        rc = out.failed ? -1 : 0;
    }

    return rc;
}
