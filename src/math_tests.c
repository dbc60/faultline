/**
 * @file math_tests.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2024-11-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "math.c"                  // unity build
#include <faultline/fl_macros.h>             // FL_SPEC_EXPORT
#include <faultline/fl_test.h>               // FLTestCase
#include "fl_exception_service.c"  // fl_expected_failure
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include <faultline/fl_math.h>               // function prototypes
#include <faultline/fl_abbreviated_types.h>  // u16, u32, u64

#include <stddef.h> // size_t, NULL

FL_TEST("Sum Over Scaled Range", sum_over_scaled_range) {
    u32    lower = 1024;
    u32    upper = 1520;
    u32    scale = 16;
    size_t actual;
    size_t expected = 0;

    // test even
    SUM_OVER_SCALED_RANGE(lower, upper, scale, actual);
    for (u32 add = lower; add <= upper; add += scale) {
        expected += add;
    }

    FL_ASSERT_DETAILS(actual == expected, "Expected %zu. Actual %zu", expected, actual);

    // test odd
    upper += 16;
    expected = 0;
    SUM_OVER_SCALED_RANGE(lower, upper, scale, actual);
    for (u32 add = lower; add <= upper; add += scale) {
        expected += add;
    }

    FL_ASSERT_DETAILS(actual == expected, "Expected %zu. Actual %zu", expected, actual);

    // single-element range: lower == upper, result must equal that value
    lower    = 500;
    upper    = 500;
    expected = 500;
    SUM_OVER_SCALED_RANGE(lower, upper, scale, actual);
    FL_ASSERT_DETAILS(actual == expected, "Expected %zu. Actual %zu", expected, actual);
}

FL_TEST("Count Leading Zeros 16-bit", count_leading_zeros16) {
    u16 val;
    u16 count;
    // __pragma(warning(push)) // for when /analyze is on
    // __pragma(warning(disable:6292)) // for when /analyze is on

    // This loop relies on val wrapping around to zero.
    for (val = 1; val != 0; val++) {
        count            = math_count_leading_zeros16(val);
        u16 expected_clz = (u16)(15 - math_log2_bit16(val));
        FL_ASSERT_EQ_UINT16(count, expected_clz);
    }
    // __pragma(warning(pop)) // for when /analyze is on

    FL_ASSERT_ZERO_UINT16(val);

    count = math_count_leading_zeros16(val);
    FL_ASSERT_EQ_UINT16(count, (u16)16);
}

FL_TEST("Count Leading Zeros 32-bit", count_leading_zeros32) {
    u32 val;
    u16 count;
    u16 expected;

    // This loop relies on val wrapping around to zero.
    for (val = 1, expected = 31; val > 0; val <<= 1, expected--) {
        count = math_count_leading_zeros32(val);
        FL_ASSERT_EQ_UINT16(count, expected);
    }

    FL_ASSERT_ZERO_UINT32(val);

    count = math_count_leading_zeros32(val);
    FL_ASSERT_EQ_UINT16(count, (u16)32);

    // multi-bit values: CLZ is determined by the position of the highest set bit
    FL_ASSERT_EQ_UINT16(math_count_leading_zeros32(3U), 30); // 0b11, highest bit = 1
    FL_ASSERT_EQ_UINT16(math_count_leading_zeros32(0x0000FFFFU), 16); // highest bit = 15
    FL_ASSERT_EQ_UINT16(math_count_leading_zeros32(0xFFFFFFFFU), 0);  // highest bit = 31
}

FL_TEST("Count Leading Zeros 64-bit", count_leading_zeros64) {
    u64 val;
    u16 count;
    u16 expected;

    // This loop relies on val wrapping around to zero.
    for (val = 1, expected = 63; val > 0; val <<= 1, expected--) {
        count = math_count_leading_zeros64(val);
        FL_ASSERT_EQ_UINT16(count, expected);
    }

    FL_ASSERT_ZERO_UINT64(val);

    count = math_count_leading_zeros64(val);
    FL_ASSERT_EQ_UINT16(count, (u16)64);

    // multi-bit values: CLZ is determined by the position of the highest set bit
    FL_ASSERT_EQ_UINT16(math_count_leading_zeros64(3ULL), 62); // 0b11, highest bit = 1
    FL_ASSERT_EQ_UINT16(math_count_leading_zeros64(0x00000000FFFFFFFFULL),
                        32); // highest bit = 31
    FL_ASSERT_EQ_UINT16(math_count_leading_zeros64(0xFFFFFFFFFFFFFFFFULL),
                        0); // highest bit = 63
}

FL_TEST("Log2 of Greatest Power of 2 No More Than Value", greatest_log2) {
    flag64 expected = 10;
    flag64 actual;

    MATH_GREATEST_LOG2_BIT32(1024, actual);
    FL_ASSERT_EQ_UINT64(actual, expected);

    MATH_GREATEST_LOG2_BIT32(1025, actual);
    FL_ASSERT_EQ_UINT64(actual, expected);

    MATH_GREATEST_LOG2_BIT32(2047, actual);
    FL_ASSERT_EQ_UINT64(actual, expected);

    MATH_GREATEST_LOG2_BIT64(1024, actual);
    FL_ASSERT_EQ_UINT64(actual, expected);

    MATH_GREATEST_LOG2_BIT64(1025, actual);
    FL_ASSERT_EQ_UINT64(actual, expected);

    MATH_GREATEST_LOG2_BIT64(2047, actual);
    FL_ASSERT_EQ_UINT64(actual, expected);

    // low boundary: smallest inputs
    MATH_GREATEST_LOG2_BIT32(1U, actual);
    FL_ASSERT_EQ_UINT64(actual, (flag64)0);

    MATH_GREATEST_LOG2_BIT32(2U, actual);
    FL_ASSERT_EQ_UINT64(actual, (flag64)1);

    MATH_GREATEST_LOG2_BIT32(3U, actual); // floor(log2(3)) == 1
    FL_ASSERT_EQ_UINT64(actual, (flag64)1);

    // high boundary: 32-bit
    MATH_GREATEST_LOG2_BIT32(0x80000000U, actual);
    FL_ASSERT_EQ_UINT64(actual, (flag64)31);

    MATH_GREATEST_LOG2_BIT32(0xFFFFFFFFU, actual);
    FL_ASSERT_EQ_UINT64(actual, (flag64)31);

    // low boundary: 64-bit
    MATH_GREATEST_LOG2_BIT64(1ULL, actual);
    FL_ASSERT_EQ_UINT64(actual, (flag64)0);

    // high boundary: 64-bit
    MATH_GREATEST_LOG2_BIT64(0x8000000000000000ULL, actual);
    FL_ASSERT_EQ_UINT64(actual, (flag64)63);

    MATH_GREATEST_LOG2_BIT64(0xFFFFFFFFFFFFFFFFULL, actual);
    FL_ASSERT_EQ_UINT64(actual, (flag64)63);
}

FL_TEST("Log2 16-bit", log2_bit16) {
    // Powers of 2: log2(2^n) == n exactly
    for (u16 n = 0; n < 16; n++) {
        u16 val    = (u16)(1u << n);
        u16 actual = math_log2_bit16(val);
        FL_ASSERT_DETAILS(actual == n, "n=%u: expected %u, got %u", (unsigned)n,
                          (unsigned)n, (unsigned)actual);
    }

    // Non-powers-of-2: floor(log2(val))
    FL_ASSERT_EQ_UINT16(math_log2_bit16(3), 1);       // highest bit = 1
    FL_ASSERT_EQ_UINT16(math_log2_bit16(1023), 9);    // highest bit = 9
    FL_ASSERT_EQ_UINT16(math_log2_bit16(0xFFFF), 15); // all bits set, highest bit = 15
}

FL_TEST("Log2 32-bit", log2_bit32) {
    // Powers of 2: log2(2^n) == n exactly
    for (u16 n = 0; n < 32; n++) {
        u32 val    = 1u << n;
        u16 actual = math_log2_bit32(val);
        FL_ASSERT_DETAILS(actual == n, "n=%u: expected %u, got %u", (unsigned)n,
                          (unsigned)n, (unsigned)actual);
    }

    // Non-powers-of-2: floor(log2(val))
    FL_ASSERT_EQ_UINT16(math_log2_bit32(3U), 1);    // highest bit = 1
    FL_ASSERT_EQ_UINT16(math_log2_bit32(1023U), 9); // highest bit = 9
    FL_ASSERT_EQ_UINT16(math_log2_bit32(0xFFFFFFFFU),
                        31); // all bits set, highest bit = 31
}

FL_TEST("Log2 64-bit", log2_bit64) {
    // Powers of 2: log2(2^n) == n exactly
    for (u16 n = 0; n < 64; n++) {
        u64 val    = (u64)1 << n;
        u16 actual = math_log2_bit64(val);
        FL_ASSERT_DETAILS(actual == n, "n=%u: expected %u, got %u", (unsigned)n,
                          (unsigned)n, (unsigned)actual);
    }

    // Non-powers-of-2: floor(log2(val))
    FL_ASSERT_EQ_UINT16(math_log2_bit64(3ULL), 1);    // highest bit = 1
    FL_ASSERT_EQ_UINT16(math_log2_bit64(1023ULL), 9); // highest bit = 9
    FL_ASSERT_EQ_UINT16(math_log2_bit64(0xFFFFFFFFFFFFFFFFULL),
                        63); // all bits set, highest bit = 63
}

FL_TEST("Population Count 32-bit", pop_count32) {
    // Zero has no set bits
    FL_ASSERT_EQ_UINT16(math_pop_count32(0U), 0);

    // All bits set: 32 ones
    FL_ASSERT_EQ_UINT16(math_pop_count32(0xFFFFFFFFU), 32);

    // Each power of 2 has exactly one set bit
    for (u16 n = 0; n < 32; n++) {
        u32 val    = 1u << n;
        u16 actual = math_pop_count32(val);
        FL_ASSERT_DETAILS(actual == 1, "n=%u: expected 1, got %u", (unsigned)n,
                          (unsigned)actual);
    }

    // Alternating bit patterns
    FL_ASSERT_EQ_UINT16(math_pop_count32(0x55555555U),
                        16); // even-indexed bits set (16 ones)
    FL_ASSERT_EQ_UINT16(math_pop_count32(0xAAAAAAAAU),
                        16); // odd-indexed bits set (16 ones)
    FL_ASSERT_EQ_UINT16(math_pop_count32(0x0F0F0F0FU),
                        16); // alternating nibbles (16 ones)
    FL_ASSERT_EQ_UINT16(math_pop_count32(0x00FF00FFU),
                        16); // alternating byte pairs (16 ones)
}

FL_TEST("Population Count 64-bit", pop_count64) {
    // Zero has no set bits
    FL_ASSERT_EQ_UINT16(math_pop_count64(0ULL), 0);

    // All bits set: 64 ones
    FL_ASSERT_EQ_UINT16(math_pop_count64(0xFFFFFFFFFFFFFFFFULL), 64);

    // Each power of 2 has exactly one set bit
    for (u16 n = 0; n < 64; n++) {
        u64 val    = (u64)1 << n;
        u16 actual = math_pop_count64(val);
        FL_ASSERT_DETAILS(actual == 1, "n=%u: expected 1, got %u", (unsigned)n,
                          (unsigned)actual);
    }

    // Alternating bit patterns
    FL_ASSERT_EQ_UINT16(math_pop_count64(0x5555555555555555ULL),
                        32); // even-indexed bits set (32 ones)
    FL_ASSERT_EQ_UINT16(math_pop_count64(0xAAAAAAAAAAAAAAAAULL),
                        32); // odd-indexed bits set (32 ones)
    FL_ASSERT_EQ_UINT16(math_pop_count64(0x0F0F0F0F0F0F0F0FULL),
                        32); // alternating nibbles (32 ones)
    FL_ASSERT_EQ_UINT16(math_pop_count64(0x00FF00FF00FF00FFULL),
                        32); // alternating byte pairs (32 ones)
}

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD(sum_over_scaled_range)
FL_SUITE_ADD(count_leading_zeros16)
FL_SUITE_ADD(count_leading_zeros32)
FL_SUITE_ADD(count_leading_zeros64)
FL_SUITE_ADD(greatest_log2)
FL_SUITE_ADD(log2_bit16)
FL_SUITE_ADD(log2_bit32)
FL_SUITE_ADD(log2_bit64)
FL_SUITE_ADD(pop_count32)
FL_SUITE_ADD(pop_count64)
FL_SUITE_END;

FL_GET_TEST_SUITE("Math", ts)
