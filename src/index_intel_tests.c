/**
 * @file index_intel_tests.c
 * @author Douglas Cuthbertson
 * @brief Tests for index_intel.h bit-scan macros, index-by-value macros/function,
 * and bin limit macros.
 * @version 0.1
 * @date 2026-02-16
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service

#include "index_intel.h"
#include <faultline/fl_exception_service_assert.h> // FL_ASSERT_* macros
#include <faultline/fl_macros.h>                   // FL_SPEC_EXPORT, FL_ARRAY_COUNT
#include <faultline/fl_test.h>                     // FL_SUITE_*, FL_GET_TEST_SUITE

// ===========================================================================
// COMPUTE_LSB2IDX32
// ===========================================================================

FL_TEST("COMPUTE_LSB2IDX32 single bit", lsb2idx32_single_bit) {
    u32 actual;
    for (u32 i = 0; i < U32_BIT; i++) {
        u32 bit = 1ul << i;
        COMPUTE_LSB2IDX32(bit, actual);
        FL_ASSERT_EQ_UINT32(i, actual);
    }
}

FL_TEST("COMPUTE_LSB2IDX32 edge cases", lsb2idx32_edge_cases) {
    u32 actual;

    // Zero input returns zero
    COMPUTE_LSB2IDX32(0, actual);
    FL_ASSERT_EQ_UINT32(0, actual);

    // Multi-bit: LSB of 0x06 (binary 110) is bit 1
    COMPUTE_LSB2IDX32(0x06, actual);
    FL_ASSERT_EQ_UINT32(1, actual);

    // Multi-bit: LSB of 0x30 (binary 110000) is bit 4
    COMPUTE_LSB2IDX32(0x30, actual);
    FL_ASSERT_EQ_UINT32(4, actual);

    // All bits set: LSB is bit 0
    COMPUTE_LSB2IDX32(0xFFFFFFFF, actual);
    FL_ASSERT_EQ_UINT32(0, actual);
}

// ===========================================================================
// COMPUTE_LSB2IDX64
// ===========================================================================

FL_TEST("COMPUTE_LSB2IDX64 single bit", lsb2idx64_single_bit) {
    u32 actual;
    for (u32 i = 0; i < U64_BIT; i++) {
        u64 bit = 1ull << i;
        COMPUTE_LSB2IDX64(bit, actual);
        FL_ASSERT_EQ_UINT32(i, actual);
    }
}

FL_TEST("COMPUTE_LSB2IDX64 edge cases", lsb2idx64_edge_cases) {
    u32 actual;

    // Zero input returns zero
    COMPUTE_LSB2IDX64(0, actual);
    FL_ASSERT_EQ_UINT32(0, actual);

    // Multi-bit: LSB of 0x06 is bit 1
    COMPUTE_LSB2IDX64(0x06ull, actual);
    FL_ASSERT_EQ_UINT32(1, actual);

    // Single high bit: LSB of 0x8000000000000000 is bit 63
    COMPUTE_LSB2IDX64(0x8000000000000000ull, actual);
    FL_ASSERT_EQ_UINT32(63, actual);

    // All bits set: LSB is bit 0
    COMPUTE_LSB2IDX64(0xFFFFFFFFFFFFFFFFull, actual);
    FL_ASSERT_EQ_UINT32(0, actual);
}

// ===========================================================================
// COMPUTE_MSB2IDX32
// ===========================================================================

FL_TEST("COMPUTE_MSB2IDX32 single bit", msb2idx32_single_bit) {
    u32 actual;
    for (u32 i = 0; i < U32_BIT; i++) {
        u32 bit = 1ul << i;
        COMPUTE_MSB2IDX32(bit, actual);
        FL_ASSERT_EQ_UINT32(i, actual);
    }
}

FL_TEST("COMPUTE_MSB2IDX32 edge cases", msb2idx32_edge_cases) {
    u32 actual;

    // Zero input returns zero
    COMPUTE_MSB2IDX32(0, actual);
    FL_ASSERT_EQ_UINT32(0, actual);

    // Multi-bit: MSB of 0x06 (binary 110) is bit 2
    COMPUTE_MSB2IDX32(0x06, actual);
    FL_ASSERT_EQ_UINT32(2, actual);

    // Multi-bit: MSB of 0x30 (binary 110000) is bit 5
    COMPUTE_MSB2IDX32(0x30, actual);
    FL_ASSERT_EQ_UINT32(5, actual);

    // All bits set: MSB is bit 31
    COMPUTE_MSB2IDX32(0xFFFFFFFF, actual);
    FL_ASSERT_EQ_UINT32(31, actual);
}

// ===========================================================================
// COMPUTE_MSB2IDX64
// ===========================================================================

FL_TEST("COMPUTE_MSB2IDX64 single bit", msb2idx64_single_bit) {
    u32 actual;
    for (u32 i = 0; i < U64_BIT; i++) {
        u64 bit = 1ull << i;
        COMPUTE_MSB2IDX64(bit, actual);
        FL_ASSERT_EQ_UINT32(i, actual);
    }
}

FL_TEST("COMPUTE_MSB2IDX64 edge cases", msb2idx64_edge_cases) {
    u32 actual;

    // Zero input returns zero
    COMPUTE_MSB2IDX64(0, actual);
    FL_ASSERT_EQ_UINT32(0, actual);

    // Multi-bit: MSB of 0x06 is bit 2
    COMPUTE_MSB2IDX64(0x06ull, actual);
    FL_ASSERT_EQ_UINT32(2, actual);

    // Single high bit: MSB of 0x8000000000000000 is bit 63
    COMPUTE_MSB2IDX64(0x8000000000000000ull, actual);
    FL_ASSERT_EQ_UINT32(63, actual);

    // All bits set: MSB is bit 63
    COMPUTE_MSB2IDX64(0xFFFFFFFFFFFFFFFFull, actual);
    FL_ASSERT_EQ_UINT32(63, actual);
}

// ===========================================================================
// INDEX_BY_VALUE64 macro
// ===========================================================================

// These test values come from the worked examples in index_by_value's doc comment.
FL_TEST("INDEX_BY_VALUE64 doc examples", index_by_value64_examples) {
    u32 idx;

    // val=1024, cnt=48, exp=10 -> idx=0
    INDEX_BY_VALUE64(1024ull, 48, 10, idx);
    FL_ASSERT_EQ_UINT32(0, idx);

    // val=1536, cnt=48, exp=10 -> idx=1
    INDEX_BY_VALUE64(1536ull, 48, 10, idx);
    FL_ASSERT_EQ_UINT32(1, idx);

    // val=3000, cnt=48, exp=10 -> idx=2
    INDEX_BY_VALUE64(3000ull, 48, 10, idx);
    FL_ASSERT_EQ_UINT32(2, idx);

    // val=65536, cnt=48, exp=10 -> idx=12
    INDEX_BY_VALUE64(65536ull, 48, 10, idx);
    FL_ASSERT_EQ_UINT32(12, idx);
}

FL_TEST("INDEX_BY_VALUE64 edge cases", index_by_value64_edge_cases) {
    u32 idx;

    // Below threshold: val=0 -> idx=0
    INDEX_BY_VALUE64(0ull, 48, 10, idx);
    FL_ASSERT_EQ_UINT32(0, idx);

    // Below threshold: val=512 (less than 1<<10=1024) -> idx=0
    INDEX_BY_VALUE64(512ull, 48, 10, idx);
    FL_ASSERT_EQ_UINT32(0, idx);

    // Overflow clamp: very large value -> idx=CNT-1=47
    INDEX_BY_VALUE64(0xFFFFFFFFFFFFFFFFull, 48, 10, idx);
    FL_ASSERT_EQ_UINT32(47, idx);
}

// ===========================================================================
// index_by_value inline function
// ===========================================================================

FL_TEST("index_by_value matches INDEX_BY_VALUE64", index_by_value_matches_macro) {
    u32    cnt         = 48;
    u32    exp         = 10;
    flag64 test_vals[] = {
        0,
        512,
        1024,
        1536,
        2048,
        3000,
        4096,
        8192,
        65536,
        1024ull * 1024,
        1024ull * 1024 * 1024,
        0xFFFFFFFFFFFFFFFFull,
    };

    for (u32 i = 0; i < FL_ARRAY_COUNT(test_vals); i++) {
        u32 macro_idx;
        INDEX_BY_VALUE64(test_vals[i], cnt, exp, macro_idx);
        u32 func_idx = index_by_value(test_vals[i], cnt, exp);
        FL_ASSERT_EQ_UINT32(macro_idx, func_idx);
    }
}

// ===========================================================================
// INDEX_BY_VALUE32 macro
// ===========================================================================

FL_TEST("INDEX_BY_VALUE32 basic", index_by_value32_basic) {
    u32 idx;

    // Below threshold: val=0 -> idx=0
    INDEX_BY_VALUE32(0, 32, 8, idx);
    FL_ASSERT_EQ_UINT32(0, idx);

    // Below threshold: val=128 (less than 1<<8=256) -> idx=0
    INDEX_BY_VALUE32(128, 32, 8, idx);
    FL_ASSERT_EQ_UINT32(0, idx);

    // At threshold: val=256 (1<<8) -> idx=0
    INDEX_BY_VALUE32(256, 32, 8, idx);
    FL_ASSERT_EQ_UINT32(0, idx);

    // val=384 -> idx=1
    INDEX_BY_VALUE32(384, 32, 8, idx);
    FL_ASSERT_EQ_UINT32(1, idx);

    // val=512 -> idx=2
    INDEX_BY_VALUE32(512, 32, 8, idx);
    FL_ASSERT_EQ_UINT32(2, idx);

    // val=768 -> idx=3
    INDEX_BY_VALUE32(768, 32, 8, idx);
    FL_ASSERT_EQ_UINT32(3, idx);
}

FL_TEST("INDEX_BY_VALUE32 overflow clamp", index_by_value32_overflow) {
    u32 idx;

    // With p2=0, val=0x80000000: X > MAX_X -> idx=CNT-1=31
    INDEX_BY_VALUE32(0x80000000ul, 32, 0, idx);
    FL_ASSERT_EQ_UINT32(31, idx);
}

// ===========================================================================
// INDEX_BIN_TO_LOWER_LIMIT / INDEX_BIN_TO_UPPER_LIMIT
// ===========================================================================

FL_TEST("INDEX_BIN_TO_LOWER_LIMIT known values", bin_lower_limit_known_values) {
    // With P2=10, first bin starts at 1024 (1<<10)
    FL_ASSERT_EQ_SIZE_T((size_t)1024, (size_t)INDEX_BIN_TO_LOWER_LIMIT(0, 10));
    FL_ASSERT_EQ_SIZE_T((size_t)1536, (size_t)INDEX_BIN_TO_LOWER_LIMIT(1, 10));
    FL_ASSERT_EQ_SIZE_T((size_t)2048, (size_t)INDEX_BIN_TO_LOWER_LIMIT(2, 10));
    FL_ASSERT_EQ_SIZE_T((size_t)3072, (size_t)INDEX_BIN_TO_LOWER_LIMIT(3, 10));
    FL_ASSERT_EQ_SIZE_T((size_t)4096, (size_t)INDEX_BIN_TO_LOWER_LIMIT(4, 10));
    FL_ASSERT_EQ_SIZE_T((size_t)6144, (size_t)INDEX_BIN_TO_LOWER_LIMIT(5, 10));
}

FL_TEST("INDEX_BIN_TO_UPPER_LIMIT known values", bin_upper_limit_known_values) {
    // Upper = Lower + half-range - alignment
    FL_ASSERT_EQ_SIZE_T(1024 + 512 - INDEX_BIN_ALIGNMENT,
                        (size_t)INDEX_BIN_TO_UPPER_LIMIT(0, 10));
    FL_ASSERT_EQ_SIZE_T(1536 + 512 - INDEX_BIN_ALIGNMENT,
                        (size_t)INDEX_BIN_TO_UPPER_LIMIT(1, 10));
    FL_ASSERT_EQ_SIZE_T(2048 + 1024 - INDEX_BIN_ALIGNMENT,
                        (size_t)INDEX_BIN_TO_UPPER_LIMIT(2, 10));
    FL_ASSERT_EQ_SIZE_T(3072 + 1024 - INDEX_BIN_ALIGNMENT,
                        (size_t)INDEX_BIN_TO_UPPER_LIMIT(3, 10));
}

FL_TEST("Bin limits structural properties", bin_limits_properties) {
    u32 p2 = 10;

    for (u32 idx = 0; idx < 20; idx++) {
        size_t lower = INDEX_BIN_TO_LOWER_LIMIT(idx, p2);
        size_t upper = INDEX_BIN_TO_UPPER_LIMIT(idx, p2);

        // Upper limit must be at least as large as lower limit
        FL_ASSERT_TRUE(upper >= lower);

        // Contiguity: next bin's lower limit = this bin's upper limit + alignment
        if (idx < 19) {
            size_t next_lower = INDEX_BIN_TO_LOWER_LIMIT(idx + 1, p2);
            FL_ASSERT_EQ_SIZE_T(next_lower, upper + INDEX_BIN_ALIGNMENT);
        }
    }
}

FL_TEST("Bin lower limit round-trips through index_by_value", bin_limit_round_trip) {
    u32 cnt = 48;
    u32 p2  = 10;

    // For each bin, its lower limit should map back to the same bin index.
    for (u32 idx = 0; idx < cnt; idx++) {
        size_t lower     = INDEX_BIN_TO_LOWER_LIMIT(idx, p2);
        u32    round_idx = index_by_value((flag64)lower, cnt, p2);
        FL_ASSERT_EQ_UINT32(idx, round_idx);
    }
}

// ===========================================================================
// Test suite registration
// ===========================================================================

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD(lsb2idx32_single_bit)
FL_SUITE_ADD(lsb2idx32_edge_cases)
FL_SUITE_ADD(lsb2idx64_single_bit)
FL_SUITE_ADD(lsb2idx64_edge_cases)
FL_SUITE_ADD(msb2idx32_single_bit)
FL_SUITE_ADD(msb2idx32_edge_cases)
FL_SUITE_ADD(msb2idx64_single_bit)
FL_SUITE_ADD(msb2idx64_edge_cases)
FL_SUITE_ADD(index_by_value64_examples)
FL_SUITE_ADD(index_by_value64_edge_cases)
FL_SUITE_ADD(index_by_value_matches_macro)
FL_SUITE_ADD(index_by_value32_basic)
FL_SUITE_ADD(index_by_value32_overflow)
FL_SUITE_ADD(bin_lower_limit_known_values)
FL_SUITE_ADD(bin_upper_limit_known_values)
FL_SUITE_ADD(bin_limits_properties)
FL_SUITE_ADD(bin_limit_round_trip)
FL_SUITE_END;

FL_GET_TEST_SUITE("Index (Intel)", ts)
