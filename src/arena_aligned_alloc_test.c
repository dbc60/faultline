/**
 * @file arena_aligned_alloc_test.c
 * @author Douglas Cuthbertson
 * @brief Tests for arena_aligned_alloc and arena_aligned_alloc_throw.
 * @version 0.1
 * @date 2026-02-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/arena.h>
#include "arena_malloc_test.h"
#include "chunk.h"

#include <faultline/fl_test.h>
#include <faultline/fl_exception_types.h>
#include <faultline/fl_exception_service_assert.h>
#include <faultline/arena_malloc.h>

#include <stddef.h>  // size_t, NULL
#include <stdint.h>  // uintptr_t
#include <stdbool.h> // bool

// Verify that alignment <= CHUNK_ALIGNMENT falls through to the normal malloc path
// and the returned pointer is at least CHUNK_ALIGNMENT-aligned.
FL_TYPE_TEST_SETUP_CLEANUP("Aligned Alloc - Natural Alignment", TestRequest,
                           test_aligned_alloc_natural, setup_small_range, cleanup) {
    size_t const alignments[] = {1, 2, 4, 8, CHUNK_ALIGNMENT};

    for (size_t i = 0; i < sizeof alignments / sizeof alignments[0]; i++) {
        size_t alignment = alignments[i];
        void  *mem
            = arena_aligned_alloc_throw(t->arena, alignment, 64, __FILE__, __LINE__);
        FL_ASSERT_DETAILS(mem != NULL, "Expected non-null pointer for alignment %zu",
                          alignment);
        FL_ASSERT_DETAILS(((uintptr_t)mem & (CHUNK_ALIGNMENT - 1)) == 0,
                          "Expected %zu-byte aligned ptr for alignment %zu, got 0x%p",
                          (size_t)CHUNK_ALIGNMENT, alignment, mem);
        arena_free_throw(t->arena, mem, __FILE__, __LINE__);
    }
}

// Verify alignment = 32 (2x CHUNK_ALIGNMENT) works across a range of sizes.
// The first allocation in a fresh arena typically triggers the gap-too-small
// adjustment path (gap = 16 < CHUNK_MIN_SIZE), exercising that branch.
FL_TYPE_TEST_SETUP_CLEANUP("Aligned Alloc - 32-byte Alignment", TestRequest,
                           test_aligned_alloc_32, setup_small_range, cleanup) {
    size_t const alignment = 32;

    for (size_t size = CHUNK_ALIGNMENT; size <= 512; size += CHUNK_ALIGNMENT) {
        void *mem
            = arena_aligned_alloc_throw(t->arena, alignment, size, __FILE__, __LINE__);
        FL_ASSERT_DETAILS(mem != NULL, "Expected non-null for size %zu alignment %zu",
                          size, alignment);
        FL_ASSERT_DETAILS(((uintptr_t)mem & (alignment - 1)) == 0,
                          "Expected %zu-byte aligned ptr, got 0x%p", alignment, mem);
        arena_free_throw(t->arena, mem, __FILE__, __LINE__);
    }
}

// Verify alignment = 64 across a range of sizes.
FL_TYPE_TEST_SETUP_CLEANUP("Aligned Alloc - 64-byte Alignment", TestRequest,
                           test_aligned_alloc_64, setup_small_range, cleanup) {
    size_t const alignment = 64;

    for (size_t size = CHUNK_ALIGNMENT; size <= 512; size += CHUNK_ALIGNMENT) {
        void *mem
            = arena_aligned_alloc_throw(t->arena, alignment, size, __FILE__, __LINE__);
        FL_ASSERT_DETAILS(mem != NULL, "Expected non-null for size %zu alignment %zu",
                          size, alignment);
        FL_ASSERT_DETAILS(((uintptr_t)mem & (alignment - 1)) == 0,
                          "Expected %zu-byte aligned ptr, got 0x%p", alignment, mem);
        arena_free_throw(t->arena, mem, __FILE__, __LINE__);
    }
}

// Verify alignment = 128 across a range of sizes.
FL_TYPE_TEST_SETUP_CLEANUP("Aligned Alloc - 128-byte Alignment", TestRequest,
                           test_aligned_alloc_128, setup_small_range, cleanup) {
    size_t const alignment = 128;

    for (size_t size = CHUNK_ALIGNMENT; size <= 512; size += CHUNK_ALIGNMENT) {
        void *mem
            = arena_aligned_alloc_throw(t->arena, alignment, size, __FILE__, __LINE__);
        FL_ASSERT_DETAILS(mem != NULL, "Expected non-null for size %zu alignment %zu",
                          size, alignment);
        FL_ASSERT_DETAILS(((uintptr_t)mem & (alignment - 1)) == 0,
                          "Expected %zu-byte aligned ptr, got 0x%p", alignment, mem);
        arena_free_throw(t->arena, mem, __FILE__, __LINE__);
    }
}

// Verify multiple aligned allocations can coexist and all are correctly aligned.
FL_TYPE_TEST_SETUP_CLEANUP("Aligned Alloc - Multiple Live Allocations", TestRequest,
                           test_aligned_alloc_multiple, setup_small_range, cleanup) {
    size_t const count     = 8;
    size_t const alignment = 64;
    void        *ptrs[8];

    for (size_t i = 0; i < count; i++) {
        ptrs[i]
            = arena_aligned_alloc_throw(t->arena, alignment, 128, __FILE__, __LINE__);
        FL_ASSERT_DETAILS(ptrs[i] != NULL, "Expected non-null for allocation %zu", i);
        FL_ASSERT_DETAILS(((uintptr_t)ptrs[i] & (alignment - 1)) == 0,
                          "Expected %zu-byte aligned ptr [%zu], got 0x%p", alignment, i,
                          ptrs[i]);
    }

    for (size_t i = 0; i < count; i++) {
        arena_free_throw(t->arena, ptrs[i], __FILE__, __LINE__);
    }
}

// Verify the non-throwing variant returns NULL when a bad alignment is passed
// (instead of propagating the exception to the caller).
FL_TYPE_TEST_SETUP_CLEANUP("Aligned Alloc - Non-throw Invalid Alignment", TestRequest,
                           test_aligned_alloc_non_throw_invalid, setup_small_range,
                           cleanup) {
    // alignment = 3 is not a power of 2; arena_aligned_alloc should return NULL
    void *mem = arena_aligned_alloc(t->arena, 3, 64, __FILE__, __LINE__);
    FL_ASSERT_DETAILS(mem == NULL,
                      "Expected NULL for non-power-of-2 alignment, got 0x%p", mem);
}

// Verify the throwing variant actually throws for invalid (non-power-of-2) alignment.
FL_TYPE_TEST_SETUP_CLEANUP("Aligned Alloc - Throw on Invalid Alignment", TestRequest,
                           test_aligned_alloc_throw_invalid, setup_small_range,
                           cleanup) {
    bool volatile threw = false;

    FL_TRY {
        (void)arena_aligned_alloc_throw(t->arena, 3, 64, __FILE__, __LINE__);
    }
    FL_CATCH_ALL {
        threw = true;
    }
    FL_END_TRY;

    FL_ASSERT_DETAILS(threw, "Expected exception for non-power-of-2 alignment");
}

// Verify alignment = 0 also throws.
FL_TYPE_TEST_SETUP_CLEANUP("Aligned Alloc - Throw on Zero Alignment", TestRequest,
                           test_aligned_alloc_throw_zero, setup_small_range, cleanup) {
    bool volatile threw = false;

    FL_TRY {
        (void)arena_aligned_alloc_throw(t->arena, 0, 64, __FILE__, __LINE__);
    }
    FL_CATCH_ALL {
        threw = true;
    }
    FL_END_TRY;

    FL_ASSERT_DETAILS(threw, "Expected exception for zero alignment");
}
