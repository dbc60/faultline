/**
 * @file bits_tests.c
 * @author Douglas Cuthbertson
 * @brief The test suite to test bit macros.
 * @version 0.2
 * @date 2024-11-09
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception_service.c"
#include "fla_exception_service.c"

#include "bits.h"                           // macros under test
#include <faultline/fl_abbreviated_types.h> // flag64, u64, U64_BIT
#include <faultline/fl_macros.h>            // FL_UNUSED
#include <faultline/fl_test.h>              // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE

#include <stddef.h> // NULL

/// Assert two flag64 values are equal, displaying hex on failure.
#define ASSERT_EQ_HEX64(EXPECTED, ACTUAL) \
    FL_ASSERT_BINOP_TYPED((flag64)(EXPECTED), (flag64)(ACTUAL), ==, "==", "0x%016llX")

// ---------------------------------------------------------------------------
// ALIGN_UP
// ---------------------------------------------------------------------------

FL_TEST("ALIGN_UP", test_align_up) {
    // Zero stays zero
    ASSERT_EQ_HEX64(0, ALIGN_UP(0, 16));

    // Already aligned values stay the same
    ASSERT_EQ_HEX64(48, ALIGN_UP(48, 16));
    ASSERT_EQ_HEX64(4096, ALIGN_UP(4096, 4096));

    // Rounds up to next multiple
    ASSERT_EQ_HEX64(48, ALIGN_UP(47, 16));
    ASSERT_EQ_HEX64(64, ALIGN_UP(49, 16));
    ASSERT_EQ_HEX64(4096, ALIGN_UP(4001, 4096));

    // Alignment of 1 is identity
    ASSERT_EQ_HEX64(7, ALIGN_UP(7, 1));
    ASSERT_EQ_HEX64(0, ALIGN_UP(0, 1));

    // Various powers of 2
    ASSERT_EQ_HEX64(2, ALIGN_UP(1, 2));
    ASSERT_EQ_HEX64(2, ALIGN_UP(2, 2));
    ASSERT_EQ_HEX64(8, ALIGN_UP(5, 4));
    ASSERT_EQ_HEX64(8, ALIGN_UP(5, 8));
    ASSERT_EQ_HEX64(64, ALIGN_UP(33, 64));
}

// ---------------------------------------------------------------------------
// ALIGN_DOWN
// ---------------------------------------------------------------------------

FL_TEST("ALIGN_DOWN", test_align_down) {
    // Zero stays zero
    ASSERT_EQ_HEX64(0, ALIGN_DOWN(0, 16));

    // Already aligned values stay the same
    ASSERT_EQ_HEX64(48, ALIGN_DOWN(48, 16));
    ASSERT_EQ_HEX64(4096, ALIGN_DOWN(4096, 4096));

    // Rounds down to previous multiple
    ASSERT_EQ_HEX64(48, ALIGN_DOWN(49, 16));
    ASSERT_EQ_HEX64(32, ALIGN_DOWN(47, 16));
    ASSERT_EQ_HEX64(0, ALIGN_DOWN(4095, 4096));

    // Value smaller than alignment rounds to zero
    ASSERT_EQ_HEX64(0, ALIGN_DOWN(3, 16));
    ASSERT_EQ_HEX64(0, ALIGN_DOWN(15, 16));

    // Alignment of 1 is identity
    ASSERT_EQ_HEX64(7, ALIGN_DOWN(7, 1));

    // Various powers of 2
    ASSERT_EQ_HEX64(0, ALIGN_DOWN(1, 2));
    ASSERT_EQ_HEX64(2, ALIGN_DOWN(2, 2));
    ASSERT_EQ_HEX64(4, ALIGN_DOWN(5, 4));
    ASSERT_EQ_HEX64(0, ALIGN_DOWN(5, 8));
    ASSERT_EQ_HEX64(0, ALIGN_DOWN(33, 64));
    ASSERT_EQ_HEX64(64, ALIGN_DOWN(65, 64));
}

// ---------------------------------------------------------------------------
// UNSIGNED_NEGATION
// ---------------------------------------------------------------------------

FL_TEST("UNSIGNED_NEGATION", test_unsigned_negation) {
    // Negation of zero is zero
    flag64 zero = 0;
    ASSERT_EQ_HEX64(0, UNSIGNED_NEGATION(zero));

    // Negation of 1 is UINT64_MAX
    flag64 one = 1;
    ASSERT_EQ_HEX64(U64_MASK, UNSIGNED_NEGATION(one));

    // x + UNSIGNED_NEGATION(x) == 0 for various values
    flag64 values[] = {1, 2, 0xFF, 0x1234, 0xDEADBEEF, 0xAAAAAAAAAAAAAAAA, U64_MASK};

    for (int i = 0; i < (int)(sizeof values / sizeof values[0]); i++) {
        flag64 x   = values[i];
        flag64 sum = x + UNSIGNED_NEGATION(x);

        if (sum != 0) {
            FL_FAIL("x + UNSIGNED_NEGATION(x) != 0 for x=0x%016llX, got 0x%016llX", x,
                    sum);
        }
    }
}

// ---------------------------------------------------------------------------
// BIT_LSB
// ---------------------------------------------------------------------------

FL_TEST("BIT_LSB", test_bit_lsb) {
    // Zero returns zero
    flag64 zero = 0;
    ASSERT_EQ_HEX64(0, BIT_LSB(zero));

    // All bits set: LSB is bit 0
    flag64 all = U64_MASK;
    ASSERT_EQ_HEX64(1, BIT_LSB(all));

    // Single bit set returns itself
    for (int k = 0; k < (int)U64_BIT; k++) {
        flag64 x      = (flag64)1 << k;
        flag64 result = BIT_LSB(x);

        if (result != x) {
            FL_FAIL("BIT_LSB(1<<%d) = 0x%016llX, expected 0x%016llX", k, result, x);
        }
    }

    // Multiple bits: returns only the lowest set bit
    ASSERT_EQ_HEX64(0x08, BIT_LSB((flag64)0x28));     // 0b00101000 -> bit 3
    ASSERT_EQ_HEX64(0x10, BIT_LSB((flag64)0xF0));     // 0b11110000 -> bit 4
    ASSERT_EQ_HEX64(0x0100, BIT_LSB((flag64)0xFF00)); // 0xFF00 -> bit 8
    ASSERT_EQ_HEX64(0x80, BIT_LSB((flag64)0x80));     // single bit -> itself
}

// ---------------------------------------------------------------------------
// BIT_MASK_LEFT
// ---------------------------------------------------------------------------

FL_TEST("BIT_MASK_LEFT", test_bit_mask_left) {
    // Zero input gives zero (no bit set, so no bits to the left)
    flag64 zero = 0;
    ASSERT_EQ_HEX64(0, BIT_MASK_LEFT(zero));

    // LSB at bit 0: bits left of bit 0 = bits 1-63
    flag64 one = 1;
    ASSERT_EQ_HEX64(0xFFFFFFFFFFFFFFFE, BIT_MASK_LEFT(one));

    // LSB at bit 3 (X=0x28): bits left of bit 3 = bits 4-63
    flag64 x28 = 0x28;
    ASSERT_EQ_HEX64(0xFFFFFFFFFFFFFFF0, BIT_MASK_LEFT(x28));

    // LSB at bit 4 (X=0xF0): bits left of bit 4 = bits 5-63
    flag64 xf0 = 0xF0;
    ASSERT_EQ_HEX64(0xFFFFFFFFFFFFFFE0, BIT_MASK_LEFT(xf0));

    // LSB at bit 8 (X=0xFF00): bits left of bit 8 = bits 9-63
    flag64 xff00 = 0xFF00;
    ASSERT_EQ_HEX64(0xFFFFFFFFFFFFFE00, BIT_MASK_LEFT(xff00));

    // MSB only: no bits left of bit 63
    flag64 msb = 0x8000000000000000;
    ASSERT_EQ_HEX64(0, BIT_MASK_LEFT(msb));
}

// ---------------------------------------------------------------------------
// BIT_MASK_LEFT_LSB
// ---------------------------------------------------------------------------

FL_TEST("BIT_MASK_LEFT_LSB", test_bit_mask_left_lsb) {
    // Zero input gives zero
    flag64 zero = 0;
    ASSERT_EQ_HEX64(0, BIT_MASK_LEFT_LSB(zero));

    // LSB at bit 0: bit 0 and all bits to its left = all bits
    flag64 one = 1;
    ASSERT_EQ_HEX64(U64_MASK, BIT_MASK_LEFT_LSB(one));

    // LSB at bit 3 (X=0x28): bits 3-63
    flag64 x28 = 0x28;
    ASSERT_EQ_HEX64(0xFFFFFFFFFFFFFFF8, BIT_MASK_LEFT_LSB(x28));

    // MSB only: just bit 63
    flag64 msb = 0x8000000000000000;
    ASSERT_EQ_HEX64(0x8000000000000000, BIT_MASK_LEFT_LSB(msb));

    // Verify relationship: BIT_MASK_LEFT_LSB(X) == BIT_MASK_LEFT(X) | BIT_LSB(X)
    flag64 test_vals[] = {1, 2, 0x28, 0xFF00, 0x8000000000000000, U64_MASK};

    for (int i = 0; i < (int)(sizeof test_vals / sizeof test_vals[0]); i++) {
        flag64 x    = test_vals[i];
        flag64 mlsb = BIT_MASK_LEFT_LSB(x);
        flag64 ml   = BIT_MASK_LEFT(x);
        flag64 lsb  = BIT_LSB(x);

        if (mlsb != (ml | lsb)) {
            FL_FAIL("BIT_MASK_LEFT_LSB(0x%016llX)=0x%016llX != "
                    "BIT_MASK_LEFT|BIT_LSB=0x%016llX",
                    x, mlsb, ml | lsb);
        }
    }
}

// ---------------------------------------------------------------------------
// BITS_LEFT_LSB - single bit extraction (adapted from original test)
// ---------------------------------------------------------------------------

FL_TEST("BITS_LEFT_LSB 1-bit", test_left_bits) {
    flag64 const fa = 0xAAAAAAAAAAAAAAAA;
    flag64 const f5 = 0x5555555555555555;

    // 0xAAAA... has bits set at odd positions (1,3,5,...,63)
    for (int k = 0; k < (int)U64_BIT; k++) {
        flag64 actual = BITS_LEFT_LSB(fa, k, 1);

        if (k % 2 == 0) {
            if (actual != 0) {
                FL_FAIL("BITS_LEFT_LSB(0xAA.., %d, 1) = 0x%016llX, expected 0", k,
                        actual);
            }
        } else {
            if (actual != 1) {
                FL_FAIL("BITS_LEFT_LSB(0xAA.., %d, 1) = 0x%016llX, expected 1", k,
                        actual);
            }
        }
    }

    // 0x5555... has bits set at even positions (0,2,4,...,62)
    for (int k = 0; k < (int)U64_BIT; k++) {
        flag64 actual = BITS_LEFT_LSB(f5, k, 1);

        if (k % 2 == 1) {
            if (actual != 0) {
                FL_FAIL("BITS_LEFT_LSB(0x55.., %d, 1) = 0x%016llX, expected 0", k,
                        actual);
            }
        } else {
            if (actual != 1) {
                FL_FAIL("BITS_LEFT_LSB(0x55.., %d, 1) = 0x%016llX, expected 1", k,
                        actual);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// BITS_LEFT_LSB - multi-bit extraction
// ---------------------------------------------------------------------------

FL_TEST("BITS_LEFT_LSB multi-bit", test_left_bits_multi) {
    flag64 const x = 0xABCD1234DEADBEEF;

    // Extract individual bytes at known offsets
    ASSERT_EQ_HEX64(0xEF, BITS_LEFT_LSB(x, 0, 8));
    ASSERT_EQ_HEX64(0xBE, BITS_LEFT_LSB(x, 8, 8));
    ASSERT_EQ_HEX64(0xAD, BITS_LEFT_LSB(x, 16, 8));
    ASSERT_EQ_HEX64(0xDE, BITS_LEFT_LSB(x, 24, 8));
    ASSERT_EQ_HEX64(0x34, BITS_LEFT_LSB(x, 32, 8));
    ASSERT_EQ_HEX64(0x12, BITS_LEFT_LSB(x, 40, 8));
    ASSERT_EQ_HEX64(0xCD, BITS_LEFT_LSB(x, 48, 8));
    ASSERT_EQ_HEX64(0xAB, BITS_LEFT_LSB(x, 56, 8));

    // Extract 16-bit fields
    ASSERT_EQ_HEX64(0xBEEF, BITS_LEFT_LSB(x, 0, 16));
    ASSERT_EQ_HEX64(0xDEAD, BITS_LEFT_LSB(x, 16, 16));
    ASSERT_EQ_HEX64(0x1234, BITS_LEFT_LSB(x, 32, 16));
    ASSERT_EQ_HEX64(0xABCD, BITS_LEFT_LSB(x, 48, 16));

    // Extract 4-bit nibbles
    ASSERT_EQ_HEX64(0xF, BITS_LEFT_LSB(x, 0, 4));
    ASSERT_EQ_HEX64(0xE, BITS_LEFT_LSB(x, 4, 4));
    ASSERT_EQ_HEX64(0xE, BITS_LEFT_LSB(x, 8, 4));
}

// ---------------------------------------------------------------------------
// BITS_RIGHT_MSB - single bit extraction (adapted from original test)
// ---------------------------------------------------------------------------

FL_TEST("BITS_RIGHT_MSB 1-bit", test_right_msb) {
    flag64 const fa       = 0xAAAAAAAAAAAAAAAA;
    flag64 const f5       = 0x5555555555555555;
    flag64 const expected = 0x8000000000000000;

    // 0xAAAA...: bit 63 is set, bit 62 is clear, etc.
    // Shifting left by even k keeps MSB pattern; odd k inverts it.
    for (int k = 0; k < (int)U64_BIT; k++) {
        flag64 actual = BITS_RIGHT_MSB(fa, k, 1);

        if (k % 2 == 0) {
            if (actual != expected) {
                FL_FAIL("BITS_RIGHT_MSB(0xAA.., %d, 1) = 0x%016llX, expected 0x%016llX",
                        k, actual, expected);
            }
        } else {
            if (actual != 0) {
                FL_FAIL("BITS_RIGHT_MSB(0xAA.., %d, 1) = 0x%016llX, expected 0", k,
                        actual);
            }
        }
    }

    // 0x5555...: bit 63 is clear, bit 62 is set, etc.
    for (int k = 0; k < (int)U64_BIT; k++) {
        flag64 actual = BITS_RIGHT_MSB(f5, k, 1);

        if (k % 2 == 0) {
            if (actual != 0) {
                FL_FAIL("BITS_RIGHT_MSB(0x55.., %d, 1) = 0x%016llX, expected 0", k,
                        actual);
            }
        } else {
            if (actual != expected) {
                FL_FAIL("BITS_RIGHT_MSB(0x55.., %d, 1) = 0x%016llX, expected 0x%016llX",
                        k, actual, expected);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// BITS_RIGHT_MSB - multi-bit extraction
// ---------------------------------------------------------------------------

FL_TEST("BITS_RIGHT_MSB multi-bit", test_right_msb_multi) {
    flag64 const x = 0xABCD1234DEADBEEF;

    // Extract top 8 bits (shift 0, J=8) -> 0xAB in MSB position
    ASSERT_EQ_HEX64(0xAB00000000000000, BITS_RIGHT_MSB(x, 0, 8));

    // Shift left 8, extract top 8 -> 0xCD in MSB position
    ASSERT_EQ_HEX64(0xCD00000000000000, BITS_RIGHT_MSB(x, 8, 8));

    // Extract top 16 bits (shift 0, J=16) -> 0xABCD in MSB position
    ASSERT_EQ_HEX64(0xABCD000000000000, BITS_RIGHT_MSB(x, 0, 16));

    // Shift left 16, extract top 16 -> 0x1234 in MSB position
    ASSERT_EQ_HEX64(0x1234000000000000, BITS_RIGHT_MSB(x, 16, 16));

    // Extract top 4 bits -> 0xA in MSB position
    ASSERT_EQ_HEX64(0xA000000000000000, BITS_RIGHT_MSB(x, 0, 4));
}

// ---------------------------------------------------------------------------
// BITS_RIGHT_MSB_LEAST - single bit (adapted from original test)
// ---------------------------------------------------------------------------

FL_TEST("BITS_RIGHT_MSB_LEAST 1-bit", test_right_msb_least) {
    flag64 const fa = 0xAAAAAAAAAAAAAAAA;
    flag64 const f5 = 0x5555555555555555;

    // Same pattern as BITS_RIGHT_MSB but result is shifted down to LSB
    for (int k = 0; k < (int)U64_BIT; k++) {
        flag64 actual = BITS_RIGHT_MSB_LEAST(fa, k, 1);

        if (k % 2 == 0) {
            if (actual != 1) {
                FL_FAIL("BITS_RIGHT_MSB_LEAST(0xAA.., %d, 1) = 0x%016llX, expected 1", k,
                        actual);
            }
        } else {
            if (actual != 0) {
                FL_FAIL("BITS_RIGHT_MSB_LEAST(0xAA.., %d, 1) = 0x%016llX, expected 0", k,
                        actual);
            }
        }
    }

    for (int k = 0; k < (int)U64_BIT; k++) {
        flag64 actual = BITS_RIGHT_MSB_LEAST(f5, k, 1);

        if (k % 2 == 0) {
            if (actual != 0) {
                FL_FAIL("BITS_RIGHT_MSB_LEAST(0x55.., %d, 1) = 0x%016llX, expected 0", k,
                        actual);
            }
        } else {
            if (actual != 1) {
                FL_FAIL("BITS_RIGHT_MSB_LEAST(0x55.., %d, 1) = 0x%016llX, expected 1", k,
                        actual);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// BITS_RIGHT_MSB_LEAST - multi-bit with known expected values
// ---------------------------------------------------------------------------

FL_TEST("BITS_RIGHT_MSB_LEAST known values", test_right_msb_least_known) {
    flag64 const x = 0xABCD1234DEADBEEF;

    // Extract bytes from the MSB end, shifted down to LSBs
    ASSERT_EQ_HEX64(0xAB, BITS_RIGHT_MSB_LEAST(x, 0, 8));
    ASSERT_EQ_HEX64(0xCD, BITS_RIGHT_MSB_LEAST(x, 8, 8));
    ASSERT_EQ_HEX64(0x12, BITS_RIGHT_MSB_LEAST(x, 16, 8));
    ASSERT_EQ_HEX64(0x34, BITS_RIGHT_MSB_LEAST(x, 24, 8));

    // Extract 16-bit fields
    ASSERT_EQ_HEX64(0xABCD, BITS_RIGHT_MSB_LEAST(x, 0, 16));
    ASSERT_EQ_HEX64(0x1234, BITS_RIGHT_MSB_LEAST(x, 16, 16));

    // Extract nibbles
    ASSERT_EQ_HEX64(0xA, BITS_RIGHT_MSB_LEAST(x, 0, 4));
    ASSERT_EQ_HEX64(0xB, BITS_RIGHT_MSB_LEAST(x, 4, 4));
    ASSERT_EQ_HEX64(0xC, BITS_RIGHT_MSB_LEAST(x, 8, 4));
    ASSERT_EQ_HEX64(0xD, BITS_RIGHT_MSB_LEAST(x, 12, 4));

    // Single MSB bit checks
    flag64 const msb_set = 0xC000000000000000;
    ASSERT_EQ_HEX64(3, BITS_RIGHT_MSB_LEAST(msb_set, 0, 2));
    ASSERT_EQ_HEX64(1, BITS_RIGHT_MSB_LEAST(msb_set, 0, 1));

    flag64 const bit62 = 0x4000000000000000;
    ASSERT_EQ_HEX64(1, BITS_RIGHT_MSB_LEAST(bit62, 1, 1));

    // Full 32-bit extraction
    ASSERT_EQ_HEX64(0xABCD1234, BITS_RIGHT_MSB_LEAST(x, 0, 32));
}

// ---------------------------------------------------------------------------
// Test suite registration
// ---------------------------------------------------------------------------

FL_SUITE_BEGIN(bits)
FL_SUITE_ADD(test_align_up)
FL_SUITE_ADD(test_align_down)
FL_SUITE_ADD(test_unsigned_negation)
FL_SUITE_ADD(test_bit_lsb)
FL_SUITE_ADD(test_bit_mask_left)
FL_SUITE_ADD(test_bit_mask_left_lsb)
FL_SUITE_ADD(test_left_bits)
FL_SUITE_ADD(test_left_bits_multi)
FL_SUITE_ADD(test_right_msb)
FL_SUITE_ADD(test_right_msb_multi)
FL_SUITE_ADD(test_right_msb_least)
FL_SUITE_ADD(test_right_msb_least_known)
FL_SUITE_END;

FL_GET_TEST_SUITE("Bits", bits)
