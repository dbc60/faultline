/**
 * @file chunk_tests.c
 * @author Douglas Cuthbertson
 * @brief Test suite for chunk memory management
 * @version 0.2
 * @date 2025-05-09
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include "region.c"

#include "chunk.h"
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_* macros
#include <faultline/fl_macros.h>                   // FL_CONTAINER_OF
#include <faultline/fl_test.h>                     // FLTestCase
#include <faultline/size.h>                        // MAX_SIZE_T

#include <stdbool.h> // bool
#include <stddef.h>  // size_t, NULL
#include <stdint.h>  // uintptr_t

// ============================================================================
// Test fixture
// ============================================================================

typedef struct TestChunk {
    FLTestCase tc;
    FreeChunk *ch;
} TestChunk;

static FL_SETUP_FN(setup) {
    TestChunk *t         = FL_CONTAINER_OF(tc, TestChunk, tc);
    Region    *rgn       = new_region(MEBI(50), 0);
    size_t     committed = REGION_BYTES_COMMITTED(rgn);
    t->ch                = (FreeChunk *)rgn;
    free_chunk_init(t->ch, committed - CHUNK_SENTINEL_SIZE, true);
    CHUNK_SET_SENTINEL((Chunk *)t->ch);
}

static FL_CLEANUP_FN(cleanup) {
    TestChunk *t = FL_CONTAINER_OF(tc, TestChunk, tc);
    release_region((Region *)(t->ch));
}

// ============================================================================
// Compile-time constant sanity checks
// ============================================================================

FL_TEST("Constants", constants) {
    FL_ASSERT_EQ_SIZE_T(TWO_SIZE_T_SIZES, CHUNK_ALIGNMENT);
    FL_ASSERT_TRUE(CHUNK_MIN_SIZE >= sizeof(FreeChunk));
    FL_ASSERT_EQ_SIZE_T((size_t)0, CHUNK_MIN_SIZE & CHUNK_ALIGNMENT_MASK);
    FL_ASSERT_EQ_SIZE_T((size_t)0, CHUNK_ALIGNED_SIZE & CHUNK_ALIGNMENT_MASK);

    // Verify CHUNK_IS_ALIGNED works for a known-aligned address
    size_t aligned_addr = 0x1000;
    FL_ASSERT_TRUE(CHUNK_IS_ALIGNED((Chunk *)aligned_addr));

    size_t unaligned_addr = 0x1001;
    FL_ASSERT_FALSE(CHUNK_IS_ALIGNED((Chunk *)unaligned_addr));
}

// ============================================================================
// Initialization tests
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Init Free Chunk", TestChunk, init_free_chunk, setup,
                           cleanup) {
    FreeChunk *ch   = t->ch;
    size_t     size = CHUNK_SIZE(ch);

    FL_ASSERT_FALSE(CHUNK_IS_INUSE(ch));
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(ch));
    FL_ASSERT_TRUE(DLIST_IS_EMPTY(&ch->siblings));
    FL_ASSERT_EQ_SIZE_T(size, free_chunk_footer_size(ch));
}

FL_TYPE_TEST_SETUP_CLEANUP("Init Free Chunk Previous Not Inuse", TestChunk,
                           init_previous_not_inuse, setup, cleanup) {
    FreeChunk *ch           = t->ch;
    size_t     initial_size = CHUNK_SIZE(ch);

    // Split to get a second chunk, then re-init it with previous_inuse = false
    FreeChunk *rem      = free_chunk_split(ch, MEBI(10), __FILE__, __LINE__);
    size_t     rem_size = CHUNK_SIZE(rem);
    free_chunk_init(rem, rem_size, false);

    FL_ASSERT_FALSE(CHUNK_IS_INUSE(rem));
    FL_ASSERT_FALSE(CHUNK_IS_PREVIOUS_INUSE(rem));
    FL_ASSERT_TRUE(DLIST_IS_EMPTY(&rem->siblings));
    FL_ASSERT_EQ_SIZE_T(rem_size, free_chunk_footer_size(rem));
    FL_UNUSED(initial_size);
}

FL_TYPE_TEST_SETUP_CLEANUP("Init Free Chunk Min Size", TestChunk, init_min_size, setup,
                           cleanup) {
    FreeChunk *ch = t->ch;

    // Split off a minimum-size chunk
    size_t     big_size = CHUNK_SIZE(ch) - CHUNK_MIN_SIZE;
    FreeChunk *rem      = free_chunk_split(ch, big_size, __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(rem);
    FL_ASSERT_EQ_SIZE_T(CHUNK_MIN_SIZE, CHUNK_SIZE(rem));

    // Re-init at minimum size: the footer field IS the footer in this case
    free_chunk_init(rem, CHUNK_MIN_SIZE, true);
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(rem));
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(rem));
    FL_ASSERT_EQ_SIZE_T(CHUNK_MIN_SIZE, rem->footer);
}

// ============================================================================
// Split tests
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Split", TestChunk, split, setup, cleanup) {
    FreeChunk *ch = t->ch;
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(ch));
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(ch));

    size_t     initial_size = CHUNK_SIZE(ch);
    FreeChunk *rem          = free_chunk_split(ch, MEBI(10), __FILE__, __LINE__);
    size_t     new_size     = CHUNK_SIZE(ch);
    FL_ASSERT_EQ_SIZE_T(MEBI(10), new_size);

    size_t rem_size = CHUNK_SIZE(rem);
    FL_ASSERT_EQ_SIZE_T(initial_size - new_size, rem_size);

    FL_ASSERT_FALSE(CHUNK_IS_INUSE(rem));
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(rem));

    FL_ASSERT_EQ_SIZE_T(rem_size, free_chunk_footer_size(rem));
}

FL_TYPE_TEST_SETUP_CLEANUP("Split Preserves Flags", TestChunk, split_preserves_flags,
                           setup, cleanup) {
    FreeChunk *ch = t->ch;

    // The setup chunk has previous_inuse set. Verify it is preserved after split.
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(ch));
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(ch));

    FreeChunk *rem = free_chunk_split(ch, MEBI(10), __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(rem);

    // Original chunk preserves its previous_inuse flag
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(ch));
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(ch));

    // Remainder has previous_inuse set (it follows the original chunk)
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE(rem));
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(rem));
}

FL_TYPE_TEST_SETUP_CLEANUP("Split Exact Fit", TestChunk, split_exact_fit, setup,
                           cleanup) {
    FreeChunk *ch   = t->ch;
    size_t     size = CHUNK_SIZE(ch);

    // Splitting to the exact current size leaves no remainder
    FreeChunk *rem = free_chunk_split(ch, size, __FILE__, __LINE__);
    FL_ASSERT_NULL(rem);
    FL_ASSERT_EQ_SIZE_T(size, CHUNK_SIZE(ch));
}

FL_TYPE_TEST_SETUP_CLEANUP("Split Remainder Too Small", TestChunk,
                           split_remainder_too_small, setup, cleanup) {
    FreeChunk *ch   = t->ch;
    size_t     size = CHUNK_SIZE(ch);

    // Request leaves a remainder smaller than CHUNK_MIN_SIZE
    size_t new_size = size - (CHUNK_MIN_SIZE - CHUNK_ALIGNMENT);
    // Ensure new_size is aligned
    new_size = ALIGN_DOWN(new_size, CHUNK_ALIGNMENT);
    // Ensure new_size is at least CHUNK_MIN_SIZE
    if (new_size < CHUNK_MIN_SIZE) {
        new_size = CHUNK_MIN_SIZE;
    }

    FreeChunk *rem = free_chunk_split(ch, new_size, __FILE__, __LINE__);
    // Remainder is too small, so split doesn't happen (returns NULL)
    size_t remainder = size - new_size;
    if (remainder > 0 && remainder < CHUNK_MIN_SIZE) {
        FL_ASSERT_NULL(rem);
        // Chunk size is unchanged when split fails to produce a remainder
        FL_ASSERT_EQ_SIZE_T(size, CHUNK_SIZE(ch));
    }
}

FL_TYPE_TEST_SETUP_CLEANUP("Split Invalid Size Throws", TestChunk,
                           split_invalid_size_throws, setup, cleanup) {
    FreeChunk *ch   = t->ch;
    size_t     size = CHUNK_SIZE(ch);
    bool       caught;

    // new_size > chunk size
    caught = false;
    FL_TRY {
        free_chunk_split(ch, size + CHUNK_ALIGNMENT, __FILE__, __LINE__);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    // new_size < CHUNK_MIN_SIZE
    caught = false;
    FL_TRY {
        free_chunk_split(ch, CHUNK_MIN_SIZE - CHUNK_ALIGNMENT, __FILE__, __LINE__);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    // new_size not aligned
    caught = false;
    FL_TRY {
        free_chunk_split(ch, CHUNK_MIN_SIZE + 1, __FILE__, __LINE__);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================================
// Merge tests
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Merge with Previous", TestChunk, merge_previous, setup,
                           cleanup) {
    FreeChunk *ch            = t->ch;
    size_t     initial_size  = CHUNK_SIZE(ch);
    FreeChunk *rem           = free_chunk_split(t->ch, MEBI(10), __FILE__, __LINE__);
    size_t     rem_size      = CHUNK_SIZE(rem);
    size_t     previous_size = CHUNK_SIZE(ch);

    FreeChunk *merged = free_chunk_merge_previous(rem);
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(merged));
    FL_ASSERT_EQ_SIZE_T(previous_size + rem_size, CHUNK_SIZE(merged));
    FL_ASSERT_EQ_SIZE_T(initial_size, previous_size + rem_size);
    FL_ASSERT_EQ_SIZE_T(initial_size, free_chunk_footer_size(merged));
}

FL_TYPE_TEST_SETUP_CLEANUP("Merge with Next", TestChunk, merge_next, setup, cleanup) {
    FreeChunk *ch           = t->ch;
    size_t     initial_size = CHUNK_SIZE(ch);

    // Split into three: ch | middle | tail
    FreeChunk *middle_and_tail = free_chunk_split(ch, MEBI(10), __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(middle_and_tail);
    FreeChunk *tail = free_chunk_split(middle_and_tail, MEBI(10), __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(tail);

    size_t middle_size = CHUNK_SIZE(middle_and_tail);
    size_t tail_size   = CHUNK_SIZE(tail);

    // Merge middle with tail
    free_chunk_merge_next(middle_and_tail);
    FL_ASSERT_EQ_SIZE_T(middle_size + tail_size, CHUNK_SIZE(middle_and_tail));
    FL_ASSERT_EQ_SIZE_T(middle_size + tail_size,
                        free_chunk_footer_size(middle_and_tail));
    FL_UNUSED(initial_size);
}

// ============================================================================
// chunk_free / free_chunk_init_simple tests
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Chunk Free", TestChunk, chunk_free_test, setup, cleanup) {
    FreeChunk *ch = t->ch;

    // Split to get two chunks, then mark the first as in-use
    FreeChunk *rem = free_chunk_split(ch, MEBI(10), __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(rem);

    // Mark rem as in-use first so we can test freeing it
    CHUNK_SET_INUSE(rem);
    CHUNK_SET_PREVIOUS_INUSE(CHUNK_NEXT(rem));
    FL_ASSERT_TRUE(CHUNK_IS_INUSE(rem));

    // Free the chunk
    FreeChunk *freed = chunk_free((Chunk *)rem);
    FL_ASSERT_EQ_PTR((void *)rem, (void *)freed);
    FL_ASSERT_FALSE(CHUNK_IS_INUSE(freed));
    FL_ASSERT_TRUE(DLIST_IS_EMPTY(&freed->siblings));
    FL_ASSERT_EQ_SIZE_T(CHUNK_SIZE(freed), free_chunk_footer_size(freed));

    // Next chunk's previous-inuse flag should be cleared
    FL_ASSERT_FALSE(CHUNK_IS_PREVIOUS_INUSE(CHUNK_NEXT(freed)));
}

// ============================================================================
// free_chunk_set_inuse tests
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Set Inuse", TestChunk, set_inuse, setup, cleanup) {
    FreeChunk *ch = t->ch;

    // Split to get a chunk with a valid next chunk
    FreeChunk *rem = free_chunk_split(ch, MEBI(10), __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(rem);

    FL_ASSERT_FALSE(CHUNK_IS_INUSE(ch));

    free_chunk_set_inuse(ch);
    FL_ASSERT_TRUE(CHUNK_IS_INUSE(ch));

    // Next chunk (rem) should have previous-inuse set
    FL_ASSERT_TRUE(CHUNK_IS_PREVIOUS_INUSE((Chunk *)rem));
}

// ============================================================================
// Sibling operations tests
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Sibling Operations", TestChunk, sibling_ops, setup,
                           cleanup) {
    FreeChunk *ch = t->ch;

    // Split into three free chunks
    FreeChunk *ch2 = free_chunk_split(ch, MEBI(10), __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(ch2);
    FreeChunk *ch3 = free_chunk_split(ch2, MEBI(10), __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(ch3);

    // Initially no siblings
    FL_ASSERT_FALSE(free_chunk_has_siblings(ch));
    FL_ASSERT_FALSE(free_chunk_has_siblings(ch2));
    FL_ASSERT_FALSE(free_chunk_has_siblings(ch3));

    // Insert ch2 as sibling of ch
    free_chunk_insert(ch, ch2);
    FL_ASSERT_TRUE(free_chunk_has_siblings(ch));
    FL_ASSERT_TRUE(free_chunk_has_siblings(ch2));

    // Navigate: ch -> ch2 -> ch
    FL_ASSERT_EQ_PTR((void *)ch2, (void *)free_chunk_next_sibling(ch));
    FL_ASSERT_EQ_PTR((void *)ch, (void *)free_chunk_next_sibling(ch2));
    FL_ASSERT_EQ_PTR((void *)ch2, (void *)free_chunk_previous_sibling(ch));
    FL_ASSERT_EQ_PTR((void *)ch, (void *)free_chunk_previous_sibling(ch2));

    // Insert ch3 as sibling of ch
    free_chunk_insert(ch, ch3);
    FL_ASSERT_TRUE(free_chunk_has_siblings(ch3));

    // Remove ch2 from siblings
    free_chunk_remove(ch2);
    FL_ASSERT_FALSE(free_chunk_has_siblings(ch2));

    // ch and ch3 should still be siblings
    FL_ASSERT_TRUE(free_chunk_has_siblings(ch));
    FL_ASSERT_EQ_PTR((void *)ch3, (void *)free_chunk_next_sibling(ch));
    FL_ASSERT_EQ_PTR((void *)ch, (void *)free_chunk_next_sibling(ch3));

    // Remove ch3 too
    free_chunk_remove(ch3);
    FL_ASSERT_FALSE(free_chunk_has_siblings(ch));
}

// ============================================================================
// CHUNK_TO_MEMORY / CHUNK_FROM_MEMORY round-trip tests
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Memory Round Trip", TestChunk, memory_round_trip, setup,
                           cleanup) {
    FreeChunk *ch  = t->ch;
    void      *mem = CHUNK_TO_MEMORY(ch);

    // The memory pointer should be CHUNK_ALIGNED_SIZE past the chunk
    FL_ASSERT_EQ_PTR((void *)((char *)ch + CHUNK_ALIGNED_SIZE), mem);

    // Round-trip back to the chunk
    Chunk *recovered = CHUNK_FROM_MEMORY(mem);
    FL_ASSERT_EQ_PTR((void *)ch, (void *)recovered);
}

FL_TYPE_TEST_SETUP_CLEANUP("Memory From Null Throws", TestChunk, memory_from_null_throws,
                           setup, cleanup) {
    FL_UNUSED_TYPE_ARG;
    bool caught = false;
    FL_TRY {
        CHUNK_FROM_MEMORY(NULL);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TYPE_TEST_SETUP_CLEANUP("Memory From Unaligned Throws", TestChunk,
                           memory_from_unaligned_throws, setup, cleanup) {
    FL_UNUSED_TYPE_ARG;
    // Create a deliberately unaligned pointer
    char  dummy[64];
    void *unaligned = (void *)(((uintptr_t)dummy | CHUNK_ALIGNMENT_MASK) - 1);
    // Make sure it's actually unaligned
    if (((uintptr_t)unaligned & CHUNK_ALIGNMENT_MASK) == 0) {
        unaligned = (void *)((uintptr_t)unaligned + 1);
    }

    bool caught = false;
    FL_TRY {
        CHUNK_FROM_MEMORY(unaligned);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================================
// CHUNK_PAYLOAD_SIZE tests
// ============================================================================

FL_TYPE_TEST_SETUP_CLEANUP("Payload Size", TestChunk, chunk_payload_size, setup,
                           cleanup) {
    FreeChunk *ch   = t->ch;
    size_t     size = CHUNK_SIZE(ch);
    FL_ASSERT_EQ_SIZE_T(size - CHUNK_ALIGNED_SIZE, CHUNK_PAYLOAD_SIZE(ch));

    // Also verify after a split
    FreeChunk *rem = free_chunk_split(ch, MEBI(10), __FILE__, __LINE__);
    FL_ASSERT_NOT_NULL(rem);
    FL_ASSERT_EQ_SIZE_T(MEBI(10) - CHUNK_ALIGNED_SIZE, CHUNK_PAYLOAD_SIZE(ch));
    FL_ASSERT_EQ_SIZE_T(CHUNK_SIZE(rem) - CHUNK_ALIGNED_SIZE, CHUNK_PAYLOAD_SIZE(rem));
}

// ============================================================================
// Request-to-size macro tests
// ============================================================================

/*
 * request_to_size_is_valid is called by request_to_size.
 */
static void request_to_size_is_valid(size_t request) {
    size_t size = CHUNK_SIZE_FROM_REQUEST(request);

    // Ensure the request didn't shrink
    FL_ASSERT_GE_SIZE_T(size, request);

    // If the request is no more than the minimum size for a Chunk, then ensure size is
    // at least the minimum. Otherwise, ensure the chunk is large enough to satisfy the
    // request.
    if (request <= CHUNK_FOOTER_SIZE) {
        FL_ASSERT_EQ_SIZE_T(CHUNK_MIN_SIZE, size);
    } else {
        FL_ASSERT_GE_SIZE_T(size, CHUNK_MIN_SIZE);
        FL_ASSERT_EQ_SIZE_T((size_t)0, size & CHUNK_ALIGNMENT_MASK);
    }
}

/**
 * @brief the maximum request size should be less than MAX_SIZE_T ((2^64) - 1) and should
 * be one less than a request that would cause CHUNK_SIZE_FROM_REQUEST to wrap around
 * from very large 64-bit values to zero.
 */
FL_TEST("Maximum Request", max_request) {
    size_t actual = CHUNK_SIZE_FROM_REQUEST(CHUNK_MAX_REQUEST);
    FL_ASSERT_LT_SIZE_T(actual, MAX_SIZE_T);
    FL_ASSERT_GT_SIZE_T(actual, CHUNK_MIN_SIZE);

    actual = CHUNK_SIZE_FROM_REQUEST(CHUNK_MAX_REQUEST + 1);
    FL_ASSERT_EQ_SIZE_T((size_t)0, actual);
}

/**
 * @brief test_request_to_size exercises CHUNK_SIZE_FROM_REQUEST from 0 to 8K, which
 * should be a great enough range to ensure this macro is correct.
 */
FL_TEST("Request to Size", request_to_size) {
    for (size_t req = 0; req <= KIBI(8); req++) {
        request_to_size_is_valid(req);
    }
}

FL_TEST("Request out of range", request_out_of_range) {
    size_t request;

    // Test some small values to ensure range checking is working
    for (request = 0; request < 0x1000; request += offsetof(FreeChunk, siblings)) {
        FL_ASSERT_FALSE(CHUNK_REQUEST_OUT_OF_RANGE(request));
    }

    // This should be the maximum in-range request
    request = CHUNK_MAX_REQUEST;
    FL_ASSERT_FALSE(CHUNK_REQUEST_OUT_OF_RANGE(request));

    // Increment the request to make it out of range
    request++;
    FL_ASSERT_TRUE(CHUNK_REQUEST_OUT_OF_RANGE(request));
}

/**
 * @brief show that a request upto the footer size is padded out to the same size as a
 * minimal chunk.
 */
FL_TEST("Pad Request", pad_request) {
    FL_ASSERT_EQ_SIZE_T(CHUNK_MIN_SIZE, CHUNK_SIZE_FROM_REQUEST(CHUNK_FOOTER_SIZE));
}

/**
 * @brief The payload should be the size of a Chunk large enough to satisfy the request
 * less the size of a ChunkHeader.
 */
FL_TEST("Payload vs Chunk Size", payload_vs_chunk_size) {
    for (size_t req = 0; req < KIBI(64); req++) {
        size_t chunk_size = CHUNK_SIZE_FROM_REQUEST(req);
        size_t expected   = chunk_size - CHUNK_ALIGNED_SIZE;
        size_t actual     = CHUNK_REQUEST_TO_PAYLOAD(req);
        FL_ASSERT_EQ_SIZE_T(expected, actual);
    }
}

// ============================================================================
// CHUNK_PRODUCT_OUT_OF_RANGE tests
// ============================================================================

FL_TEST("Product out of range", product_out_of_range) {
    // Small products should be in range
    FL_ASSERT_FALSE(CHUNK_PRODUCT_OUT_OF_RANGE(1, 1));
    FL_ASSERT_FALSE(CHUNK_PRODUCT_OUT_OF_RANGE(100, 100));
    FL_ASSERT_FALSE(CHUNK_PRODUCT_OUT_OF_RANGE(1, CHUNK_MAX_REQUEST));
    FL_ASSERT_FALSE(CHUNK_PRODUCT_OUT_OF_RANGE(CHUNK_MAX_REQUEST, 1));

    // Products that overflow should be out of range
    FL_ASSERT_TRUE(CHUNK_PRODUCT_OUT_OF_RANGE(CHUNK_MAX_REQUEST, 2));
    FL_ASSERT_TRUE(CHUNK_PRODUCT_OUT_OF_RANGE(2, CHUNK_MAX_REQUEST));
    FL_ASSERT_TRUE(CHUNK_PRODUCT_OUT_OF_RANGE(MAX_SIZE_T, MAX_SIZE_T));
}

// ============================================================================
// Suite registration
// ============================================================================

FL_SUITE_BEGIN(chunks)
FL_SUITE_ADD(constants)
FL_SUITE_ADD_EMBEDDED(init_free_chunk)
FL_SUITE_ADD_EMBEDDED(init_previous_not_inuse)
FL_SUITE_ADD_EMBEDDED(init_min_size)
FL_SUITE_ADD_EMBEDDED(split)
FL_SUITE_ADD_EMBEDDED(split_preserves_flags)
FL_SUITE_ADD_EMBEDDED(split_exact_fit)
FL_SUITE_ADD_EMBEDDED(split_remainder_too_small)
FL_SUITE_ADD_EMBEDDED(split_invalid_size_throws)
FL_SUITE_ADD_EMBEDDED(merge_previous)
FL_SUITE_ADD_EMBEDDED(merge_next)
FL_SUITE_ADD_EMBEDDED(chunk_free_test)
FL_SUITE_ADD_EMBEDDED(set_inuse)
FL_SUITE_ADD_EMBEDDED(sibling_ops)
FL_SUITE_ADD_EMBEDDED(memory_round_trip)
FL_SUITE_ADD_EMBEDDED(memory_from_null_throws)
FL_SUITE_ADD_EMBEDDED(memory_from_unaligned_throws)
FL_SUITE_ADD_EMBEDDED(chunk_payload_size)
FL_SUITE_ADD(max_request)
FL_SUITE_ADD(request_to_size)
FL_SUITE_ADD(request_out_of_range)
FL_SUITE_ADD(pad_request)
FL_SUITE_ADD(payload_vs_chunk_size)
FL_SUITE_ADD(product_out_of_range)
FL_SUITE_END;

FL_GET_TEST_SUITE("Chunk", chunks);
