/**
 * @file stream_consumer_test.c
 * @author Douglas Cuthbertson
 * @brief Single-binary platform test for the stream service, compiled against an
 *        *imported* stream package layered over its memory_service dependency (not
 *        the repo's src/ tree).
 *
 * Compiled WITH FL_PLATFORM_BUILD, so fl_stream.h resolves the FL_STREAM_* macros to
 * the flp_ provider directly. The provider allocates its UTF-16 path buffers through
 * FL_MALLOC only on the long-path route, so the test binds the plain memory context to
 * an arena first -- the same assembly a real platform host performs.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty (no dllimport on a locally-defined
 * function). Use /DDLL_BUILD instead when compiling into an actual DLL.
 *
 * Exit code: 0 = all passed, 1 = one or more failures.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_selftest.h"

#include <stddef.h> // size_t, NULL
#include <stdlib.h> // getenv, remove
#include <string.h> // memcmp, strlen

#include <faultline/arena.h>              // Arena, new_arena, release_arena
#include <faultline/fl_file.h>            // FL_FILE_OPEN/READ/WRITE/CLOSE (read-back only)
#include <faultline/fl_file_types.h>      // FLFile, FL_FILE_READ
#include <faultline/fl_macros.h>          // FL_UNUSED
#include <faultline/fl_memory_service.h>  // FLA_SET_MEMORY_SERVICE_FN
#include <faultline/fl_stream.h>          // FL_STREAM_OPEN/WRITE/CLOSE/CONSOLE
#include <faultline/fl_try.h>             // FL_TRY, FL_CATCH_ALL, FL_END_TRY
#include <faultline/flp_memory_context.h> // FLMemoryContext, flp_init_memory_context
#include <flp_memory_service.h>           // flp_init_memory_service

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // GetStdHandle, SetStdHandle

// The provider reaches memory through FL_MALLOC; binding the static context is
// the only side effect wanted here, so the setter is a no-op (nothing to inject
// into -- this binary IS the platform).
static FLA_SET_MEMORY_SERVICE_FN(noop_set_memory_service) {
    FL_UNUSED(svc);
    FL_UNUSED(size);
}

// Read the whole contents of path back via the (unrelated) file service.
static size_t read_back(char const *path, char *buf, size_t bufsize) {
    FLFile *f = FL_FILE_OPEN(path, FL_FILE_READ);
    if (f == NULL) {
        return 0;
    }
    size_t n = FL_FILE_READ(f, buf, bufsize, 0);
    FL_FILE_CLOSE(f);
    return n;
}

int main(void) {
    fprintf(stdout, "stream_consumer_test (imported package)\n");

    Arena          *arena = new_arena(0, 0);
    FLMemoryContext ctx   = {0};
    char            path[512];
    char const     *tmp = getenv("TEMP");

    flp_init_memory_context(&ctx, arena);
    flp_init_memory_service(noop_set_memory_service, &ctx);

    snprintf(path, sizeof path, "%s\\fl_stream_selftest.tmp", tmp != NULL ? tmp : ".");
    remove(path);

    FL_TRY {
        SECTION("open missing directory returns NULL");
        {
            FLFile *f = FL_STREAM_OPEN("fl_no_such_dir\\missing.bin");
            CHECK(f == NULL);
        }

        SECTION("write has no offset and appends at end of file");
        {
            FLFile *f = FL_STREAM_OPEN(path);
            CHECK(f != NULL);
            CHECK(FL_STREAM_WRITE(f, "AAA", 3) == 3);
            CHECK(FL_STREAM_WRITE(f, "BBB", 3) == 3);
            FL_STREAM_CLOSE(f);

            char buf[8] = {0};
            CHECK(read_back(path, buf, sizeof buf) == 6);
            CHECK(memcmp(buf, "AAABBB", 6) == 0);
        }

        SECTION("reopen appends to existing content");
        {
            FLFile *f = FL_STREAM_OPEN(path);
            CHECK(f != NULL);
            CHECK(FL_STREAM_WRITE(f, "CCC", 3) == 3);
            FL_STREAM_CLOSE(f);

            char buf[16] = {0};
            CHECK(read_back(path, buf, sizeof buf) == 9);
            CHECK(memcmp(buf, "AAABBBCCC", 9) == 0);
        }

        SECTION("console close is a no-op on a redirected handle");
        {
            FLFile *temp = FL_FILE_OPEN(path, FL_FILE_WRITE);
            CHECK(temp != NULL);
            HANDLE temp_handle = (HANDLE)temp;

            HANDLE saved = GetStdHandle(STD_OUTPUT_HANDLE);
            CHECK(SetStdHandle(STD_OUTPUT_HANDLE, temp_handle) != 0);

            FLFile *console = FL_STREAM_CONSOLE(FL_STREAM_STDOUT);
            CHECK(console != NULL);
            CHECK(FL_STREAM_WRITE(console, "redirected\n", 11) == 11);
            FL_STREAM_CLOSE(console); // must not close temp_handle

            // A fresh console() call must still resolve to the still-open handle.
            console = FL_STREAM_CONSOLE(FL_STREAM_STDOUT);
            CHECK(console != NULL);
            CHECK(FL_STREAM_WRITE(console, "again\n", 6) == 6);

            CHECK(SetStdHandle(STD_OUTPUT_HANDLE, saved) != 0);
            FL_FILE_CLOSE(temp); // the test's own handle, not the service's to close

            char buf[32] = {0};
            CHECK(read_back(path, buf, sizeof buf) == 17);
            CHECK(memcmp(buf, "redirected\nagain\n", 17) == 0);
        }
    }
    FL_CATCH_ALL {
        fprintf(stderr, "FAIL: unexpected exception\n");
        g_failures++;
    }
    FL_END_TRY;

    remove(path);
    release_arena(&arena);

    if (g_failures == 0) {
        fprintf(stdout, "PASS (stream service checks)\n");
    } else {
        fprintf(stderr, "%d check(s) FAILED.\n", g_failures);
    }

    return g_failures > 0 ? 1 : 0;
}
