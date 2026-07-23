/**
 * @file flp_stream_service_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the stream service implementation.
 * @version 0.1
 * @date 2026-07-22
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Verifies the platform-side stream service (flp_stream_service.c): service plumbing,
 * append-write round-trips (no offset parameter), open-failure handling, and the
 * console accessor's no-op close / process-std-handle redirection.
 *
 * The driver injects an FLStreamService via fla_set_stream_service() when it loads this
 * DLL, so the cross-boundary test observes the host's install while the round-trip
 * tests exercise the provider directly. The driver also injects an FLExceptionService,
 * so FL_ASSERT macros throw exceptions caught by the driver. flp_file_service.c is
 * included only for read-back verification -- the stream contract has no read.
 */

#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_* macros
#include <faultline/fl_test.h> // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h>  // FL_THROW (resolves to FLA_* in DLL builds)

#include "fl_exception_service.c"  // exception reason constants
#include "fla_exception_service.c" // TLS exception service (consumer side)
#include "fla_memory_service.c"    // g_fla_memory_service (FL_MALLOC backing)
#include "flp_file_service.c"      // read-back verification (no read in the stream contract)
#include "flp_stream_service.c"    // code under test (platform stream service)
#include "fla_stream_service.c"    // consumer accessor + default stubs

#include <string.h> // memcmp, strlen

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int tmp_counter = 0;

// A unique temp path per call so tests do not collide on a shared file.
static void next_path(char *path, size_t size) {
    snprintf(path, size, "fl_stream_test_%d.tmp", ++tmp_counter);
}

// Delete a UTF-8-named file. Routes through DeleteFileW so non-ASCII names (and
// ordinary ASCII ones, which are valid UTF-8) are both handled.
static void delete_path(char const *path) {
    WCHAR w[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, w, MAX_PATH) != 0) {
        DeleteFileW(w);
    }
}

// Read the whole contents of path back via the (unrelated) synchronous file service.
static size_t read_back(char const *path, char *buf, size_t bufsize) {
    FLFile *f = flp_file_open(path, FL_FILE_READ);
    FL_ASSERT_NOT_NULL(f);
    size_t n = flp_file_read(f, buf, bufsize, 0);
    flp_file_close(f);
    return n;
}

// ---------------------------------------------------------------------------
// Cross-boundary injection
//
// This relies entirely on the driver: when it loads this DLL it resolves the exported
// fla_set_stream_service via GetProcAddress and installs the host's stream service into
// g_fla_stream_service before any test runs. A pass proves the symbol was exported and
// the service crossed the DLL boundary; were it missing, g_fla_stream_service would
// still hold the default_stream_* abort stubs (static in the included
// fla_stream_service.c). The host installs its own flp_stream_* copies -- different
// addresses from this DLL's -- so we assert the stubs were replaced rather than pointer
// identity, then round-trip a write through the injected service.
//
// Registered first so it observes the driver's injection before the plumbing test below
// self-injects this DLL's own provider into g_fla_stream_service.
// ---------------------------------------------------------------------------

FL_TEST("Driver Injects Stream Service", driver_injects_stream_service) {
    FL_ASSERT_TRUE(g_fla_stream_service.open != default_stream_open);
    FL_ASSERT_TRUE(g_fla_stream_service.write != default_stream_write);
    FL_ASSERT_TRUE(g_fla_stream_service.close != default_stream_close);
    FL_ASSERT_TRUE(g_fla_stream_service.console != default_stream_console);

    // Exercise the injected service end to end; if these were the abort stubs the
    // process would terminate.
    char path[256];
    next_path(path, sizeof path);
    char const msg[] = "injected";

    FLFile *f = g_fla_stream_service.open(path);
    FL_ASSERT_NOT_NULL(f);
    size_t wn = g_fla_stream_service.write(f, msg, sizeof msg - 1);
    g_fla_stream_service.close(f);
    FL_ASSERT_EQ_SIZE_T(wn, sizeof msg - 1);

    char buf[16] = {0};
    FL_ASSERT_EQ_SIZE_T(read_back(path, buf, sizeof buf), sizeof msg - 1);
    FL_ASSERT_TRUE(memcmp(buf, msg, sizeof msg - 1) == 0);

    delete_path(path);
}

// ---------------------------------------------------------------------------
// Service plumbing
// ---------------------------------------------------------------------------

// flp_init_stream_service installs this DLL's platform functions into its own
// g_fla_stream_service, replacing whatever the driver injected at load.
FL_TEST("Init Installs Provider", init_installs_provider) {
    flp_init_stream_service(fla_set_stream_service);
    FL_ASSERT_TRUE(g_fla_stream_service.open == flp_stream_open);
    FL_ASSERT_TRUE(g_fla_stream_service.write == flp_stream_write);
    FL_ASSERT_TRUE(g_fla_stream_service.close == flp_stream_close);
    FL_ASSERT_TRUE(g_fla_stream_service.console == flp_stream_console);
}

// ---------------------------------------------------------------------------
// Append write round-trip
// ---------------------------------------------------------------------------

// write takes no offset: two writes both land at end of file and concatenate. This is
// the direct replacement for the file service's removed "Append Ignores Offset" test.
FL_TEST("Open Write Close Round Trip", open_write_close_round_trip) {
    char path[256];
    next_path(path, sizeof path);

    FLFile *f = flp_stream_open(path);
    FL_ASSERT_NOT_NULL(f);
    FL_ASSERT_EQ_SIZE_T(flp_stream_write(f, "AAA", 3), (size_t)3);
    FL_ASSERT_EQ_SIZE_T(flp_stream_write(f, "BBB", 3), (size_t)3);
    flp_stream_close(f);

    char buf[8] = {0};
    FL_ASSERT_EQ_SIZE_T(read_back(path, buf, sizeof buf), (size_t)6);
    FL_ASSERT_TRUE(memcmp(buf, "AAABBB", 6) == 0);

    delete_path(path);
}

// Closing and reopening the same path preserves prior content: OPEN_ALWAYS +
// FILE_APPEND_DATA survive a close/reopen cycle, not just repeated writes within one
// session.
FL_TEST("Reopen Appends To Existing", reopen_appends_to_existing) {
    char path[256];
    next_path(path, sizeof path);

    FLFile *f = flp_stream_open(path);
    FL_ASSERT_NOT_NULL(f);
    FL_ASSERT_EQ_SIZE_T(flp_stream_write(f, "AAA", 3), (size_t)3);
    flp_stream_close(f);

    f = flp_stream_open(path);
    FL_ASSERT_NOT_NULL(f);
    FL_ASSERT_EQ_SIZE_T(flp_stream_write(f, "BBB", 3), (size_t)3);
    flp_stream_close(f);

    char buf[8] = {0};
    FL_ASSERT_EQ_SIZE_T(read_back(path, buf, sizeof buf), (size_t)6);
    FL_ASSERT_TRUE(memcmp(buf, "AAABBB", 6) == 0);

    delete_path(path);
}

// ---------------------------------------------------------------------------
// Open-failure handling
// ---------------------------------------------------------------------------

FL_TEST("Open Missing Directory Returns Null", open_missing_directory_returns_null) {
    FLFile *f = flp_stream_open("no_such_dir_abc123\\no_such_file.tmp");
    FL_ASSERT_NULL(f);
}

// ---------------------------------------------------------------------------
// Console accessor
// ---------------------------------------------------------------------------

FL_TEST("Console Stdout Returns Non Null", console_stdout_returns_non_null) {
    FLFile *f = flp_stream_console(FL_STREAM_STDOUT);
    FL_ASSERT_NOT_NULL(f);
}

FL_TEST("Console Stderr Returns Non Null", console_stderr_returns_non_null) {
    FLFile *f = flp_stream_console(FL_STREAM_STDERR);
    FL_ASSERT_NOT_NULL(f);
}

// The key coverage for console(): redirect the process's real stdout to a temp file
// for the duration of the test, write through the console accessor twice (closing the
// handle in between to prove close() is a no-op rather than tearing down the redirected
// target), restore the original stdout, then read the temp file back through the
// synchronous file service.
FL_TEST("Console Redirect Write Round Trip", console_redirect_write_round_trip) {
    char path[256];
    next_path(path, sizeof path);

    FLFile *temp = flp_file_open(path, FL_FILE_WRITE);
    FL_ASSERT_NOT_NULL(temp);
    HANDLE temp_handle = (HANDLE)temp;

    HANDLE saved = GetStdHandle(STD_OUTPUT_HANDLE);
    FL_ASSERT_TRUE(SetStdHandle(STD_OUTPUT_HANDLE, temp_handle) != 0);

    FLFile *console = flp_stream_console(FL_STREAM_STDOUT);
    FL_ASSERT_NOT_NULL(console);
    FL_ASSERT_EQ_SIZE_T(flp_stream_write(console, "first\n", 6), (size_t)6);
    flp_stream_close(console); // must not close temp_handle

    // A fresh console() call must still resolve to the (still-open, still-redirected)
    // real handle, not a handle invalidated by the close() above.
    console = flp_stream_console(FL_STREAM_STDOUT);
    FL_ASSERT_NOT_NULL(console);
    FL_ASSERT_EQ_SIZE_T(flp_stream_write(console, "second\n", 7), (size_t)7);

    FL_ASSERT_TRUE(SetStdHandle(STD_OUTPUT_HANDLE, saved) != 0);
    flp_file_close(temp); // the test's own handle, not the service's to close

    char buf[16] = {0};
    FL_ASSERT_EQ_SIZE_T(read_back(path, buf, sizeof buf), (size_t)13);
    FL_ASSERT_TRUE(memcmp(buf, "first\nsecond\n", 13) == 0);

    delete_path(path);
}

// ---------------------------------------------------------------------------
// Suite registration
// ---------------------------------------------------------------------------

FL_SUITE_BEGIN(StreamService)
FL_SUITE_ADD(driver_injects_stream_service)
FL_SUITE_ADD(init_installs_provider)
FL_SUITE_ADD(open_write_close_round_trip)
FL_SUITE_ADD(reopen_appends_to_existing)
FL_SUITE_ADD(open_missing_directory_returns_null)
FL_SUITE_ADD(console_stdout_returns_non_null)
FL_SUITE_ADD(console_stderr_returns_non_null)
FL_SUITE_ADD(console_redirect_write_round_trip)
FL_SUITE_END;

FL_GET_TEST_SUITE("Stream Service", StreamService);
