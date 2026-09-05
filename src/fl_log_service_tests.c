/**
 * @file fl_log_service_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the log service implementation.
 * @version 0.2
 * @date 2026-02-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This test suite verifies the platform-side log service (flp_log_service.c)
 * covering lifecycle, level filtering, output routing, output format,
 * static helpers, service plumbing, and thread safety. Path-based output now
 * routes through the stream service (flp_stream_service.c), so this suite also
 * pulls in its memory-service backing (FL_MALLOC/FL_FREE), mirroring the same
 * unity include pattern flp_stream_service_tests.c uses.
 *
 * Exceptions are this module's own: it compiles fl_exception.c.
 * FL_ASSERT macros throw exceptions caught by but_driver.exe.
 */

// Unity build: include implementation files directly
// Order matters: exception service first, then code under test
#include <faultline/fl_exception_assert.h> // FL_ASSERT_* macros
#include <faultline/fl_macros.h>           // FL_ANALYSIS_SUPPRESS
#include <faultline/fl_threads.h>          // thrd_t, thrd_create, thrd_join
#include <faultline/fl_test.h>             // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h> // FL_TRY, FL_CATCH, FL_THROW (resolves to FLA_* in DLL builds)

#include "fl_exception.c"       // exception reasons, push/pop/throw
#include "fla_memory_service.c" // g_fla_memory_service (FL_MALLOC backing)
#include "flp_stream_service.c" // flp_log_service.c's append/console output path
#include "lock_os.c"            // FLLock backend for flp_log_service.c
#include "flp_log_service.c"    // code under test (platform log service)
#include "fla_log_service.c"    // code under test (application log service)
#include "fla_timer_service.c"  // g_fla_timer_service, fla_set_timer_service

#include <io.h> // _dup, _dup2, _close, _fileno (stderr suppression)

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

static int tmp_counter = 0;

/**
 * @brief Initialize the logger with output directed to a temp file.
 * Sets log level to LOG_LEVEL_TRACE so all messages are captured.
 *
 * The name is built from a process-local counter, so a given test always gets
 * the same one, and the log service opens its output append-mode. Removing the
 * file first is what makes a test independent of whatever the previous run left
 * behind: a run that dies mid-test, or whose remove() loses a race with a
 * virus scanner holding the handle, would otherwise leave content that the next
 * run appends to -- surfacing the failure in a later run than the one that
 * caused it.
 */
static void init_to_tmpfile(char *path, size_t size) {
    flp_log_init();
    snprintf(path, size, "fl_log_test_%d.tmp", ++tmp_counter);
    remove(path);
    flp_log_set_output_path(path);
    flp_log_set_level(LOG_LEVEL_TRACE);
}

/**
 * @brief Flush and close the logger, read the temp file contents into buf,
 * delete the temp file, and return the number of bytes read.
 */
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

/**
 * @brief Count newline-terminated lines in a buffer.
 */
static size_t count_lines(char const *buf) {
    size_t count = 0;
    for (char const *p = buf; *p; p++) {
        if (*p == '\n') {
            count++;
        }
    }
    return count;
}

/**
 * @brief Redirect stderr to NUL for a block expected to emit a diagnostic
 * (e.g. the invalid-path fallback warning). Saves and returns the original
 * stderr fd; pass it to restore_stderr() to undo. Saving the underlying fd
 * keeps any harness-level redirection of stderr intact.
 */
static int suppress_stderr(void) {
    fflush(stderr);
    int   saved = _dup(_fileno(stderr));
    FILE *devnull;
    freopen_s(&devnull, "NUL", "w", stderr);
    return saved;
}

/**
 * @brief Restore stderr previously redirected by suppress_stderr().
 */
static void restore_stderr(int saved) {
    fflush(stderr);
    int restored = _dup2(saved, _fileno(stderr));
    _close(saved);
    FL_ASSERT_TRUE(restored != -1);
}

// ---------------------------------------------------------------------------
// Lifecycle tests
// ---------------------------------------------------------------------------

FL_TEST("Init Defaults", init_defaults) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_log_set_level(LOG_LEVEL_INFO);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "hello %s", "world");
    size_t n = read_output(path, buf, sizeof buf);
    FL_ASSERT_GT_SIZE_T(n, (size_t)0);
    FL_ASSERT_TRUE(strstr(buf, "hello world") != NULL);
}

FL_TEST("Cleanup Flushes Owned File", cleanup_flushes_owned_file) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "flush check");
    size_t n = read_output(path, buf, sizeof buf);
    FL_ASSERT_GT_SIZE_T(n, (size_t)0);
    FL_ASSERT_TRUE(strstr(buf, "flush check") != NULL);
}

FL_TEST("Cleanup Does Not Close Unowned", cleanup_does_not_close_unowned) {
    char path[256];
    snprintf(path, sizeof path, "fl_log_test_%d.tmp", ++tmp_counter);

    FILE *f;
    fopen_s(&f, path, "w");
    FL_ASSERT_NOT_NULL(f);

    flp_log_init();
    flp_log_set_output(f);
    flp_log_set_level(LOG_LEVEL_TRACE);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "unowned");
    flp_log_cleanup();

    // f should still be writable after cleanup
    int result = fprintf(f, "still open\n");
    FL_ASSERT_GT_INT(result, 0);
    fclose(f);
    remove(path);
}

FL_TEST("Init Cleanup Reinit", init_cleanup_reinit) {
    char path[256], buf[4096];

    flp_log_init();
    flp_log_cleanup();

    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "reinit works");
    size_t n = read_output(path, buf, sizeof buf);
    FL_ASSERT_GT_SIZE_T(n, (size_t)0);
    FL_ASSERT_TRUE(strstr(buf, "reinit works") != NULL);
}

FL_TEST("Init Custom With Path", init_custom_with_path) {
    char path[256], buf[4096];
    snprintf(path, sizeof path, "fl_log_test_%d.tmp", ++tmp_counter);

    // init_custom with a non-NULL path opens the file itself and takes ownership.
    flp_log_init_custom(LOG_LEVEL_TRACE, path);
    FL_ASSERT_TRUE(g_logger.output_kind == FLP_LOG_OUTPUT_STREAM);
    FL_ASSERT_EQ_INT((int)g_logger.max_level, (int)LOG_LEVEL_TRACE);

    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "custom path msg");
    size_t n = read_output(path, buf, sizeof buf);
    FL_ASSERT_GT_SIZE_T(n, (size_t)0);
    FL_ASSERT_TRUE(strstr(buf, "custom path msg") != NULL);
}

FL_TEST("Init Custom Null Path Stdout", init_custom_null_path_stdout) {
    // A NULL path routes to stdout (unowned) and honors the requested level.
    flp_log_init_custom(LOG_LEVEL_WARN, NULL);
    FL_ASSERT_TRUE(g_logger.output_kind == FLP_LOG_OUTPUT_STDIO);
    FL_ASSERT_EQ_PTR((void *)g_logger.stdio_output, (void *)stdout);
    FL_ASSERT_EQ_INT((int)g_logger.max_level, (int)LOG_LEVEL_WARN);
    flp_log_cleanup();
}

FL_TEST("Init Is Idempotent", init_is_idempotent) {
    // The first init wins; a second init while still initialized is a no-op
    // and must not overwrite the configured level.
    flp_log_init_custom(LOG_LEVEL_WARN, NULL);
    flp_log_init_custom(LOG_LEVEL_TRACE, NULL);
    FL_ASSERT_EQ_INT((int)g_logger.max_level, (int)LOG_LEVEL_WARN);
    flp_log_cleanup();
}

// ---------------------------------------------------------------------------
// Filtering tests
// ---------------------------------------------------------------------------

FL_TEST("Default Level Is Info", default_level_is_info) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_log_set_level(LOG_LEVEL_INFO);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "info msg");
    flp_write_log(LOG_LEVEL_VERBOSE, __FILE__, __LINE__, "test", "verbose msg");
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "info msg") != NULL);
    FL_ASSERT_TRUE(strstr(buf, "verbose msg") == NULL);
}

FL_TEST("Set Level Trace All Pass", set_level_trace_all_pass) {
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
    FL_ASSERT_EQ_SIZE_T(count_lines(buf), (size_t)7);
}

FL_TEST("Set Level Fatal Only", set_level_fatal_only) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_log_set_level(LOG_LEVEL_FATAL);
    flp_write_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, "test", "fatal");
    flp_write_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, "test", "error");
    flp_write_log(LOG_LEVEL_WARN, __FILE__, __LINE__, "test", "warn");
    read_output(path, buf, sizeof buf);
    FL_ASSERT_EQ_SIZE_T(count_lines(buf), (size_t)1);
}

FL_TEST("Level Boundary", level_boundary) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_log_set_level(LOG_LEVEL_WARN);
    flp_write_log(LOG_LEVEL_WARN, __FILE__, __LINE__, "test", "warn msg");
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "info msg");
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "warn msg") != NULL);
    FL_ASSERT_TRUE(strstr(buf, "info msg") == NULL);
}

// ---------------------------------------------------------------------------
// Routing tests
// ---------------------------------------------------------------------------

FL_TEST("Set Output NULL Defaults Stdout", set_output_null_defaults_stdout) {
    flp_log_init();
    flp_log_set_output(NULL);
    FL_ASSERT_TRUE(g_logger.output_kind == FLP_LOG_OUTPUT_STDIO);
    FL_ASSERT_EQ_PTR((void *)g_logger.stdio_output, (void *)stdout);
    flp_log_cleanup();
}

FL_TEST("Set Output To File", set_output_to_file) {
    char path[256], buf[4096];
    snprintf(path, sizeof path, "fl_log_test_%d.tmp", ++tmp_counter);

    FILE *f;
    fopen_s(&f, path, "w");
    FL_ASSERT_NOT_NULL(f);

    flp_log_init();
    flp_log_set_output(f);
    flp_log_set_level(LOG_LEVEL_TRACE);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "file output");
    flp_log_cleanup();

    fclose(f);
    fopen_s(&f, path, "r");
    FL_ASSERT_NOT_NULL(f);
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n]   = '\0';
    fclose(f);
    remove(path);

    FL_ASSERT_TRUE(strstr(buf, "file output") != NULL);
}

FL_TEST("Set Output Path Append Mode", set_output_path_append_mode) {
    char path[256], buf[8192];
    snprintf(path, sizeof path, "fl_log_test_%d.tmp", ++tmp_counter);

    // First cycle
    flp_log_init();
    flp_log_set_output_path(path);
    flp_log_set_level(LOG_LEVEL_TRACE);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "first message");
    flp_log_cleanup();

    // Second cycle (same path, should append)
    flp_log_init();
    flp_log_set_output_path(path);
    flp_log_set_level(LOG_LEVEL_TRACE);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "second message");
    flp_log_cleanup();

    FILE *f;
    fopen_s(&f, path, "rb");
    FL_ASSERT_NOT_NULL(f);
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n]   = '\0';
    fclose(f);
    remove(path);

    FL_ASSERT_TRUE(strstr(buf, "first message") != NULL);
    FL_ASSERT_TRUE(strstr(buf, "second message") != NULL);
}

FL_TEST("Set Output Path Invalid Fallback", set_output_path_invalid_fallback) {
    flp_log_init();
    // The invalid path triggers an expected "falling back to stdout" warning on
    // stderr; silence it so it doesn't clutter the test run output.
    int saved = suppress_stderr();
    flp_log_set_output_path("X:\\no_such_dir_abc123\\no_such_file.log");
    restore_stderr(saved);
    FL_ASSERT_TRUE(g_logger.output_kind == FLP_LOG_OUTPUT_STDIO);
    FL_ASSERT_EQ_PTR((void *)g_logger.stdio_output, (void *)stdout);
    flp_log_cleanup();
}

// ---------------------------------------------------------------------------
// Format tests
// ---------------------------------------------------------------------------

FL_TEST("Format Has Timestamp", format_has_timestamp) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "timestamp check");
    read_output(path, buf, sizeof buf);
    // Output starts with [YYYY-MM-DD
    FL_ASSERT_TRUE(buf[0] == '[');
    FL_ASSERT_TRUE(buf[5] == '-');
    FL_ASSERT_TRUE(buf[8] == '-');
}

FL_TEST("Format Level Names", format_level_names) {
    char const *expected[]
        = {"FATAL", "ERROR", "WARN", "INFO", "VERBOSE", "DEBUG", "TRACE"};
    FLLogLevel levels[]
        = {LOG_LEVEL_FATAL,   LOG_LEVEL_ERROR, LOG_LEVEL_WARN, LOG_LEVEL_INFO,
           LOG_LEVEL_VERBOSE, LOG_LEVEL_DEBUG, LOG_LEVEL_TRACE};

    for (int i = 0; i < 7; i++) {
        char path[256], buf[4096];
        init_to_tmpfile(path, sizeof path);
        flp_write_log(levels[i], __FILE__, __LINE__, "test", "level check");
        read_output(path, buf, sizeof buf);

        char label[16];
        snprintf(label, sizeof label, "[%-7s]", expected[i]);
        FL_ASSERT_TRUE(strstr(buf, label) != NULL);
    }
}

FL_TEST("Format Thread Id", format_thread_id) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "tid check");
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "[T:") != NULL);
}

FL_TEST("Format Filename Extracted", format_filename_extracted) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, "C:\\path\\to\\source.c", 42, "my_func",
                  "filename check");
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "source.c: 42") != NULL);
    FL_ASSERT_TRUE(strstr(buf, "C:\\path") == NULL);
}

FL_TEST("Format Id Present", format_id_present) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "MYID", "id check");
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "[MYID]") != NULL);
}

FL_TEST("Format Id Absent Empty", format_id_absent_empty) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "", "no id check");
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "no id check") != NULL);
    FL_ASSERT_TRUE(strstr(buf, "[]") == NULL);
}

FL_TEST("Format Id Absent NULL", format_id_absent_null) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, NULL, "null id check");
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "null id check") != NULL);
    FL_ASSERT_TRUE(strstr(buf, "[]") == NULL);
}

FL_TEST("Format Message With Args", format_message_with_args) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "x=%d", 42);
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "x=42") != NULL);
}

// ---------------------------------------------------------------------------
// Static helper tests (get_filename is static in flp_log_service.c,
// accessible here via unity build)
// ---------------------------------------------------------------------------

FL_TEST("Get Filename Backslash", get_filename_backslash) {
    char const *result = get_filename("C:\\foo\\bar.c");
    FL_ASSERT_STR_EQ("bar.c", result);
}

FL_TEST("Get Filename Forward Slash", get_filename_forward_slash) {
    char const *result = get_filename("/foo/bar.c");
    FL_ASSERT_STR_EQ("bar.c", result);
}

FL_TEST("Get Filename No Path", get_filename_no_path) {
    char const *result = get_filename("bar.c");
    FL_ASSERT_STR_EQ("bar.c", result);
}

FL_TEST("Get Filename Mixed Slashes", get_filename_mixed_slashes) {
    char const *result = get_filename("C:\\foo/bar.c");
    FL_ASSERT_STR_EQ("bar.c", result);
}

// ---------------------------------------------------------------------------
// Service plumbing tests
// ---------------------------------------------------------------------------

static FLLogService test_svc;

FLA_SET_LOG_SERVICE_FN(set_log_service) {
    if (size != sizeof(FLLogService)) {
        FL_THROW(fl_internal_error);
    }
    test_svc = *svc;
}

// Verify that flp_init_log_service sets the write function
FL_TEST("Init Service Sets Write", init_service_sets_write) {
    flp_init_log_service(set_log_service);
    FL_ASSERT_TRUE(test_svc.write == flp_write_log);
}

// Verify the test service writes to the file specified by the platform service
FL_TEST("App Writes to Platform Log", app_writes_to_platform_log) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);
    flp_init_log_service(set_log_service);
    test_svc.write(LOG_LEVEL_TRACE, __FILE__, __LINE__, "test", "noop check to %s",
                   path);
    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "noop check") != NULL);
}

// --------------------------------------------------------------------------------------
// Application-side service tests (fla_log_service.c)
//
// These exercise the real g_fla_log_service and fla_set_log_service, as opposed to the
// local test_svc / set_log_service stub used above. Note the driver injects a real log
// service into the DLL at load time, so g_fla_log_service is already wired before any
// test runs; the initial default_write stub and the size-mismatch abort branch of
// fla_set_log_service are therefore not reachable here and are intentionally left
// uncovered.
// --------------------------------------------------------------------------------------

static int        recording_write_called = 0;
static FLLogLevel recording_write_level  = LOG_LEVEL_FATAL;

static FL_WRITE_LOG_FN(recording_write) {
    FL_UNUSED(file);
    FL_UNUSED(line);
    FL_UNUSED(id);
    FL_UNUSED(format);
    recording_write_called++;
    recording_write_level = level;
}

// fla_set_log_service installs the provided write pointer, and calls through
// g_fla_log_service route to it with the arguments intact.
FL_TEST("App Set Installs Write", app_set_installs_write) {
    fl_write_log_fn *saved = g_fla_log_service.write;

    FLLogService svc = {.write = recording_write};
    fla_set_log_service(&svc, sizeof svc);
    FL_ASSERT_TRUE(g_fla_log_service.write == recording_write);

    recording_write_called = 0;
    recording_write_level  = LOG_LEVEL_FATAL;
    g_fla_log_service.write(LOG_LEVEL_WARN, __FILE__, __LINE__, "test", "routed %d", 1);
    FL_ASSERT_EQ_INT(recording_write_called, 1);
    FL_ASSERT_EQ_INT((int)recording_write_level, (int)LOG_LEVEL_WARN);

    g_fla_log_service.write = saved;
}

// The application service can be wired to the real platform writer, so an app-side write
// is directed to the platform logger's output file.
FL_TEST("App Set Routes To Platform", app_set_routes_to_platform) {
    fl_write_log_fn *saved = g_fla_log_service.write;
    char             path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);

    FLLogService svc = {.write = flp_write_log};
    fla_set_log_service(&svc, sizeof svc);
    g_fla_log_service.write(LOG_LEVEL_INFO, __FILE__, __LINE__, "test",
                            "via app service");

    read_output(path, buf, sizeof buf);
    FL_ASSERT_TRUE(strstr(buf, "via app service") != NULL);

    g_fla_log_service.write = saved;
}

// ---------------------------------------------------------------------------
// Thread safety tests
// ---------------------------------------------------------------------------

typedef struct {
    int thread_num;
} ThreadWriterArg;

static int thread_writer(void *arg) {
    ThreadWriterArg *ta = (ThreadWriterArg *)arg;
    for (int i = 0; i < 50; i++) {
        flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "thread %d msg %d",
                      ta->thread_num, i);
    }
    return 0;
}

// 6262: 200 log lines need a larger buffer than the per-frame stack budget /analyze
// allows, and 64 KB of the 1 MB default stack is not a risk. The heap is not an
// alternative here: malloc in this translation unit is the injectable memory service,
// so a heap buffer would make this scaffolding a fault-injection site.
FL_ANALYSIS_SUPPRESS(6262)

FL_TEST("Concurrent Writes No Corruption", concurrent_writes_no_corruption) {
    char path[256];
    char buf[65536];
    init_to_tmpfile(path, sizeof path);

    thrd_t          threads[4];
    ThreadWriterArg args[4];
    for (int i = 0; i < 4; i++) {
        args[i].thread_num = i;
        thrd_create(&threads[i], thread_writer, &args[i]);
    }
    for (int i = 0; i < 4; i++) {
        thrd_join(threads[i], NULL);
    }

    read_output(path, buf, sizeof buf);
    FL_ASSERT_EQ_SIZE_T(count_lines(buf), (size_t)200);
}

static int thread_single_log(void *arg) {
    int id = *(int *)arg;
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "thread %d", id);
    return 0;
}

FL_TEST("Thread Ids Unique", thread_ids_unique) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);

    thrd_t threads[4];
    int    ids[4] = {0, 1, 2, 3};
    for (int i = 0; i < 4; i++) {
        thrd_create(&threads[i], thread_single_log, &ids[i]);
    }
    for (int i = 0; i < 4; i++) {
        thrd_join(threads[i], NULL);
    }

    read_output(path, buf, sizeof buf);
    FL_ASSERT_EQ_SIZE_T(count_lines(buf), (size_t)4);

    unsigned long tids[4]   = {0};
    int           tid_count = 0;
    char         *p         = buf;
    while ((p = strstr(p, "[T:")) != NULL) {
        p += 3;
        tids[tid_count++] = strtoul(p, NULL, 10);
        if (tid_count >= 4) {
            break;
        }
    }
    FL_ASSERT_EQ_INT(tid_count, 4);

    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            FL_ASSERT_NEQ_UINT32(tids[i], tids[j]);
        }
    }
}

FL_TEST("Thread Id Stable", thread_id_stable) {
    char path[256], buf[4096];
    init_to_tmpfile(path, sizeof path);

    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "stable1");
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "stable2");
    flp_write_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "test", "stable3");

    read_output(path, buf, sizeof buf);
    FL_ASSERT_EQ_SIZE_T(count_lines(buf), (size_t)3);

    unsigned long tids[3]   = {0};
    int           tid_count = 0;
    char         *p         = buf;
    while ((p = strstr(p, "[T:")) != NULL) {
        p += 3;
        tids[tid_count++] = strtoul(p, NULL, 10);
        if (tid_count >= 3) {
            break;
        }
    }
    FL_ASSERT_EQ_INT(tid_count, 3);
    FL_ASSERT_EQ_UINT32(tids[0], tids[1]);
    FL_ASSERT_EQ_UINT32(tids[1], tids[2]);
}

// ---------------------------------------------------------------------------
// Cross-boundary injection
//
// The plumbing tests above install a local stub or save/restore
// g_fla_log_service.write themselves. This one relies entirely on the driver: when
// it loads this DLL it resolves the exported fla_set_log_service via GetProcAddress
// and installs the host's log service into g_fla_log_service before any test runs.
// A pass proves the symbol was exported and the service crossed the DLL boundary;
// were it missing,
// g_fla_log_service would still hold the default_write abort stub (static in the
// included fla_log_service.c). The host installs its own flp_write_log -- a
// different address from this DLL's copy -- so we assert the stub was replaced
// rather than pointer identity, then route a write through the injected service.
//
// Registered first so it observes the driver's injection before the save/restore
// plumbing tests touch g_fla_log_service.
// ---------------------------------------------------------------------------

FL_TEST("Driver Injects Log Service", driver_injects_log_service) {
    FL_ASSERT_TRUE(g_fla_log_service.write != default_write);

    // Exercise the injected writer; if it were still the abort stub this would
    // terminate the process. TRACE keeps it out of the default-level output.
    g_fla_log_service.write(LOG_LEVEL_TRACE, __FILE__, __LINE__, "test",
                            "cross-boundary log via injected service");
}

// ---------------------------------------------------------------------------
// Suite registration
// ---------------------------------------------------------------------------

FL_SUITE_BEGIN(LogService)
FL_SUITE_ADD(driver_injects_log_service)
FL_SUITE_ADD(init_defaults)
FL_SUITE_ADD(cleanup_flushes_owned_file)
FL_SUITE_ADD(cleanup_does_not_close_unowned)
FL_SUITE_ADD(init_cleanup_reinit)
FL_SUITE_ADD(init_custom_with_path)
FL_SUITE_ADD(init_custom_null_path_stdout)
FL_SUITE_ADD(init_is_idempotent)
FL_SUITE_ADD(default_level_is_info)
FL_SUITE_ADD(set_level_trace_all_pass)
FL_SUITE_ADD(set_level_fatal_only)
FL_SUITE_ADD(level_boundary)
FL_SUITE_ADD(set_output_null_defaults_stdout)
FL_SUITE_ADD(set_output_to_file)
FL_SUITE_ADD(set_output_path_append_mode)
FL_SUITE_ADD(set_output_path_invalid_fallback)
FL_SUITE_ADD(format_has_timestamp)
FL_SUITE_ADD(format_level_names)
FL_SUITE_ADD(format_thread_id)
FL_SUITE_ADD(format_filename_extracted)
FL_SUITE_ADD(format_id_present)
FL_SUITE_ADD(format_id_absent_empty)
FL_SUITE_ADD(format_id_absent_null)
FL_SUITE_ADD(format_message_with_args)
FL_SUITE_ADD(get_filename_backslash)
FL_SUITE_ADD(get_filename_forward_slash)
FL_SUITE_ADD(get_filename_no_path)
FL_SUITE_ADD(get_filename_mixed_slashes)
FL_SUITE_ADD(init_service_sets_write)
FL_SUITE_ADD(app_writes_to_platform_log)
FL_SUITE_ADD(app_set_installs_write)
FL_SUITE_ADD(app_set_routes_to_platform)
FL_SUITE_ADD(concurrent_writes_no_corruption)
FL_SUITE_ADD(thread_ids_unique)
FL_SUITE_ADD(thread_id_stable)
FL_SUITE_END;

FL_GET_TEST_SUITE("Log Service", LogService);
