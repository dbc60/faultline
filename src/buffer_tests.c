/**
 * @file buffer_tests.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.2
 * @date 2025-07-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "arena.c"
#include "arena_dbg.c"
#include "../third_party/fnv/FNV64.c"
#include "set.c"
#include "fault_injector.c"
#include "buffer.c" // all functions, both static and exported
#include "digital_search_tree.c"
#include "region.c"
#include "region_node.c"
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include "fla_log_service.c"       // g_fla_log_service
#include "fla_memory_service.c"

#include <faultline/fl_test.h>   // FLTestCase, FLTestSuite
#include <faultline/fl_macros.h> // FL_UNUSED, FL_CONTAINER_OF

#include <stddef.h>

#define BUFFER_CAPACITY 10
#define ELEMENT_SIZE    sizeof(int)

// Typed-buffer wrapper for int, exercised by test_typed_buffer.
DEFINE_TYPED_BUFFER(int, int)

typedef struct TestWithArena {
    FLTestCase tc;
    Arena     *arena;
} TestWithArena;

static void setup_with_arena(FLTestCase *fltc) {
    TestWithArena *tc = FL_CONTAINER_OF(fltc, TestWithArena, tc);
    tc->arena         = new_arena(0, 0);
}

static void cleanup_with_arena(FLTestCase *fltc) {
    TestWithArena *tc = FL_CONTAINER_OF(fltc, TestWithArena, tc);
    release_arena(&tc->arena);
}

FL_TYPE_TEST_SETUP_CLEANUP("new", TestWithArena, test_new, setup_with_arena,
                           cleanup_with_arena) {
    Buffer *buf = new_buffer(t->arena, BUFFER_CAPACITY, ELEMENT_SIZE);
    FL_ASSERT_NOT_NULL(buf);
    FL_ASSERT_ZERO_SIZE_T(buffer_count(buf));
    FL_ASSERT_EQ_SIZE_T(buf->element_size, ELEMENT_SIZE);
    FL_ASSERT_TRUE(buf->capacity == BUFFER_CAPACITY);
    destroy_buffer(buf);
}

FL_TYPE_TEST_SETUP_CLEANUP("increase capacity", TestWithArena, test_increase_capacity,
                           setup_with_arena, cleanup_with_arena) {
    Buffer buf = {0};
    init_buffer(&buf, t->arena, 0, ELEMENT_SIZE);
    increase_capacity(&buf, 1);
    FL_ASSERT_EQ_SIZE_T(buf.capacity, SIZE_T_ONE);
    FL_ASSERT_NOT_NULL(buf.mem);
    increase_capacity(&buf, 2);
    FL_ASSERT_EQ_SIZE_T(buf.capacity, (size_t)3);
    FL_ASSERT_NOT_NULL(buf.mem);
    ARENA_FREE_THROW(t->arena, buf.mem);
}

typedef struct TestArray {
    FLTestCase tc;
    Arena     *arena;
    Buffer    *buf;
} TestArray;

static void setup_buf(FLTestCase *fltc) {
    TestArray *tc = FL_CONTAINER_OF(fltc, TestArray, tc);
    tc->arena     = new_arena(0, 0);
    tc->buf       = new_buffer(tc->arena, BUFFER_CAPACITY, ELEMENT_SIZE);
    FL_ASSERT_NOT_NULL(tc->buf);
}

static void cleanup_buf(FLTestCase *fltc) {
    TestArray *tc = FL_CONTAINER_OF(fltc, TestArray, tc);
    destroy_buffer(tc->buf);
    release_arena(&tc->arena);
}

FL_TYPE_TEST_SETUP_CLEANUP("count", TestArray, test_count, setup_buf, cleanup_buf) {
    FL_ASSERT_ZERO_SIZE_T(buffer_count(t->buf));
}

FL_TYPE_TEST_SETUP_CLEANUP("size", TestArray, test_size, setup_buf, cleanup_buf) {
    FL_ASSERT_EQ_SIZE_T(t->buf->element_size, ELEMENT_SIZE);
}

FL_TYPE_TEST_SETUP_CLEANUP("capacity", TestArray, test_capacity, setup_buf,
                           cleanup_buf) {
    FL_ASSERT_TRUE(t->buf->capacity == BUFFER_CAPACITY);
}

FL_TYPE_TEST_SETUP_CLEANUP("put", TestArray, test_put, setup_buf, cleanup_buf) {
    int  value = 42;
    int *p     = buffer_put(t->buf, &value);

    FL_ASSERT_NOT_NULL(p);
    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), SIZE_T_ONE);
    FL_ASSERT_EQ_INT(*p, value);
}

FL_TYPE_TEST_SETUP_CLEANUP("put grow", TestArray, test_put_grow, setup_buf,
                           cleanup_buf) {
    int  value = 42;
    int *p;

    for (size_t i = 0; i < BUFFER_CAPACITY; i++) {
        p = buffer_put(t->buf, &value);
        FL_ASSERT_NOT_NULL(p);
        FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), i + 1);
        FL_ASSERT_EQ_INT(*p, value);
        FL_ASSERT_TRUE(t->buf->capacity == BUFFER_CAPACITY);
    }

    p = buffer_put(t->buf, &value);
    FL_ASSERT_NOT_NULL(p);
    FL_ASSERT_TRUE(buffer_count(t->buf) == BUFFER_CAPACITY + 1);
    FL_ASSERT_DETAILS(t->buf->capacity == INCREASE_CAPACITY_BY + BUFFER_CAPACITY,
                      "buffer capacity should be 20, but is %zu", t->buf->capacity);
}

FL_TYPE_TEST_SETUP_CLEANUP("get", TestArray, test_get, setup_buf, cleanup_buf) {
    int  value = 42;
    int *p     = buffer_put(t->buf, &value);

    FL_ASSERT_NOT_NULL(p);
    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), SIZE_T_ONE);
    FL_ASSERT_EQ_INT(*p, value);
    int *q = buffer_get(t->buf, 0);
    FL_ASSERT_NOT_NULL(q);
    FL_ASSERT_EQ_PTR(q, p);
    FL_ASSERT_EQ_INT(*q, value);
}

FL_TYPE_TEST_SETUP_CLEANUP("get grow", TestArray, test_get_grow, setup_buf,
                           cleanup_buf) {
    int value = 42;
    for (size_t i = 0; i < BUFFER_CAPACITY; i++) {
        int *p = buffer_put(t->buf, &value);
        FL_ASSERT_NOT_NULL(p);
        FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), i + 1);
        FL_ASSERT_EQ_INT(*p, value);
    }
    int *p = buffer_put(t->buf, &value);
    FL_ASSERT_NOT_NULL(p);
    FL_ASSERT_TRUE(buffer_count(t->buf) == BUFFER_CAPACITY + 1);
    FL_ASSERT_TRUE(t->buf->capacity == INCREASE_CAPACITY_BY + BUFFER_CAPACITY);
    int *q = buffer_get(t->buf, BUFFER_CAPACITY);
    FL_ASSERT_NOT_NULL(q);
    FL_ASSERT_EQ_PTR(q, p);
    FL_ASSERT_EQ_INT(*q, value);
}

// --- new_buffer with zero capacity ---
FL_TYPE_TEST_SETUP_CLEANUP("new zero capacity", TestWithArena, test_new_zero_capacity,
                           setup_with_arena, cleanup_with_arena) {
    Buffer *buf = new_buffer(t->arena, 0, ELEMENT_SIZE);
    FL_ASSERT_NOT_NULL(buf);
    FL_ASSERT_ZERO_SIZE_T(buffer_count(buf));
    FL_ASSERT_EQ_SIZE_T(buf->element_size, ELEMENT_SIZE);
    FL_ASSERT_ZERO_SIZE_T(buf->capacity);
    FL_ASSERT_NULL(buf->mem);
    destroy_buffer(buf);
}

// --- init_buffer and buffer_is_initialized ---

FL_TYPE_TEST_SETUP_CLEANUP("init", TestWithArena, test_init, setup_with_arena,
                           cleanup_with_arena) {
    Buffer *buf = ARENA_MALLOC_THROW(t->arena, BUFFER_CAPACITY * ELEMENT_SIZE);
    init_buffer(buf, t->arena, BUFFER_CAPACITY, ELEMENT_SIZE);

    FL_ASSERT_TRUE(buffer_is_initialized(buf));
    FL_ASSERT_TRUE(buf->capacity == BUFFER_CAPACITY);
    FL_ASSERT_EQ_SIZE_T(buf->element_size, ELEMENT_SIZE);
    FL_ASSERT_ZERO_SIZE_T(buf->count);

    ARENA_FREE_THROW(t->arena, buf->mem);
}

// --- buffer_put from a zero-capacity buffer ---

FL_TYPE_TEST_SETUP_CLEANUP("put from zero capacity", TestWithArena,
                           test_put_from_zero_capacity, setup_with_arena,
                           cleanup_with_arena) {
    Buffer *buf = new_buffer(t->arena, 0, ELEMENT_SIZE);
    FL_ASSERT_NULL(buf->mem);
    FL_ASSERT_ZERO_SIZE_T(buf->capacity);

    int  value = 42;
    int *p     = buffer_put(buf, &value);
    FL_ASSERT_NOT_NULL(p);
    FL_ASSERT_EQ_INT(*p, value);
    FL_ASSERT_EQ_SIZE_T(buffer_count(buf), SIZE_T_ONE);
    FL_ASSERT_DETAILS(buf->capacity == INCREASE_CAPACITY_BY,
                      "capacity should be %d, but is %zu", INCREASE_CAPACITY_BY,
                      buf->capacity);

    destroy_buffer(buf);
}

// --- buffer_allocate_next_free_slot ---

FL_TYPE_TEST_SETUP_CLEANUP("allocate next free slot", TestArray,
                           test_allocate_next_free_slot, setup_buf, cleanup_buf) {
    int *slot = buffer_allocate_next_free_slot(t->buf);
    FL_ASSERT_NOT_NULL(slot);
    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), SIZE_T_ONE);

    *slot    = 99;
    int *at0 = buffer_get(t->buf, 0);
    FL_ASSERT_EQ_PTR(at0, slot);
    FL_ASSERT_EQ_INT(*at0, 99);
}

FL_TYPE_TEST_SETUP_CLEANUP("allocate next free slot grow", TestArray,
                           test_allocate_next_free_slot_grow, setup_buf, cleanup_buf) {
    for (size_t i = 0; i < BUFFER_CAPACITY; i++) {
        int *slot = buffer_allocate_next_free_slot(t->buf);
        FL_ASSERT_NOT_NULL(slot);
    }
    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), (size_t)BUFFER_CAPACITY);
    FL_ASSERT_EQ_SIZE_T(t->buf->capacity, (size_t)BUFFER_CAPACITY);

    int *slot = buffer_allocate_next_free_slot(t->buf);
    FL_ASSERT_NOT_NULL(slot);
    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), (size_t)BUFFER_CAPACITY + 1);
    FL_ASSERT_EQ_SIZE_T(t->buf->capacity,
                        (size_t)BUFFER_CAPACITY + INCREASE_CAPACITY_BY);
}

// --- buffer_clear ---

FL_TYPE_TEST_SETUP_CLEANUP("clear", TestArray, test_clear, setup_buf, cleanup_buf) {
    int value = 42;

    buffer_put(t->buf, &value);
    buffer_put(t->buf, &value);
    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), (size_t)2);

    buffer_clear(t->buf);
    FL_ASSERT_ZERO_SIZE_T(buffer_count(t->buf));
    FL_ASSERT_EQ_SIZE_T(t->buf->capacity, (size_t)BUFFER_CAPACITY);
    FL_ASSERT_NOT_NULL(t->buf->mem);

    // Buffer is usable again after clearing.
    int *p = buffer_put(t->buf, &value);
    FL_ASSERT_NOT_NULL(p);
    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), SIZE_T_ONE);
}

// --- buffer_clear_and_release ---

FL_TYPE_TEST_SETUP_CLEANUP("clear and release", TestArray, test_clear_and_release,
                           setup_buf, cleanup_buf) {
    int value = 42;

    buffer_put(t->buf, &value);
    buffer_put(t->buf, &value);
    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), SIZE_T_TWO);

    buffer_clear_and_release(t->buf);
    FL_ASSERT_ZERO_SIZE_T(buffer_count(t->buf));
    FL_ASSERT_ZERO_SIZE_T(t->buf->capacity);
    FL_ASSERT_NULL(t->buf->mem);
}

// --- buffer_copy ---

FL_TYPE_TEST_SETUP_CLEANUP("copy sufficient capacity", TestWithArena,
                           test_copy_sufficient_capacity, setup_with_arena,
                           cleanup_with_arena) {
    Buffer *dst = new_buffer(t->arena, BUFFER_CAPACITY, ELEMENT_SIZE);
    Buffer *src = new_buffer(t->arena, BUFFER_CAPACITY, ELEMENT_SIZE);

    for (int i = 0; i < 5; i++) {
        buffer_put(src, &i);
    }
    FL_ASSERT_EQ_SIZE_T(buffer_count(src), (size_t)5);

    size_t copied = buffer_copy(dst, src);
    FL_ASSERT_EQ_SIZE_T(copied, (size_t)5);
    FL_ASSERT_EQ_SIZE_T(buffer_count(dst), (size_t)5);
    FL_ASSERT_EQ_SIZE_T(dst->capacity, (size_t)BUFFER_CAPACITY); // no growth needed

    for (size_t i = 0; i < 5; i++) {
        int *p = buffer_get(dst, i);
        FL_ASSERT_EQ_INT((int)i, *p);
    }

    destroy_buffer(src);
    destroy_buffer(dst);
}

FL_TYPE_TEST_SETUP_CLEANUP("copy grow", TestWithArena, test_copy_grow, setup_with_arena,
                           cleanup_with_arena) {
    // dst has less free space than src has elements, so it must grow.
    Buffer *dst = new_buffer(t->arena, 2, ELEMENT_SIZE);
    Buffer *src = new_buffer(t->arena, 5, ELEMENT_SIZE);

    for (int i = 0; i < 5; i++) {
        buffer_put(src, &i);
    }

    size_t copied = buffer_copy(dst, src);
    FL_ASSERT_EQ_SIZE_T(copied, (size_t)5);
    FL_ASSERT_EQ_SIZE_T(buffer_count(dst), (size_t)5);
    FL_ASSERT_GE_SIZE_T(dst->capacity, (size_t)5);

    for (size_t i = 0; i < 5; i++) {
        int *p = buffer_get(dst, i);
        FL_ASSERT_EQ_INT((int)i, *p);
    }

    destroy_buffer(src);
    destroy_buffer(dst);
}

FL_TYPE_TEST_SETUP_CLEANUP("copy from empty", TestWithArena, test_copy_from_empty,
                           setup_with_arena, cleanup_with_arena) {
    Buffer *dst = new_buffer(t->arena, BUFFER_CAPACITY, ELEMENT_SIZE);
    Buffer *src = new_buffer(t->arena, BUFFER_CAPACITY, ELEMENT_SIZE);

    for (int i = 0; i < 3; i++) {
        buffer_put(dst, &i);
    }
    FL_ASSERT_EQ_SIZE_T(buffer_count(dst), (size_t)3);
    FL_ASSERT_ZERO_SIZE_T(buffer_count(src));

    size_t copied = buffer_copy(dst, src);
    FL_ASSERT_ZERO_SIZE_T(copied);
    FL_ASSERT_EQ_SIZE_T(buffer_count(dst), (size_t)3); // unchanged

    destroy_buffer(src);
    destroy_buffer(dst);
}

// --- buffer_get out of bounds ---

FL_TYPE_TEST_SETUP_CLEANUP("get out of bounds", TestWithArena, test_get_out_of_bounds,
                           setup_with_arena, cleanup_with_arena) {
    Buffer *buf          = new_buffer(t->arena, 1, ELEMENT_SIZE);
    int     value        = 42;
    bool volatile caught = false;

    buffer_put(buf, &value); // count == 1; valid indices are {0}

    FL_TRY {
        buffer_get(buf, 1); // one past the end
        FL_FAIL("expected fl_invalid_value to be thrown");
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;

    FL_ASSERT_TRUE(caught);
    destroy_buffer(buf);
}

// --- two automatic growth cycles ---

FL_TYPE_TEST_SETUP_CLEANUP("put double grow", TestArray, test_put_double_grow, setup_buf,
                           cleanup_buf) {
    // First grow: count reaches BUFFER_CAPACITY      -> capacity: 10 -> 22
    // Second grow: count reaches BUFFER_CAPACITY + INCREASE_CAPACITY_BY -> capacity: 22
    // -> 34
    size_t const target = BUFFER_CAPACITY + INCREASE_CAPACITY_BY + 1;
    int          value  = 42;

    for (size_t i = 0; i < target; i++) {
        int *p = buffer_put(t->buf, &value);
        FL_ASSERT_NOT_NULL(p);
        FL_ASSERT_EQ_INT(*p, value);
    }

    FL_ASSERT_EQ_SIZE_T(buffer_count(t->buf), target);
    FL_ASSERT_DETAILS(t->buf->capacity == BUFFER_CAPACITY + 2 * INCREASE_CAPACITY_BY,
                      "capacity should be %zu, but is %zu",
                      (size_t)(BUFFER_CAPACITY + 2 * INCREASE_CAPACITY_BY),
                      t->buf->capacity);
}

// --- new_buffer with zero element_size (debug-only assert) ---

FL_TYPE_TEST_SETUP_CLEANUP("new zero element size throws", TestWithArena,
                           test_new_zero_element_size_throws, setup_with_arena,
                           cleanup_with_arena) {
    bool volatile caught = false;
    FL_TRY {
        new_buffer(t->arena, BUFFER_CAPACITY, 0);
        FL_FAIL("expected assertion to be thrown");
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// --- buffer_put with NULL elem (debug-only assert) ---

FL_TYPE_TEST_SETUP_CLEANUP("put null elem throws", TestWithArena,
                           test_put_null_elem_throws, setup_with_arena,
                           cleanup_with_arena) {
    Buffer *buf          = new_buffer(t->arena, BUFFER_CAPACITY, ELEMENT_SIZE);
    bool volatile caught = false;
    FL_TRY {
        buffer_put(buf, NULL);
        FL_FAIL("expected assertion to be thrown");
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
    destroy_buffer(buf);
}

// --- buffer_copy with mismatched element_size (debug-only assert) ---

FL_TYPE_TEST_SETUP_CLEANUP("copy mismatched element size throws", TestWithArena,
                           test_copy_mismatched_element_size_throws, setup_with_arena,
                           cleanup_with_arena) {
    Buffer *dst          = new_buffer(t->arena, BUFFER_CAPACITY, sizeof(int));
    Buffer *src          = new_buffer(t->arena, BUFFER_CAPACITY, sizeof(double));
    bool volatile caught = false;
    FL_TRY {
        buffer_copy(dst, src);
        FL_FAIL("expected assertion to be thrown");
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
    destroy_buffer(src);
    destroy_buffer(dst);
}

// --- buffer_copy appends to a non-empty dst ---

FL_TYPE_TEST_SETUP_CLEANUP("copy into nonempty dst", TestWithArena,
                           test_copy_into_nonempty_dst, setup_with_arena,
                           cleanup_with_arena) {
    Buffer *dst = new_buffer(t->arena, BUFFER_CAPACITY, ELEMENT_SIZE);
    Buffer *src = new_buffer(t->arena, 5, ELEMENT_SIZE);
    int     val;

    val = 10;
    buffer_put(dst, &val);
    val = 20;
    buffer_put(dst, &val);
    val = 30;
    buffer_put(dst, &val);
    FL_ASSERT_EQ_SIZE_T(buffer_count(dst), (size_t)3);

    for (int i = 0; i < 5; i++) {
        buffer_put(src, &i);
    }
    FL_ASSERT_EQ_SIZE_T(buffer_count(src), (size_t)5);

    size_t copied = buffer_copy(dst, src);
    FL_ASSERT_EQ_SIZE_T(copied, (size_t)5);
    FL_ASSERT_EQ_SIZE_T(buffer_count(dst), (size_t)8);

    FL_ASSERT_EQ_INT(10, *(int *)buffer_get(dst, 0));
    FL_ASSERT_EQ_INT(20, *(int *)buffer_get(dst, 1));
    FL_ASSERT_EQ_INT(30, *(int *)buffer_get(dst, 2));
    for (int i = 0; i < 5; i++) {
        FL_ASSERT_EQ_INT(i, *(int *)buffer_get(dst, (size_t)(3 + i)));
    }

    destroy_buffer(src);
    destroy_buffer(dst);
}

// --- buffer_clear_and_release on a zero-capacity buffer (mem == NULL path) ---

FL_TYPE_TEST_SETUP_CLEANUP("clear and release empty", TestWithArena,
                           test_clear_and_release_empty, setup_with_arena,
                           cleanup_with_arena) {
    Buffer *buf = new_buffer(t->arena, 0, ELEMENT_SIZE);

    FL_ASSERT_NULL(buf->mem);
    FL_ASSERT_ZERO_SIZE_T(buf->capacity);

    buffer_clear_and_release(buf); // must not crash: mem is NULL, nothing to free
    FL_ASSERT_NULL(buf->mem);
    FL_ASSERT_ZERO_SIZE_T(buf->count);
    FL_ASSERT_ZERO_SIZE_T(buf->capacity);

    destroy_buffer(buf);
}

// --- DEFINE_TYPED_BUFFER: exercises the generated int typed-buffer API ---

FL_TYPE_TEST_SETUP_CLEANUP("typed buffer", TestWithArena, test_typed_buffer,
                           setup_with_arena, cleanup_with_arena) {
    intBuffer *buf = new_int_buffer(t->arena, BUFFER_CAPACITY);

    FL_ASSERT_NOT_NULL(buf);
    FL_ASSERT_ZERO_SIZE_T(int_buffer_count(buf));
    FL_ASSERT_EQ_SIZE_T(buf->capacity, (size_t)BUFFER_CAPACITY);

    int  val = 42;
    int *p   = int_buffer_put(buf, &val);
    FL_ASSERT_NOT_NULL(p);
    FL_ASSERT_EQ_INT(*p, val);
    FL_ASSERT_EQ_SIZE_T(int_buffer_count(buf), (size_t)1);

    int *q = int_buffer_get(buf, 0);
    FL_ASSERT_EQ_PTR(q, p);
    FL_ASSERT_EQ_INT(*q, val);

    int_buffer_clear(buf);
    FL_ASSERT_ZERO_SIZE_T(int_buffer_count(buf));

    destroy_int_buffer(buf);
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD_EMBEDDED(test_new)
FL_SUITE_ADD_EMBEDDED(test_increase_capacity)
FL_SUITE_ADD_EMBEDDED(test_count)
FL_SUITE_ADD_EMBEDDED(test_size)
FL_SUITE_ADD_EMBEDDED(test_capacity)
FL_SUITE_ADD_EMBEDDED(test_put)
FL_SUITE_ADD_EMBEDDED(test_put_grow)
FL_SUITE_ADD_EMBEDDED(test_get)
FL_SUITE_ADD_EMBEDDED(test_get_grow)
FL_SUITE_ADD_EMBEDDED(test_new_zero_capacity)
FL_SUITE_ADD_EMBEDDED(test_init)
FL_SUITE_ADD_EMBEDDED(test_put_from_zero_capacity)
FL_SUITE_ADD_EMBEDDED(test_allocate_next_free_slot)
FL_SUITE_ADD_EMBEDDED(test_allocate_next_free_slot_grow)
FL_SUITE_ADD_EMBEDDED(test_clear)
FL_SUITE_ADD_EMBEDDED(test_clear_and_release)
FL_SUITE_ADD_EMBEDDED(test_copy_sufficient_capacity)
FL_SUITE_ADD_EMBEDDED(test_copy_grow)
FL_SUITE_ADD_EMBEDDED(test_copy_from_empty)
FL_SUITE_ADD_EMBEDDED(test_get_out_of_bounds)
FL_SUITE_ADD_EMBEDDED(test_put_double_grow)
FL_SUITE_ADD_EMBEDDED(test_new_zero_element_size_throws)
FL_SUITE_ADD_EMBEDDED(test_put_null_elem_throws)
FL_SUITE_ADD_EMBEDDED(test_copy_mismatched_element_size_throws)
FL_SUITE_ADD_EMBEDDED(test_copy_into_nonempty_dst)
FL_SUITE_ADD_EMBEDDED(test_clear_and_release_empty)
FL_SUITE_ADD_EMBEDDED(test_typed_buffer)
FL_SUITE_END;

FL_GET_TEST_SUITE("Buffer", ts)
