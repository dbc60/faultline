/**
 * @file arena_malloc_throw_test.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-12-08
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "arena_malloc_test.h"

#include <faultline/fl_test.h> // FLTestCase
#include <faultline/fl_exception_types.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/fl_abbreviated_types.h> // u32
#include <faultline/size.h>                 // GIBI

#include <stddef.h> // size_t, NULL

FL_TYPE_TEST_SETUP_CLEANUP("Allocate with Exceptions - Small Range", TestRequest,
                           test_small_range_malloc_throw, setup_small_range, cleanup) {
    for (size_t req = t->lo; req < t->hi; req += CHUNK_ALIGNMENT) {
        void *mem = arena_malloc_throw(t->arena, req, __FILE__, __LINE__);

        if (mem == NULL) {
            FL_THROW(fl_internal_error);
        }

        Chunk *ch               = CHUNK_FROM_MEMORY(mem);
        size_t expected_payload = CHUNK_REQUEST_TO_PAYLOAD(req);
        size_t actual_payload   = CHUNK_PAYLOAD_SIZE(ch);
        FL_ASSERT_DETAILS(actual_payload == expected_payload,
                          "Expected payload %zd. Actual payload %zd", expected_payload,
                          actual_payload);

        arena_free_throw(t->arena, mem, __FILE__, __LINE__);
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Allocate with Exceptions - Large Range", TestRequest,
                           test_large_range_throw, setup_large_range, cleanup) {
    for (size_t req = t->lo; req < t->hi; req <<= 1) {
        void *mem = arena_malloc_throw(t->arena, req, __FILE__, __LINE__);
        FL_ASSERT_NOT_NULL(mem);

        Chunk *ch               = CHUNK_FROM_MEMORY(mem);
        size_t expected_payload = CHUNK_REQUEST_TO_PAYLOAD(req);
        size_t actual_payload   = CHUNK_PAYLOAD_SIZE(ch);
        FL_ASSERT_DETAILS(actual_payload == expected_payload,
                          "Expected payload %zd. Actual payload %zd", expected_payload,
                          actual_payload);

        arena_free_throw(t->arena, mem, __FILE__, __LINE__);
    }
}
