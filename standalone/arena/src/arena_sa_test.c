/**
 * @file arena_sa_test.c
 * @author Douglas Cuthbertson
 * @brief Standalone arena test suite — no FaultLine test infrastructure required.
 *
 * /DFL_EMBEDDED keeps FL_DECL_SPEC empty (no dllimport on a locally-defined
 * function).  Use /DDLL_BUILD instead when compiling into an actual DLL.
 *
 * Exit code: 0 = all passed, 1 = one or more failures.
 * @version 0.2
 * @date 2026-05-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */

#include <stdbool.h> // bool
#include <stdint.h>  // uintptr_t
#include <stdio.h>   // fprintf, stdout, stderr

#include <faultline/fl_try.h> // FL_TRY, FL_CATCH, FL_END_TRY, FL_THROW, FL_CATCH_STR
#include <faultline/arena.h>  // Arena, new_arena, release_arena, arena_*
#include <faultline/arena_malloc.h> // arena_malloc, arena_free, arena_calloc, etc.

/*
 * Minimal test harness
 */

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

#define SECTION(name) fprintf(stdout, "  %s\n", name)

/*
 * Helpers
 */

/* Convenience wrappers that insert __FILE__ and __LINE__ */
#define ARENA_MALLOC(ar, sz)       arena_malloc((ar), (sz), __FILE__, __LINE__)
#define ARENA_CALLOC(ar, n, sz)    arena_calloc((ar), (n), (sz), __FILE__, __LINE__)
#define ARENA_REALLOC(ar, p, sz)   arena_realloc((ar), (p), (sz), __FILE__, __LINE__)
#define ARENA_FREE(ar, p)          arena_free((ar), (p), __FILE__, __LINE__)
#define ARENA_FREE_POINTER(ar, pp) arena_free_pointer((ar), (pp), __FILE__, __LINE__)
#define ARENA_ALIGNED_ALLOC(ar, al, sz) \
    arena_aligned_alloc((ar), (al), (sz), __FILE__, __LINE__)
/* ARENA_MALLOC_THROW, ARENA_FREE_THROW, etc. are already defined in arena.h */

/* Commit 64 KiB for most tests — small enough to be cheap. */
#define COMMIT_64K ((size_t)64 * 1024)
#define COMMIT_4M  ((size_t)4 * 1024 * 1024)

/*
 * 1. Lifecycle
 */

static void test_lifecycle(void) {
    SECTION("lifecycle: new_arena / release_arena");

    Arena *arena = new_arena(COMMIT_64K, 0);
    CHECK(arena != NULL);
    CHECK(arena_footprint(arena) > 0);

    release_arena(&arena);
    CHECK(arena == NULL);

    /* Double release must not crash */
    release_arena(&arena);
    CHECK(arena == NULL);
}

/*
 * 2. Throwing malloc
 */

static void test_throwing_malloc(void) {
    SECTION("throwing malloc: small, large, zero");

    Arena *arena = new_arena(COMMIT_4M, 0);
    CHECK(arena != NULL);

    /* Small allocation */
    void *p1 = ARENA_MALLOC_THROW(arena, 32);
    CHECK(p1 != NULL);
    CHECK(arena_allocation_count(arena) == 1);

    /* Large allocation */
    void *p2 = ARENA_MALLOC_THROW(arena, 4096);
    CHECK(p2 != NULL);
    CHECK(arena_allocation_count(arena) == 2);

    /* Zero-byte request — implementation-defined; must not crash */
    void *p0 = ARENA_MALLOC_THROW(arena, 0);
    (void)p0; /* may be NULL or a unique pointer */

    ARENA_FREE_THROW(arena, p2);
    ARENA_FREE_THROW(arena, p1);

    release_arena(&arena);
}

/*
 * 3. Throwing calloc
 */

static void test_throwing_calloc(void) {
    SECTION("throwing calloc: zero-fill, non-null");

    Arena *arena = new_arena(COMMIT_4M, 0);
    CHECK(arena != NULL);

    unsigned char *buf = ARENA_CALLOC_THROW(arena, 128, sizeof(unsigned char));
    CHECK(buf != NULL);

    bool all_zero = true;
    for (int i = 0; i < 128; i++) {
        if (buf[i] != 0) {
            all_zero = false;
            break;
        }
    }
    CHECK_MSG(all_zero, "calloc memory must be zero-initialised");

    release_arena(&arena);
}

/*
 * 4. Throwing realloc
 */

static void test_throwing_realloc(void) {
    SECTION("throwing realloc: grow, shrink, NULL mem");

    Arena *arena = new_arena(COMMIT_4M, 0);
    CHECK(arena != NULL);

    /* NULL mem → behaves like malloc */
    void *p = ARENA_REALLOC_THROW(arena, NULL, 64);
    CHECK(p != NULL);

    /* Grow */
    void *p2 = ARENA_REALLOC_THROW(arena, p, 512);
    CHECK(p2 != NULL);

    /* Shrink */
    void *p3 = ARENA_REALLOC_THROW(arena, p2, 32);
    CHECK(p3 != NULL);

    release_arena(&arena);
}

/*
 * 5. Throwing aligned alloc
 */

static void test_throwing_aligned_alloc(void) {
    SECTION("throwing aligned alloc: powers of two");

    Arena *arena = new_arena(COMMIT_4M, 0);
    CHECK(arena != NULL);

    size_t alignments[] = {16, 32, 64, 128, 256};
    for (size_t i = 0; i < sizeof alignments / sizeof alignments[0]; i++) {
        size_t al  = alignments[i];
        void  *mem = ARENA_ALIGNED_ALLOC_THROW(arena, al, al * 4);
        CHECK(mem != NULL);
        CHECK(((uintptr_t)mem & (al - 1)) == 0);
    }

    release_arena(&arena);
}

/*
 * 6. Non-throwing wrappers
 */

static void test_non_throwing_wrappers(void) {
    SECTION("non-throwing: malloc, calloc, realloc, free, aligned_alloc");

    Arena *arena = new_arena(COMMIT_4M, 0);
    CHECK(arena != NULL);

    void *p = ARENA_MALLOC(arena, 128);
    CHECK(p != NULL);

    unsigned char *c = ARENA_CALLOC(arena, 64, 1);
    CHECK(c != NULL);

    void *r = ARENA_REALLOC(arena, p, 256);
    CHECK(r != NULL);

    void *a = ARENA_ALIGNED_ALLOC(arena, 32, 128);
    CHECK(a != NULL);
    CHECK(((uintptr_t)a & 31) == 0);

    /* free_pointer sets *ptr to NULL */
    void *fp = ARENA_MALLOC(arena, 64);
    CHECK(fp != NULL);
    ARENA_FREE_POINTER(arena, &fp);
    CHECK(fp == NULL);

    release_arena(&arena);
}

/*
 * 7. Inspection APIs
 */

static void test_inspection(void) {
    SECTION("inspection: footprint, max_footprint, allocation_count, allocation_size");

    Arena *arena = new_arena(COMMIT_4M, 0);
    CHECK(arena != NULL);

    size_t fp0     = arena_footprint(arena);
    size_t max_fp0 = arena_max_footprint(arena);
    size_t cnt0    = arena_allocation_count(arena);
    CHECK(fp0 > 0);
    CHECK(max_fp0 >= fp0);
    CHECK(cnt0 == 0);

    void *p = ARENA_MALLOC(arena, 256);
    CHECK(p != NULL);
    CHECK(arena_allocation_count(arena) == 1);
    CHECK(arena_allocation_size(p) >= 256);
    CHECK(arena_is_valid_allocation(arena, p));

    ARENA_FREE(arena, p);
    CHECK(arena_allocation_count(arena) == 0);

    release_arena(&arena);
}

/*
 * 8. Multi-region growth
 */

static void test_multi_region_growth(void) {
    SECTION("multi-region: allocate past initial commit");

    /* Start with 64 KiB commit; then allocate more than that to force growth */
    Arena *arena = new_arena(COMMIT_64K, 8);
    CHECK(arena != NULL);

    size_t fp_before = arena_footprint(arena);

    /* Allocate in chunks until we exceed the initial commit */
    size_t total = 0;
    for (int i = 0; i < 32; i++) {
        void *p = ARENA_MALLOC(arena, 4096);
        CHECK(p != NULL);
        if (p == NULL)
            break;
        total += 4096;
    }

    size_t fp_after = arena_footprint(arena);
    /* Footprint must have grown */
    CHECK_MSG(fp_after >= fp_before,
              "footprint should grow after multi-region allocation");
    CHECK(arena_max_footprint(arena) >= fp_after);

    release_arena(&arena);
}

/*
 * 9. OOM via FL_TRY/FL_CATCH on the throwing path
 */

static void test_oom_catching(void) {
    SECTION("OOM: FL_CATCH(arena_out_of_memory) on footprint-limited arena");

    Arena *arena = new_arena(COMMIT_64K, 0);
    CHECK(arena != NULL);

    /* Cap the arena at its initial commit so any growth attempt throws OOM. */
    arena_set_footprint_limit(arena, COMMIT_64K);

    bool caught = false;
    FL_TRY {
        for (int i = 0; i < 1024; i++) {
            (void)ARENA_MALLOC_THROW(arena, 4096);
        }
    }
    FL_CATCH(arena_out_of_memory) {
        caught = true;
    }
    FL_END_TRY;

    CHECK_MSG(caught, "arena_out_of_memory must be catchable via FL_CATCH");

    release_arena(&arena);
}

/*
 * 10. arena_set_footprint_limit: enforce, clear, re-enforce
 */

static void test_footprint_limit(void) {
    SECTION("footprint limit: enforce OOM, clear, allocate again");

    Arena *arena = new_arena(COMMIT_64K, 0);
    CHECK(arena != NULL);

    /* Cap at initial commit — any growth attempt must throw */
    arena_set_footprint_limit(arena, COMMIT_64K);

    bool caught = false;
    FL_TRY {
        for (int i = 0; i < 1024; i++) {
            (void)ARENA_MALLOC_THROW(arena, 4096);
        }
    }
    FL_CATCH(arena_out_of_memory) {
        caught = true;
    }
    FL_END_TRY;
    CHECK_MSG(caught, "footprint-limited arena must throw arena_out_of_memory");

    /* Clear the limit — arena can grow again; use non-throwing wrapper */
    arena_set_footprint_limit(arena, 0);
    void *p = ARENA_MALLOC(arena, 4096);
    CHECK_MSG(p != NULL, "arena must satisfy allocation after clearing footprint limit");

    release_arena(&arena);
}

/*
 * 11. FL_CATCH_STR: match by string value rather than pointer identity
 */

static void test_catch_str(void) {
    SECTION("FL_CATCH_STR: match by value; non-match rethrows");

    /* Positive: catch using a string literal instead of the reason symbol */
    bool caught = false;
    FL_TRY {
        FL_THROW(arena_out_of_memory);
    }
    FL_CATCH_STR("arena out of memory") {
        caught = true;
    }
    FL_END_TRY;
    CHECK_MSG(caught, "FL_CATCH_STR must match when string values are equal");

    /* Negative: a non-matching string must not catch; exception propagates out */
    bool wrong_caught = false;
    bool propagated   = false;
    FL_TRY {
        FL_TRY {
            FL_THROW(arena_out_of_memory);
        }
        FL_CATCH_STR("something else") {
            wrong_caught = true;
        }
        FL_END_TRY;
    }
    FL_CATCH(arena_out_of_memory) {
        propagated = true;
    }
    FL_END_TRY;
    CHECK_MSG(!wrong_caught, "FL_CATCH_STR must not catch a non-matching reason");
    CHECK_MSG(propagated, "unmatched FL_CATCH_STR must let the exception propagate");
}

/*
 * main
 */

int main(void) {
    fprintf(stdout, "arena_sa_test\n");
    test_lifecycle();
    test_throwing_malloc();
    test_throwing_calloc();
    test_throwing_realloc();
    test_throwing_aligned_alloc();
    test_non_throwing_wrappers();
    test_inspection();
    test_multi_region_growth();
    test_oom_catching();
    test_footprint_limit();
    test_catch_str();

    if (g_failures == 0) {
        fprintf(stdout, "PASS (%d tests)\n", 11);
        return 0;
    } else {
        fprintf(stdout, "FAIL (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
}
