/**
 * @file fl_log_service_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the log service implementation.
 * @version 0.1
 * @date 2026-02-11
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * This test suite verifies the platform-side log service (flp_log_service.c)
 * covering lifecycle, level filtering, output routing, output format,
 * static helpers, service plumbing, and thread safety.
 *
 * The driver injects an FLExceptionService via fla_set_exception_service().
 * FL_ASSERT macros throw exceptions caught by but_driver.exe.
 */

// Unity build: include implementation files directly
// Order matters: exception service first, then code under test
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_* macros
#include <faultline/fl_test.h> // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h> // FL_TRY, FL_CATCH, FL_THROW (resolves to FLA_* in DLL builds)

#include "fl_exception_service.c"  // exception reason constants
#include "fla_exception_service.c" // TLS exception service (app-side)
#include "flp_log_service.c"       // code under test (platform log service)

#include <io.h> // _dup, _dup2, _fileno, _close

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

static int tmp_counter = 0;

/**
 * @brief Initialize the logger with output directed to a temp file.
 * Sets log level to LOG_LEVEL_TRACE so all messages are captured.
 */
static void init_to_tmpfile(char *path, size_t size) {
    flp_log_init();
    snprintf(path, size, "fl_log_test_%d.tmp", ++tmp_counter);
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
    FL_ASSERT_EQ_PTR((void *)g_logger.output, (void *)stdout);
    FL_ASSERT_FALSE(g_logger.close_output);
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

    // Suppress expected stderr error from flp_log_set_output_path
    fflush(stderr);
    int   saved_stderr = _dup(_fileno(stderr));
    FILE *devnull;
    freopen_s(&devnull, "NUL", "w", stderr);

    flp_log_set_output_path("X:\\no_such_dir_abc123\\no_such_file.log");

    // Restore stderr
    fflush(stderr);
    _dup2(saved_stderr, _fileno(stderr));
    _close(saved_stderr);

    FL_ASSERT_EQ_PTR((void *)g_logger.output, (void *)stdout);
    FL_ASSERT_FALSE(g_logger.close_output);
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
    flp_write_log(LOG_LEVEL_INFO, "C:\\path\\to\\source.c", 42, "my_func", "test",
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
// Suite registration
// ---------------------------------------------------------------------------

FL_SUITE_BEGIN(LogService)
FL_SUITE_ADD(init_defaults)
FL_SUITE_ADD(cleanup_flushes_owned_file)
FL_SUITE_ADD(cleanup_does_not_close_unowned)
FL_SUITE_ADD(init_cleanup_reinit)
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
FL_SUITE_ADD(concurrent_writes_no_corruption)
FL_SUITE_ADD(thread_ids_unique)
FL_SUITE_ADD(thread_id_stable)
FL_SUITE_END;

FL_GET_TEST_SUITE("Log Service", LogService);
