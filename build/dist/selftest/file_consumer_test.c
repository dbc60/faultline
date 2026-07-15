/**
 * @file file_consumer_test.c
 * @author Douglas Cuthbertson
 * @brief Single-binary platform test for the file service, compiled against an
 *        *imported* file package layered over its memory_service dependency (not
 *        the repo's src/ tree).
 *
 * Compiled WITH FL_PLATFORM_BUILD, so fl_file.h resolves the FL_FILE_* macros to
 * the flp_ provider directly. The provider allocates its UTF-16 path buffers
 * through FL_MALLOC, so the test binds the plain memory context to an arena
 * first -- the same assembly a real platform host performs.
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
#include <faultline/fl_file.h>            // FL_FILE_OPEN/READ/WRITE/CLOSE
#include <faultline/fl_file_types.h>      // FLFile, FL_FILE_READ/WRITE/APPEND
#include <faultline/fl_macros.h>          // FL_UNUSED
#include <faultline/fl_memory_service.h>  // FLA_SET_MEMORY_SERVICE_FN
#include <faultline/fl_try.h>             // FL_TRY, FL_CATCH_ALL, FL_END_TRY
#include <faultline/flp_memory_context.h> // FLMemoryContext, flp_init_memory_context
#include <flp_memory_service.h>           // flp_init_memory_service

// The provider reaches memory through FL_MALLOC; binding the static context is
// the only side effect wanted here, so the setter is a no-op (nothing to inject
// into -- this binary IS the platform).
static FLA_SET_MEMORY_SERVICE_FN(noop_set_memory_service) {
    FL_UNUSED(svc);
    FL_UNUSED(size);
}

int main(void) {
    fprintf(stdout, "file_consumer_test (imported package)\n");

    Arena          *arena = new_arena(0, 0);
    FLMemoryContext ctx   = {0};
    char            path[512];
    char const     *tmp = getenv("TEMP");

    flp_init_memory_context(&ctx, arena);
    flp_init_memory_service(noop_set_memory_service, &ctx);

    snprintf(path, sizeof path, "%s\\fl_file_selftest.tmp", tmp != NULL ? tmp : ".");

    char const payload[] = "faultline file service\n";
    size_t     len       = sizeof payload - 1;

    FL_TRY {
        SECTION("open missing returns NULL");
        {
            FLFile *f = FL_FILE_OPEN("fl_no_such_dir\\missing.bin", FL_FILE_READ);
            CHECK(f == NULL);
        }

        SECTION("write then read round trip");
        {
            FLFile *f = FL_FILE_OPEN(path, FL_FILE_WRITE);
            CHECK(f != NULL);
            CHECK(FL_FILE_WRITE(f, payload, len, 0) == len);
            FL_FILE_CLOSE(f);

            char buf[64] = {0};
            f            = FL_FILE_OPEN(path, FL_FILE_READ);
            CHECK(f != NULL);
            CHECK(FL_FILE_READ(f, buf, sizeof buf, 0) == len);
            CHECK(memcmp(buf, payload, len) == 0);

            // Positional read: pick the tail up mid-payload.
            char tail[64] = {0};
            CHECK(FL_FILE_READ(f, tail, sizeof tail, 10) == len - 10);
            CHECK(memcmp(tail, payload + 10, len - 10) == 0);
            FL_FILE_CLOSE(f);
        }

        SECTION("append ignores the write offset");
        {
            char const extra[] = "tail";
            FLFile    *f       = FL_FILE_OPEN(path, FL_FILE_APPEND);
            CHECK(f != NULL);
            CHECK(FL_FILE_WRITE(f, extra, 4, 0) == 4); // offset 0 still appends
            FL_FILE_CLOSE(f);

            char buf[64] = {0};
            f            = FL_FILE_OPEN(path, FL_FILE_READ);
            CHECK(f != NULL);
            CHECK(FL_FILE_READ(f, buf, sizeof buf, 0) == len + 4);
            CHECK(memcmp(buf + len, extra, 4) == 0);
            FL_FILE_CLOSE(f);
        }

        SECTION("truncate on FL_FILE_WRITE reopen");
        {
            FLFile *f = FL_FILE_OPEN(path, FL_FILE_WRITE);
            CHECK(f != NULL);
            CHECK(FL_FILE_WRITE(f, "x", 1, 0) == 1);
            FL_FILE_CLOSE(f);

            char buf[8] = {0};
            f           = FL_FILE_OPEN(path, FL_FILE_READ);
            CHECK(f != NULL);
            CHECK(FL_FILE_READ(f, buf, sizeof buf, 0) == 1);
            CHECK(buf[0] == 'x');
            FL_FILE_CLOSE(f);
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
        fprintf(stdout, "PASS (file service checks)\n");
    } else {
        fprintf(stderr, "%d check(s) FAILED.\n", g_failures);
    }

    return g_failures > 0 ? 1 : 0;
}
