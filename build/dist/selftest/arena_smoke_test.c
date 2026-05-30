/**
 * @file arena_smoke_test.c
 * @author Douglas Cuthbertson
 * @brief Smoke test that the imported `arena` package compiles and runs as a
 *        single FL_EMBEDDED binary.
 *
 * The in-driver arena_*_test suites already cover the allocator's functional
 * surface; this test stays minimal on purpose. Its purpose is to prove the
 * carved-out arena package is self-sufficient: create an arena, allocate and
 * free, observe the allocation count, and release.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty. Exit code: 0 = pass, 1 = failure.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

#include <faultline/fl_try.h>
#include <faultline/arena.h>
#include <faultline/arena_malloc.h>

#include <stdbool.h>
#include <stdio.h>

static int g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                   \
        }                                                                   \
    } while (0)

#define COMMIT_64K ((size_t)64 * 1024)

int main(void) {
    fprintf(stdout, "arena_smoke_test (imported package)\n");

    Arena *arena = new_arena(COMMIT_64K, 0);
    CHECK(arena != NULL);
    CHECK(arena_footprint(arena) > 0);
    CHECK(arena_allocation_count(arena) == 0);

    void *p = arena_malloc(arena, 128, __FILE__, __LINE__);
    CHECK(p != NULL);
    CHECK(arena_allocation_count(arena) == 1);
    CHECK(arena_allocation_size(p) >= 128);

    arena_free(arena, p, __FILE__, __LINE__);
    CHECK(arena_allocation_count(arena) == 0);

    release_arena(&arena);
    CHECK(arena == NULL);

    if (g_failures == 0) {
        fprintf(stdout, "PASS\n");
        return 0;
    }
    fprintf(stderr, "FAIL (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
    return 1;
}
