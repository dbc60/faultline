/**
 * @file log_consumer_test.c
 * @author Douglas Cuthbertson
 * @brief Consumer-side test for the application log service, compiled against an
 *        *imported* `log` package (not the repo's src/ tree).
 *
 * This is the recovered standalone log test. It is the only coverage of the
 * application side of the log service: the FaultLine driver always injects the
 * log service, so the default (un-injected) write stub and level threshold can
 * only be exercised here, outside the driver.
 *
 * Unity build: fla_log_service.c is included directly so its static
 * default_write and s_level are in the same translation unit. The package
 * places it at <root>/src/fla_log_service.c, so the build adds <root>/src to
 * the include path and this quoted include resolves there.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty (no dllimport on a locally-defined
 * function). Use /DDLL_BUILD instead when compiling fla_log_service.c into a DLL.
 *
 * Exit code: 0 = all passed, 1 = one or more failures.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include "fla_log_service.c"

#include <faultline/fla_log_service.h>
#include <faultline/fl_log_service.h>
#include <faultline/fl_log.h> // LOG_* (selects fla_ backend; no FL_BUILD_DRIVER)

#include <stdarg.h>
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
// Capture write function
// ---------------------------------------------------------------------------

static int         g_call_count;
static FLLogLevel  g_last_level;
static int         g_last_line;
static char const *g_last_file;
static char        g_last_id[64];
static char        g_last_msg[256];

static FL_WRITE_LOG_FN(capture_write) {
    g_call_count++;
    g_last_level = level;
    g_last_file  = file;
    g_last_line  = line;
    strncpy_s(g_last_id, sizeof g_last_id, id ? id : "", _TRUNCATE);
    va_list args;
    va_start(args, format);
    vsnprintf(g_last_msg, sizeof g_last_msg, format, args);
    va_end(args);
}

static void reset_capture(void) {
    g_call_count  = 0;
    g_last_level  = LOG_LEVEL_FATAL;
    g_last_file   = NULL;
    g_last_line   = 0;
    g_last_id[0]  = '\0';
    g_last_msg[0] = '\0';
}

static void install_capture(void) {
    FLLogService svc = {.write = capture_write};
    fla_set_log_service(&svc, sizeof svc);
    reset_capture();
}

// ---------------------------------------------------------------------------
// 1. Default service
// ---------------------------------------------------------------------------

static void test_default_write_non_null(void) {
    SECTION("default: write function is non-NULL");
    CHECK(g_fla_log_service.write != NULL);
}

// ---------------------------------------------------------------------------
// 2. Service injection
// ---------------------------------------------------------------------------

static void test_set_service_installs_write(void) {
    SECTION("fla_set_log_service: replaces write fn");
    install_capture();
    CHECK(g_fla_log_service.write == capture_write);
}

static void test_set_service_write_is_called(void) {
    SECTION("fla_set_log_service: LOG_INFO routes to installed fn");
    install_capture();
    LOG_INFO("t", "hello");
    CHECK(g_call_count == 1);
}

// ---------------------------------------------------------------------------
// 3. LOG_* macro routing
// ---------------------------------------------------------------------------

static void test_log_macro_levels(void) {
    SECTION("LOG_* macros: each passes the correct FLLogLevel");
    install_capture();

    static const struct {
        FLLogLevel  level;
        char const *name;
    } cases[] = {
        {LOG_LEVEL_FATAL, "FATAL"},     {LOG_LEVEL_ERROR, "ERROR"},
        {LOG_LEVEL_WARN, "WARN"},       {LOG_LEVEL_INFO, "INFO"},
        {LOG_LEVEL_VERBOSE, "VERBOSE"}, {LOG_LEVEL_DEBUG, "DEBUG"},
        {LOG_LEVEL_TRACE, "TRACE"},
    };

    reset_capture();
    LOG_FATAL("t", "m");
    CHECK_MSG(g_last_level == LOG_LEVEL_FATAL, cases[0].name);
    reset_capture();
    LOG_ERROR("t", "m");
    CHECK_MSG(g_last_level == LOG_LEVEL_ERROR, cases[1].name);
    reset_capture();
    LOG_WARN("t", "m");
    CHECK_MSG(g_last_level == LOG_LEVEL_WARN, cases[2].name);
    reset_capture();
    LOG_INFO("t", "m");
    CHECK_MSG(g_last_level == LOG_LEVEL_INFO, cases[3].name);
    reset_capture();
    LOG_VERBOSE("t", "m");
    CHECK_MSG(g_last_level == LOG_LEVEL_VERBOSE, cases[4].name);
    reset_capture();
    LOG_DEBUG("t", "m");
    CHECK_MSG(g_last_level == LOG_LEVEL_DEBUG, cases[5].name);
    reset_capture();
    LOG_TRACE("t", "m");
    CHECK_MSG(g_last_level == LOG_LEVEL_TRACE, cases[6].name);
}

static void test_log_macro_id_and_format(void) {
    SECTION("LOG_INFO: id and printf-style args forwarded");
    install_capture();
    LOG_INFO("MYCOMP", "x=%d y=%s", 7, "hello");
    CHECK(strcmp(g_last_id, "MYCOMP") == 0);
    CHECK(strcmp(g_last_msg, "x=7 y=hello") == 0);
}

static void test_log_macro_file_line_variants(void) {
    SECTION("LOG_*_FILE_LINE: explicit file and line forwarded");
    install_capture();
    LOG_WARN_FILE_LINE("t", "other.c", 99, "msg");
    CHECK(g_last_file != NULL && strcmp(g_last_file, "other.c") == 0);
    CHECK(g_last_line == 99);
    CHECK(g_last_level == LOG_LEVEL_WARN);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    fprintf(stdout, "log_consumer_test (imported package)\n");

    fprintf(stdout, "Default service:\n");
    test_default_write_non_null();

    fprintf(stdout, "Service injection:\n");
    test_set_service_installs_write();
    test_set_service_write_is_called();

    fprintf(stdout, "LOG_* macro routing:\n");
    test_log_macro_levels();
    test_log_macro_id_and_format();
    test_log_macro_file_line_variants();

    if (g_failures == 0) {
        fprintf(stdout, "PASS (%d tests)\n", 6);
        return 0;
    } else {
        fprintf(stdout, "FAIL (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
}
