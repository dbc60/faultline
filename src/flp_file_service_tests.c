/**
 * @file flp_file_service_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for the file service implementation.
 * @version 0.1
 * @date 2026-06-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 * Verifies the platform-side file service (flp_file_service.c): service plumbing,
 * positional read/write round-trips, open-failure handling, and the UTF-8 -> UTF-16 path
 * conversion.
 *
 * The driver injects an FLFileService via fla_set_file_service() when it loads this DLL,
 * so the cross-boundary test observes the host's install while the round-trip tests
 * exercise the provider directly. FL_ASSERT macros throw through this module's own
 * exception implementation, and fl_run_case catches them here.
 */

#include <faultline/fl_exception_assert.h> // FL_ASSERT_* macros
#include <faultline/fl_test.h>             // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h>              // FL_THROW (resolves to FLA_* in DLL builds)

#include "fl_exception.c"       // exception reasons, push/pop/throw
#include "fla_memory_service.c" // g_fla_memory_service (FL_MALLOC backing)
#include "flp_file_service.c"   // code under test (platform file service)
#include "fla_file_service.c"   // consumer accessor + default stubs
#include "fla_timer_service.c"  // g_fla_timer_service, fla_set_timer_service

#include <string.h> // memcmp, strlen
#include <wchar.h>  // swprintf, wcsrchr

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int tmp_counter = 0;

// A unique temp path per call so tests do not collide on a shared file.
static void next_path(char *path, size_t size) {
    snprintf(path, size, "fl_file_test_%d.tmp", ++tmp_counter);
}

// Delete a UTF-8-named file. Routes through DeleteFileW so non-ASCII names (and ordinary
// ASCII ones, which are valid UTF-8) are both handled.
static void delete_path(char const *path) {
    WCHAR w[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, w, MAX_PATH) != 0) {
        DeleteFileW(w);
    }
}

// Write the whole buffer to a fresh file at offset 0 and close it.
static void write_file(char const *path, void const *data, size_t len) {
    FLFile *f = flp_file_open(path, FL_FILE_WRITE);
    FL_ASSERT_NOT_NULL(f);
    size_t n = flp_file_write(f, data, len, 0);
    flp_file_close(f);
    FL_ASSERT_EQ_SIZE_T(n, len);
}

// ---------------------------------------------------------------------------
// Cross-boundary injection
//
// This relies entirely on the driver: when it loads this DLL it resolves the exported
// fla_set_file_service via GetProcAddress and installs the host's file service into
// g_fla_file_service before any test runs. A pass proves the symbol was exported and the
// service crossed the DLL boundary; were it missing, g_fla_file_service would still hold
// the default_file_* abort stubs (static in the included fla_file_service.c). The host
// installs its own flp_file_* copies -- different addresses from this DLL's -- so we
// assert the stubs were replaced rather than pointer identity, then round-trip a file
// through the injected service.
//
// Registered first so it observes the driver's injection before the plumbing test below
// self-injects this DLL's own provider into g_fla_file_service.
// ---------------------------------------------------------------------------

FL_TEST("Driver Injects File Service", driver_injects_file_service) {
    FL_ASSERT_TRUE(g_fla_file_service.open != default_file_open);
    FL_ASSERT_TRUE(g_fla_file_service.read != default_file_read);
    FL_ASSERT_TRUE(g_fla_file_service.write != default_file_write);
    FL_ASSERT_TRUE(g_fla_file_service.close != default_file_close);

    // Exercise the injected service end to end; if these were the abort stubs the
    // process would terminate.
    char path[256];
    next_path(path, sizeof path);
    char const msg[] = "injected";

    FLFile *f = g_fla_file_service.open(path, FL_FILE_WRITE);
    FL_ASSERT_NOT_NULL(f);
    size_t wn = g_fla_file_service.write(f, msg, sizeof msg - 1, 0);
    g_fla_file_service.close(f);
    FL_ASSERT_EQ_SIZE_T(wn, sizeof msg - 1);

    f = g_fla_file_service.open(path, FL_FILE_READ);
    FL_ASSERT_NOT_NULL(f);
    char   buf[16] = {0};
    size_t rn      = g_fla_file_service.read(f, buf, sizeof msg - 1, 0);
    g_fla_file_service.close(f);
    FL_ASSERT_EQ_SIZE_T(rn, sizeof msg - 1);
    FL_ASSERT_TRUE(memcmp(buf, msg, sizeof msg - 1) == 0);

    delete_path(path);
}

// ---------------------------------------------------------------------------
// Service plumbing
// ---------------------------------------------------------------------------

// flp_init_file_service installs this DLL's platform functions into its own
// g_fla_file_service, replacing whatever the driver injected at load.
FL_TEST("Init Installs Provider", init_installs_provider) {
    flp_init_file_service(fla_set_file_service);
    FL_ASSERT_TRUE(g_fla_file_service.open == flp_file_open);
    FL_ASSERT_TRUE(g_fla_file_service.read == flp_file_read);
    FL_ASSERT_TRUE(g_fla_file_service.write == flp_file_write);
    FL_ASSERT_TRUE(g_fla_file_service.close == flp_file_close);
}

// ---------------------------------------------------------------------------
// Round-trip read/write
// ---------------------------------------------------------------------------

FL_TEST("Write Then Read Round Trip", write_then_read_round_trip) {
    char path[256];
    next_path(path, sizeof path);

    char const msg[] = "hello, faultline";
    write_file(path, msg, sizeof msg - 1);

    FLFile *f = flp_file_open(path, FL_FILE_READ);
    FL_ASSERT_NOT_NULL(f);
    char   buf[64] = {0};
    size_t n       = flp_file_read(f, buf, sizeof msg - 1, 0);
    flp_file_close(f);

    FL_ASSERT_EQ_SIZE_T(n, sizeof msg - 1);
    FL_ASSERT_TRUE(memcmp(buf, msg, sizeof msg - 1) == 0);

    delete_path(path);
}

// A read names its own offset and does not depend on a file pointer.
FL_TEST("Positional Read", positional_read) {
    char path[256];
    next_path(path, sizeof path);

    char const data[] = "ABCDEFGH";
    write_file(path, data, 8);

    FLFile *f = flp_file_open(path, FL_FILE_READ);
    FL_ASSERT_NOT_NULL(f);
    char   buf[4] = {0};
    size_t n      = flp_file_read(f, buf, 3, 2); // expect "CDE"
    flp_file_close(f);

    FL_ASSERT_EQ_SIZE_T(n, (size_t)3);
    FL_ASSERT_TRUE(memcmp(buf, "CDE", 3) == 0);

    delete_path(path);
}

// Two positional writes in one session will be at the offsets named, not at a running
// file pointer.
FL_TEST("Positional Write", positional_write) {
    char path[256];
    next_path(path, sizeof path);

    FLFile *f = flp_file_open(path, FL_FILE_WRITE);
    FL_ASSERT_NOT_NULL(f);
    FL_ASSERT_EQ_SIZE_T(flp_file_write(f, "AAAA", 4, 0), (size_t)4);
    FL_ASSERT_EQ_SIZE_T(flp_file_write(f, "BB", 2, 4), (size_t)2);
    flp_file_close(f);

    f = flp_file_open(path, FL_FILE_READ);
    FL_ASSERT_NOT_NULL(f);
    char   buf[8] = {0};
    size_t n      = flp_file_read(f, buf, 6, 0);
    flp_file_close(f);

    FL_ASSERT_EQ_SIZE_T(n, (size_t)6);
    FL_ASSERT_TRUE(memcmp(buf, "AAAABB", 6) == 0);

    delete_path(path);
}

// ---------------------------------------------------------------------------
// Open-failure handling
// ---------------------------------------------------------------------------

// Opening a missing file for reading returns NULL rather than a live handle.
FL_TEST("Open Missing Returns Null", open_missing_returns_null) {
    FLFile *f = flp_file_open("no_such_dir_abc123\\no_such_file.tmp", FL_FILE_READ);
    FL_ASSERT_NULL(f);
}

// An unrecognized mode is rejected by the switch default.
FL_TEST("Open Invalid Mode Returns Null", open_invalid_mode_returns_null) {
    FLFile *f = flp_file_open("anything.tmp", (FLFileMode)999);
    FL_ASSERT_NULL(f);
}

// ---------------------------------------------------------------------------
// UTF-8 path handling
// ---------------------------------------------------------------------------

// A non-ASCII UTF-8 path round-trips, exercising the CreateFileW conversion. The
// "\xC3\xA9" bytes are the UTF-8 encoding of U+00E9 (e-acute).
FL_TEST("Utf8 Path Round Trip", utf8_path_round_trip) {
    char const path[] = "fl_file_test_caf\xC3\xA9.tmp";

    char const msg[] = "unicode";
    write_file(path, msg, sizeof msg - 1);

    FLFile *f = flp_file_open(path, FL_FILE_READ);
    FL_ASSERT_NOT_NULL(f);
    char   buf[16] = {0};
    size_t n       = flp_file_read(f, buf, sizeof msg - 1, 0);
    flp_file_close(f);

    FL_ASSERT_EQ_SIZE_T(n, sizeof msg - 1);
    FL_ASSERT_TRUE(memcmp(buf, msg, sizeof msg - 1) == 0);

    delete_path(path);
}

// ---------------------------------------------------------------------------
// Long-path handling
// ---------------------------------------------------------------------------

// The provider accepts paths longer than MAX_PATH by opening them in extended-length
// (\\?\) form. The test builds a directory chain deep enough that the relative path
// exceeds MAX_PATH, round-trips a file at the bottom through the provider, and removes
// the chain deepest-first. The directories are created and deleted with explicit
// \\?\ paths; only the file open goes through the provider's own conversion.
FL_TEST("Long Path Round Trip", long_path_round_trip) {
    char const seg[] = "fl_long_path_segment_0123456789"; // 31 chars per level
    int const  depth = 10;                                // > 300 chars of path

    // Extended-length absolute base for the directory operations.
    WCHAR wdir[1024];
    WCHAR cwd[MAX_PATH];
    DWORD cwd_len = GetCurrentDirectoryW(MAX_PATH, cwd);
    FL_ASSERT_TRUE(cwd_len > 0 && cwd_len < MAX_PATH);
    int wpos = swprintf(wdir, sizeof wdir / sizeof wdir[0], L"\\\\?\\%ls", cwd);
    FL_ASSERT_TRUE(wpos > 0);

    // Relative UTF-8 path handed to the provider.
    char   rel[512];
    size_t rel_len = 0;

    for (int i = 0; i < depth; i++) {
        rel_len += (size_t)snprintf(rel + rel_len, sizeof rel - rel_len, "%s\\", seg);
        wpos += swprintf(wdir + wpos, sizeof wdir / sizeof wdir[0] - (size_t)wpos,
                         L"\\%hs", seg);
        if (!CreateDirectoryW(wdir, NULL)) {
            FL_ASSERT_EQ_UINT32(GetLastError(), (DWORD)ERROR_ALREADY_EXISTS);
        }
    }
    snprintf(rel + rel_len, sizeof rel - rel_len, "long.tmp");
    FL_ASSERT_GT_SIZE_T(strlen(rel), (size_t)MAX_PATH);

    char const msg[] = "long path";
    write_file(rel, msg, sizeof msg - 1);

    FLFile *f = flp_file_open(rel, FL_FILE_READ);
    FL_ASSERT_NOT_NULL(f);
    char   buf[16] = {0};
    size_t n       = flp_file_read(f, buf, sizeof msg - 1, 0);
    flp_file_close(f);

    FL_ASSERT_EQ_SIZE_T(n, sizeof msg - 1);
    FL_ASSERT_TRUE(memcmp(buf, msg, sizeof msg - 1) == 0);

    // Remove the file, then the directory chain, deepest first.
    WCHAR wfile[1024];
    swprintf(wfile, sizeof wfile / sizeof wfile[0], L"%ls\\long.tmp", wdir);
    DeleteFileW(wfile);
    for (int i = 0; i < depth; i++) {
        RemoveDirectoryW(wdir);
        WCHAR *slash = wcsrchr(wdir, L'\\');
        FL_ASSERT_NOT_NULL(slash);
        *slash = L'\0';
    }
}

// ---------------------------------------------------------------------------
// Suite registration
// ---------------------------------------------------------------------------

FL_SUITE_BEGIN(FileService)
FL_SUITE_ADD(driver_injects_file_service)
FL_SUITE_ADD(init_installs_provider)
FL_SUITE_ADD(write_then_read_round_trip)
FL_SUITE_ADD(positional_read)
FL_SUITE_ADD(positional_write)
FL_SUITE_ADD(open_missing_returns_null)
FL_SUITE_ADD(open_invalid_mode_returns_null)
FL_SUITE_ADD(utf8_path_round_trip)
FL_SUITE_ADD(long_path_round_trip)
FL_SUITE_END;

FL_GET_TEST_SUITE("File Service", FileService);
