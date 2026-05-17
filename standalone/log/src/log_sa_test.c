/**
 * @file log_sa_test.c
 * @author Douglas Cuthbertson
 * @brief Standalone log service test — no FaultLine infrastructure required.
 *
 * Build (from repo root):
 *
 *   cl.exe /W4 /std:c17 /experimental:c11atomics /DFL_EMBEDDED ^
 *     /I standalone\log\include ^
 *     standalone\log\src\log_sa_test.c ^
 *     /link /OUT:target\log_sa_test.exe
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty (no dllimport/export).
 *
 * Unity build: fl_threads.c, flp_log_service.c, and fla_log_service.c are
 * included directly, giving access to the static g_logger struct and the
 * static get_filename helper defined in flp_log_service.c.
 *
 * Exit code: 0 = all passed, 1 = one or more failures.
 *
 * @version 0.1
 * @date 2026-05-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include "fl_threads.c"      // polyfill — compiled out when <threads.h> is available
#include "flp_log_service.c" // platform implementation; exposes g_logger, get_filename
#include "fla_log_service.c" // application implementation

#include <faultline/fl_threads.h> // thrd_t, thrd_create, thrd_join

#include <io.h> // _dup, _dup2, _fileno, _close
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Test harness
// ---------------------------------------------------------------------------

static int g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_MSG(cond, msg)                                                            \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            fprintf(stderr, "FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #cond, (msg)); \
            g_failures++;                                                               \
        }                                                                               \
    } while (0)

#define SECTION(name) fprintf(stdout, "  %s\n", (name))

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int tmp_counter = 0;

static void init_to_tmpfile(char *path, size_t size) {
    flp_log_init();
    snprintf(path, size, "fl_log_test_%d.tmp", ++tmp_counter);
    flp_log_set_output_path(path);
    flp_log_set_level(LOG_LEVEL_TRACE);
}

static size_t read_output(char const *path, char *buf, size_t buf_size) {
    flp_log_cleanup();

    FILE   *f;
    errno_t err = fopen_s(&f, path, "rb");
    if (err != 0 || f == NULL) {
        buf[0] = '\0';
        return 0;
    }

    size_t n = fread(buf, 1, buf_size - 1, f);
    buf[n]   = '\0';
    fclose(f);
    remove(path);
    return n;
}

static size_t count_lines(char const *buf) {
    size_t n = 0;
    for (char const *p = buf; *p; p++) {
        if (*p == '\n')
            n++;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

static void test_init_defaults(void) {
    SECTION("lifecycle: init defaults");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_log_set_level(LOG_LEVEL_INFO);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "hello %s", "world");
    size_t n = read_output(path, buf, sizeof buf);
    CHECK(n > 0);
    CHECK(strstr(buf, "hello world") != NULL);
}

static void test_cleanup_flushes_owned_file(void) {
    SECTION("lifecycle: cleanup flushes owned file");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "flush check");
    size_t n = read_output(path, buf, sizeof buf);
    CHECK(n > 0);
    CHECK(strstr(buf, "flush check") != NULL);
}

static void test_cleanup_does_not_close_unowned(void) {
    SECTION("lifecycle: cleanup does not close unowned file");
    char path[256];
    snprintf(path, sizeof path, "fl_log_test_%d.tmp", ++tmp_counter);

    FILE *f;
    fopen_s(&f, path, "w");
    CHECK(f != NULL);

    flp_log_init();
    flp_log_set_output(f);
    flp_log_set_level(LOG_LEVEL_TRACE);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "unowned");
    flp_log_cleanup();

    CHECK(fprintf(f, "still open\n") > 0);
    fclose(f);
    remove(path);
}

static void test_init_cleanup_reinit(void) {
    SECTION("lifecycle: init / cleanup / reinit");
    char path[256], buf[4096];
    flp_log_init();
    flp_log_cleanup();
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "reinit works");
    size_t n = read_output(path, buf, sizeof buf);
    CHECK(n > 0);
    CHECK(strstr(buf, "reinit works") != NULL);
}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------

static void test_default_level_is_info(void) {
    SECTION("filtering: info passes, verbose suppressed");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_log_set_level(LOG_LEVEL_INFO);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "info msg");
    flp_write_log(LOG_LEVEL_VERBOSE, __FILE__, __LINE__, "test", "verbose msg");
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "info msg") != NULL);
    CHECK(strstr(buf, "verbose msg") == NULL);
}

static void test_set_level_trace_all_pass(void) {
    SECTION("filtering: trace — all 7 levels pass");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, "test", "fatal");
    flp_write_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, "test", "error");
    flp_write_log(LOG_LEVEL_WARN, __FILE__, __LINE__, "test", "warn");
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "info");
    flp_write_log(LOG_LEVEL_VERBOSE, __FILE__, __LINE__, "test", "verbose");
    flp_write_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, "test", "debug");
    flp_write_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, "test", "trace");
    read_output(path, buf, sizeof buf);
    CHECK(count_lines(buf) == 7);
}

static void test_set_level_fatal_only(void) {
    SECTION("filtering: fatal — only fatal passes");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_log_set_level(LOG_LEVEL_FATAL);
    flp_write_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, "test", "fatal");
    flp_write_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, "test", "error");
    flp_write_log(LOG_LEVEL_WARN, __FILE__, __LINE__, "test", "warn");
    read_output(path, buf, sizeof buf);
    CHECK(count_lines(buf) == 1);
}

static void test_level_boundary(void) {
    SECTION("filtering: warn boundary");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_log_set_level(LOG_LEVEL_WARN);
    flp_write_log(LOG_LEVEL_WARN, __FILE__, __LINE__, "test", "warn msg");
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "info msg");
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "warn msg") != NULL);
    CHECK(strstr(buf, "info msg") == NULL);
}

// ---------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------

static void test_set_output_null_defaults_stdout(void) {
    SECTION("routing: NULL output defaults to stdout");
    flp_log_init();
    flp_log_set_output(NULL);
    CHECK(g_logger.output == stdout);
    CHECK(g_logger.close_output == false);
    flp_log_cleanup();
}

static void test_set_output_to_file(void) {
    SECTION("routing: explicit FILE*");
    char path[256], buf[4096];
    snprintf(path, sizeof path, "fl_log_test_%d.tmp", ++tmp_counter);

    FILE *f;
    fopen_s(&f, path, "w");
    CHECK(f != NULL);

    flp_log_init();
    flp_log_set_output(f);
    flp_log_set_level(LOG_LEVEL_TRACE);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "file output");
    flp_log_cleanup();

    fclose(f);
    fopen_s(&f, path, "r");
    CHECK(f != NULL);
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n]   = '\0';
    fclose(f);
    remove(path);
    CHECK(strstr(buf, "file output") != NULL);
}

static void test_set_output_path_append_mode(void) {
    SECTION("routing: path opens in append mode");
    char path[256], buf[8192];
    snprintf(path, sizeof path, "fl_log_test_%d.tmp", ++tmp_counter);

    flp_log_init();
    flp_log_set_output_path(path);
    flp_log_set_level(LOG_LEVEL_TRACE);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "first message");
    flp_log_cleanup();

    flp_log_init();
    flp_log_set_output_path(path);
    flp_log_set_level(LOG_LEVEL_TRACE);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "second message");
    flp_log_cleanup();

    FILE *f;
    fopen_s(&f, path, "rb");
    CHECK(f != NULL);
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n]   = '\0';
    fclose(f);
    remove(path);
    CHECK(strstr(buf, "first message") != NULL);
    CHECK(strstr(buf, "second message") != NULL);
}

static void test_set_output_path_invalid_fallback(void) {
    SECTION("routing: invalid path falls back to stdout");
    flp_log_init();

    fflush(stderr);
    int   saved = _dup(_fileno(stderr));
    FILE *nul;
    freopen_s(&nul, "NUL", "w", stderr);

    flp_log_set_output_path("X:\\no_such_dir_abc123\\no_such_file.log");

    fflush(stderr);
    _dup2(saved, _fileno(stderr));
    _close(saved);

    CHECK(g_logger.output == stdout);
    CHECK(g_logger.close_output == false);
    flp_log_cleanup();
}

// ---------------------------------------------------------------------------
// Format
// ---------------------------------------------------------------------------

static void test_format_has_timestamp(void) {
    SECTION("format: timestamp [YYYY-MM-DD");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "ts");
    read_output(path, buf, sizeof buf);
    CHECK(buf[0] == '[');
    CHECK(buf[5] == '-');
    CHECK(buf[8] == '-');
}

static void test_format_level_names(void) {
    SECTION("format: level names padded to 7 chars");
    static char const *expected[]
        = {"FATAL", "ERROR", "WARN", "INFO", "VERBOSE", "DEBUG", "TRACE"};
    static FLLogLevel levels[]
        = {LOG_LEVEL_FATAL,   LOG_LEVEL_ERROR, LOG_LEVEL_WARN, LOG_LEVEL_INFO,
           LOG_LEVEL_VERBOSE, LOG_LEVEL_DEBUG, LOG_LEVEL_TRACE};

    for (int i = 0; i < 7; i++) {
        char path[256], buf[4096], label[16];
        init_to_tmpfile(path, sizeof path);
        flp_write_log(levels[i], __FILE__, __LINE__, "test", "lvl");
        read_output(path, buf, sizeof buf);
        snprintf(label, sizeof label, "[%-7s]", expected[i]);
        CHECK_MSG(strstr(buf, label) != NULL, expected[i]);
    }
}

static void test_format_thread_id(void) {
    SECTION("format: thread id [T:...");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "tid");
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "[T:") != NULL);
}

static void test_format_filename_extracted(void) {
    SECTION("format: basename extracted from path");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, "C:\\path\\to\\source.c", 42, "fn", "msg");
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "source.c: 42") != NULL);
    CHECK(strstr(buf, "C:\\path") == NULL);
}

static void test_format_id_present(void) {
    SECTION("format: component id appears in brackets");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "MYID", "msg");
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "[MYID]") != NULL);
}

static void test_format_id_absent_empty(void) {
    SECTION("format: empty id omitted");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "", "no id");
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "no id") != NULL);
    CHECK(strstr(buf, "[]") == NULL);
}

static void test_format_id_absent_null(void) {
    SECTION("format: NULL id omitted");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, NULL, "null id");
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "null id") != NULL);
    CHECK(strstr(buf, "[]") == NULL);
}

static void test_format_message_with_args(void) {
    SECTION("format: printf-style args");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "x=%d", 42);
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "x=42") != NULL);
}

// ---------------------------------------------------------------------------
// Static helpers (accessible via unity build)
// ---------------------------------------------------------------------------

static void test_get_filename(void) {
    SECTION("helpers: get_filename");
    CHECK(strcmp(get_filename("C:\\foo\\bar.c"), "bar.c") == 0);
    CHECK(strcmp(get_filename("/foo/bar.c"), "bar.c") == 0);
    CHECK(strcmp(get_filename("bar.c"), "bar.c") == 0);
    CHECK(strcmp(get_filename("C:\\foo/bar.c"), "bar.c") == 0);
}

// ---------------------------------------------------------------------------
// Service plumbing
// ---------------------------------------------------------------------------

static FLLogService g_captured_svc;

static FLA_SET_LOG_SERVICE_FN(capture_log_service) {
    if (size == sizeof(FLLogService)) {
        g_captured_svc = *svc;
    }
}

static void test_init_service_sets_write(void) {
    SECTION("service: flp_init_log_service sets write fn");
    flp_init_log_service(capture_log_service);
    CHECK(g_captured_svc.write == flp_write_log);
}

static void test_app_writes_to_platform_log(void) {
    SECTION("service: app write fn routes through platform");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_init_log_service(capture_log_service);
    g_captured_svc.write(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "svc check");
    read_output(path, buf, sizeof buf);
    CHECK(strstr(buf, "svc check") != NULL);
}

// ---------------------------------------------------------------------------
// Thread safety
// ---------------------------------------------------------------------------

typedef struct {
    int id;
} WriterArg;

static int thread_writer(void *arg) {
    WriterArg *wa = (WriterArg *)arg;
    for (int i = 0; i < 50; i++) {
        flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "thread %d msg %d",
                      wa->id, i);
    }
    return 0;
}

static void test_concurrent_writes_no_corruption(void) {
    SECTION("threads: 4 threads x 50 writes = 200 lines, no corruption");
    char      path[256], buf[65536];
    WriterArg args[4];
    thrd_t    threads[4];

    init_to_tmpfile(path, sizeof path);
    for (int i = 0; i < 4; i++) {
        args[i].id = i;
        thrd_create(&threads[i], thread_writer, &args[i]);
    }
    for (int i = 0; i < 4; i++)
        thrd_join(threads[i], NULL);

    read_output(path, buf, sizeof buf);
    CHECK(count_lines(buf) == 200);
}

static int thread_single_log(void *arg) {
    int id = *(int *)arg;
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "thread %d", id);
    return 0;
}

static void test_thread_ids_unique(void) {
    SECTION("threads: 4 threads have distinct TIDs");
    char   path[256], buf[4096];
    int    ids[4] = {0, 1, 2, 3};
    thrd_t threads[4];

    init_to_tmpfile(path, sizeof path);
    for (int i = 0; i < 4; i++)
        thrd_create(&threads[i], thread_single_log, &ids[i]);
    for (int i = 0; i < 4; i++)
        thrd_join(threads[i], NULL);

    read_output(path, buf, sizeof buf);
    CHECK(count_lines(buf) == 4);

    unsigned long tids[4] = {0};
    int           tc      = 0;
    for (char *p = buf; (p = strstr(p, "[T:")) != NULL && tc < 4; p += 3) {
        tids[tc++] = strtoul(p + 3, NULL, 10);
    }
    CHECK(tc == 4);
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            CHECK_MSG(tids[i] != tids[j], "duplicate TID");
        }
    }
}

static void test_thread_id_stable(void) {
    SECTION("threads: same thread produces same TID across writes");
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "s1");
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "s2");
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "s3");
    read_output(path, buf, sizeof buf);
    CHECK(count_lines(buf) == 3);

    unsigned long tids[3] = {0};
    int           tc      = 0;
    for (char *p = buf; (p = strstr(p, "[T:")) != NULL && tc < 3; p += 3) {
        tids[tc++] = strtoul(p + 3, NULL, 10);
    }
    CHECK(tc == 3);
    CHECK(tids[0] == tids[1]);
    CHECK(tids[1] == tids[2]);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    fprintf(stdout, "Standalone log service tests\n");

    fprintf(stdout, "Lifecycle:\n");
    test_init_defaults();
    test_cleanup_flushes_owned_file();
    test_cleanup_does_not_close_unowned();
    test_init_cleanup_reinit();

    fprintf(stdout, "Filtering:\n");
    test_default_level_is_info();
    test_set_level_trace_all_pass();
    test_set_level_fatal_only();
    test_level_boundary();

    fprintf(stdout, "Routing:\n");
    test_set_output_null_defaults_stdout();
    test_set_output_to_file();
    test_set_output_path_append_mode();
    test_set_output_path_invalid_fallback();

    fprintf(stdout, "Format:\n");
    test_format_has_timestamp();
    test_format_level_names();
    test_format_thread_id();
    test_format_filename_extracted();
    test_format_id_present();
    test_format_id_absent_empty();
    test_format_id_absent_null();
    test_format_message_with_args();

    fprintf(stdout, "Helpers:\n");
    test_get_filename();

    fprintf(stdout, "Service plumbing:\n");
    test_init_service_sets_write();
    test_app_writes_to_platform_log();

    fprintf(stdout, "Thread safety:\n");
    test_concurrent_writes_no_corruption();
    test_thread_ids_unique();
    test_thread_id_stable();

    if (g_failures == 0) {
        fprintf(stdout, "\nAll tests passed.\n");
    } else {
        fprintf(stdout, "\n%d test(s) FAILED.\n", g_failures);
    }
    return g_failures > 0 ? 1 : 0;
}
