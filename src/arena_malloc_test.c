/**
 * @file arena_malloc_test.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2025-03-18
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "arena_internal.h"
#include "arena_dbg.h"
#include "arena_malloc_test.h"
#include "chunk.h"           // CHUNK_ALIGNMENT
#include <faultline/dlist.h> // DList

#include <faultline/fl_test.h> // FLTestCase
#include <faultline/fl_exception_types.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/fl_abbreviated_types.h> // u32
#include <faultline/arena_malloc.h>         // Arena Malloc API
#include <faultline/size.h>                 // GIBI

#include <stddef.h> // size_t, NULL

// just to be sure that the global arena is initialized at the right time.
static FLExceptionReason const setup_failure_malloc_test = "setup failure";

void setup_small_range(FLTestCase *btc) {
    TestRequest *tr = FL_CONTAINER_OF(btc, TestRequest, tc);
    tr->lo          = 0;
    tr->hi          = 1000;
    tr->arena       = new_arena(0, 0);
}

void cleanup(FLTestCase *btc) {
    TestRequest *tr = FL_CONTAINER_OF(btc, TestRequest, tc);

    release_arena(&tr->arena);
    FL_ASSERT_NULL(tr->arena);
}

void setup_large_range(FLTestCase *btc) {
    TestRequest *tr = FL_CONTAINER_OF(btc, TestRequest, tc);
    tr->lo          = ARENA_MAX_SMALL_CHUNK;
    tr->hi          = GIBI(2);

    // make sure the arena is large enough
    tr->arena = new_arena(tr->hi, 0);
}

FL_TYPE_TEST_SETUP_CLEANUP("Allocate - Small Range", TestRequest,
                           test_small_range_malloc, setup_small_range, cleanup) {
    for (size_t req = t->lo; req < t->hi; req += CHUNK_ALIGNMENT) {
        void *mem = arena_malloc(t->arena, req, __FILE__, __LINE__);
        FL_ASSERT_NOT_NULL(mem);

        Chunk *ch               = CHUNK_FROM_MEMORY(mem);
        size_t expected_payload = CHUNK_REQUEST_TO_PAYLOAD(req);
        size_t actual_payload   = CHUNK_PAYLOAD_SIZE(ch);
        FL_ASSERT_DETAILS(actual_payload == expected_payload,
                          "Expected payload %zd. Actual payload %zd", expected_payload,
                          actual_payload);

        arena_free(t->arena, mem, __FILE__, __LINE__);
    }
}

typedef struct DListI64 {
    DList link;
    i64   value;
} DListI64;

static void setup_top(FLTestCase *btc) {
    TestRequest *tr = FL_CONTAINER_OF(btc, TestRequest, tc);
    tr->lo          = 0;
    tr->hi          = 0;

    tr->arena = new_arena(0, 0);
    ARENA_CHECK_TOP_CHUNK(tr->arena);
}

// Allocate all of the top chunk and verify the chunk and the new top
FL_TYPE_TEST_SETUP_CLEANUP("Allocate - Top", TestRequest, test_top, setup_top, cleanup) {
    Chunk *all = arena_malloc_throw(t->arena, CHUNK_PAYLOAD_SIZE(t->arena->top),
                                    __FILE__, __LINE__);
    ARENA_CHECK_INUSE_CHUNK(t->arena, CHUNK_FROM_MEMORY(all));
    ARENA_CHECK_TOP_CHUNK(t->arena);
}

static void setup_many(FLTestCase *btc) {
    TestRequest *tr = FL_CONTAINER_OF(btc, TestRequest, tc);
    tr->lo          = 0;
    tr->hi          = MEBI(1);

    size_t const granularity = arena_granularity();
    size_t const page_size   = arena_page_size();

    // every 4096 bytes, arena_allocate_top will allocate a 64-byte chunk to satisfy a
    // 48-byte request. This is an attempt to compensate for that to ensure we reserve
    // enough memory for the unit tests.
    u32 extra = (u32)(ALIGN_UP((tr->hi + 1) * CHUNK_SIZE_FROM_REQUEST(sizeof(DListI64)),
                               page_size));
    size_t reserve_bytes
        = ALIGN_UP((tr->hi + 1) * CHUNK_SIZE_FROM_REQUEST(sizeof(DListI64)), granularity)
          + extra;
    u32 reserve = (u32)(reserve_bytes / granularity);
    tr->arena   = new_arena(0, reserve);
}

// delete in the same order in which items were allocated
FL_TYPE_TEST_SETUP_CLEANUP("In-Order Delete", TestRequest, test_in_order_delete,
                           setup_many, cleanup) {
    DListI64 *head = arena_malloc_throw(t->arena, sizeof(*head), __FILE__, __LINE__);

    DLIST_INIT(&head->link);
    head->value = -1;

    for (size_t i = 0; i < t->hi; i++) {
        DListI64 *node = NULL;

        FL_TRY {
            node = arena_malloc_throw(t->arena, sizeof(*node), __FILE__, __LINE__);
        }
        FL_CATCH(arena_out_of_memory) {
            char details[1024];
            strcpy_s(details, sizeof details, FL_DETAILS);
            strcat_s(details, sizeof details, "Failed on iteration %zu");
            FL_THROW_DETAILS_FILE_LINE(FL_REASON, details, FL_FILE, FL_LINE, i);
        }
        FL_CATCH_ALL {
            char details[1024];
            strcpy_s(details, sizeof details, FL_DETAILS);
            strcat_s(details, sizeof details, "Failed on iteration %zu");
            printf("Arena Malloc Test: Test In Order Delete. %s, Failed on iteration "
                   "%zu at %s[%d]\n",
                   FL_REASON, i, FL_FILE, FL_LINE);
            FL_THROW_DETAILS_FILE_LINE(FL_REASON, details, FL_FILE, FL_LINE, i);
        }
        FL_END_TRY;

        DLIST_INIT(&node->link);
        node->value = i;
        DLIST_INSERT_NEXT(&head->link, &node->link);
    }

    for (i64 i = (i64)t->lo; i < (i64)t->hi; i++) {
        DListI64 *node = FL_CONTAINER_OF(DLIST_LAST(&head->link), DListI64, link);

        DLIST_REMOVE(&node->link);
        FL_ASSERT_DETAILS(node->value == i, "Expected %zu. Actual %zd", i, node->value);
        arena_free_throw(t->arena, node, __FILE__, __LINE__);
    }
}

// delete in the reverse order in which items were allocated
FL_TYPE_TEST_SETUP_CLEANUP("Reverse-Order Delete", TestRequest,
                           test_reverse_order_delete, setup_many, cleanup) {
    DListI64 *head = arena_malloc_throw(t->arena, sizeof(*head), __FILE__, __LINE__);

    DLIST_INIT(&head->link);
    head->value = -1;

    for (size_t i = 0; i < t->hi; i++) {
        DListI64 *node = NULL;

        FL_TRY {
            node = arena_malloc_throw(t->arena, sizeof(*node), __FILE__, __LINE__);
        }
        FL_CATCH(arena_out_of_memory) {
            char details[1024];
            strcpy_s(details, sizeof details, FL_DETAILS);
            strcat_s(details, sizeof details, "Failed on iteration %zu");
            FL_THROW_DETAILS_FILE_LINE(FL_REASON, details, FL_FILE, FL_LINE, i);
        }
        FL_END_TRY;

        DLIST_INIT(&node->link);
        node->value = i;
        DLIST_INSERT_NEXT(&head->link, &node->link);
    }

    for (i64 i = (i64)t->hi - 1; i >= (i64)t->lo; i--) {
        DListI64 *node = FL_CONTAINER_OF(DLIST_FIRST(&head->link), DListI64, link);
        DLIST_REMOVE(&node->link);
        FL_ASSERT_DETAILS(node->value == i, "Expected %zu. Actual %zd", i, node->value);

        arena_free_throw(t->arena, node, __FILE__, __LINE__);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Allocate - Large Range", TestRequest, test_large_range,
                           setup_large_range, cleanup) {
    for (size_t req = t->lo; req < t->hi; req <<= 1) {
        void *mem = arena_malloc(t->arena, req, __FILE__, __LINE__);
        FL_ASSERT_NOT_NULL(mem);

        Chunk *ch               = CHUNK_FROM_MEMORY(mem);
        size_t expected_payload = CHUNK_REQUEST_TO_PAYLOAD(req);
        size_t actual_payload   = CHUNK_PAYLOAD_SIZE(ch);
        FL_ASSERT_DETAILS(actual_payload == expected_payload,
                          "Expected payload %zd. Actual payload %zd", expected_payload,
                          actual_payload);

        arena_free(t->arena, mem, __FILE__, __LINE__);
    }
}

FL_TEST("Merge Backward", merge_backward) {
    Arena *arena = new_arena(0, 0);
    void  *a     = arena_malloc(arena, 4096, __FILE__, __LINE__);
    void  *b     = arena_malloc(arena, 4096, __FILE__, __LINE__);
    void  *c     = arena_malloc(arena, 4096, __FILE__, __LINE__);
    void  *top_blocker
        = arena_malloc(arena, 4096, __FILE__, __LINE__); // keep c off the top
    DigitalSearchTree *dst_a = ARENA_CHUNK_TO_DST(CHUNK_FROM_MEMORY(a));
    DigitalSearchTree *dst_c = ARENA_CHUNK_TO_DST(CHUNK_FROM_MEMORY(c));

    arena_free(arena, c, __FILE__, __LINE__);
    arena_free(arena, a, __FILE__, __LINE__);
    FL_ASSERT_TRUE(DST_HAS_SIBLINGS(dst_c));
    FL_ASSERT_TRUE(DST_HAS_SIBLINGS(dst_a));
    FL_ASSERT_EQ_PTR(DST_NEXT_SIBLING(dst_c), dst_a);
    FL_ASSERT_EQ_PTR(DST_NEXT_SIBLING(dst_a), dst_c);

    arena_free(arena, b, __FILE__, __LINE__);
    size_t chunk_size = CHUNK_SIZE_FROM_REQUEST(4096);
    FL_ASSERT_EQ_SIZE_T(CHUNK_SIZE((Chunk *)dst_a), 3 * chunk_size); // all three merged
    FL_ASSERT_TRUE(DST_IS_ROOT(dst_a));                              // back in a bin
    FL_ASSERT_TRUE(DST_IS_LEAF(dst_a));       // only chunk of this size
    FL_ASSERT_TRUE(!DST_HAS_SIBLINGS(dst_a)); // no same-size neighbors

    arena_free(arena, top_blocker, __FILE__, __LINE__);
}

FL_TEST("Merge Forward", merge_forward) {
    Arena             *arena       = new_arena(0, 0);
    void              *p           = arena_malloc(arena, 4096, __FILE__, __LINE__);
    void              *x           = arena_malloc(arena, 512, __FILE__, __LINE__);
    void              *b           = arena_malloc(arena, 4096, __FILE__, __LINE__);
    void              *c           = arena_malloc(arena, 4096, __FILE__, __LINE__);
    void              *top_blocker = arena_malloc(arena, 4096, __FILE__, __LINE__);
    DigitalSearchTree *dst_p       = ARENA_CHUNK_TO_DST(CHUNK_FROM_MEMORY(p));
    DigitalSearchTree *dst_b       = ARENA_CHUNK_TO_DST(CHUNK_FROM_MEMORY(b));
    DigitalSearchTree *dst_c       = ARENA_CHUNK_TO_DST(CHUNK_FROM_MEMORY(c));

    arena_free(arena, p, __FILE__, __LINE__);
    arena_free(arena, c, __FILE__, __LINE__);

    // Verify sibling setup before triggering the merge
    FL_ASSERT_TRUE(DST_HAS_SIBLINGS(dst_p));
    FL_ASSERT_TRUE(DST_HAS_SIBLINGS(dst_c));
    FL_ASSERT_EQ_PTR(DST_NEXT_SIBLING(dst_p), dst_c);
    FL_ASSERT_EQ_PTR(DST_NEXT_SIBLING(dst_c), dst_p);

    arena_free(arena, b, __FILE__, __LINE__); // b merges forward with c (a sibling)

    size_t chunk_size = CHUNK_SIZE_FROM_REQUEST(4096);
    FL_ASSERT_EQ_SIZE_T(CHUNK_SIZE((Chunk *)dst_b), 2 * chunk_size); // b+c merged
    FL_ASSERT_TRUE(DST_IS_ROOT(dst_b));
    FL_ASSERT_TRUE(DST_IS_LEAF(dst_b));
    FL_ASSERT_TRUE(!DST_HAS_SIBLINGS(dst_b));
    FL_ASSERT_EQ_SIZE_T(CHUNK_SIZE((Chunk *)dst_p), chunk_size); // p untouched
    FL_ASSERT_TRUE(DST_IS_ROOT(dst_p));
    FL_ASSERT_TRUE(DST_IS_LEAF(dst_p));
    FL_ASSERT_TRUE(!DST_HAS_SIBLINGS(dst_p));

    arena_free(arena, top_blocker, __FILE__, __LINE__);
    arena_free(arena, x, __FILE__, __LINE__);
}
