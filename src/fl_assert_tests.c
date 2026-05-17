/**
 * @file fl_assert_tests.c
 * @author Douglas Cuthbertson
 * @brief Unit tests for all FL_ASSERT* macros in fl_exception_service_assert.h.
 *
 * Goals:
 *  1. Every macro is instantiated with the matching C type, so the compiler
 *     (-Wformat) will flag any mismatch between the type and its format specifier.
 *  2. Every macro's pass path (no throw) and fail path (throws) are both exercised.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include "fl_exception_service.c"  // fl_expected_failure, fl_invalid_value, etc.
#include "fla_exception_service.c" // fl_throw_assertion, g_fla_exception_service
#include <faultline/fl_test.h>     // FL_TEST, FL_SUITE_*, FL_GET_TEST_SUITE
#include <faultline/fl_try.h>      // FL_TRY, FL_CATCH, FL_CATCH_ALL, FL_END_TRY
#include <faultline/fl_exception_service_assert.h> // all FL_ASSERT* macros

#include <stdbool.h> // bool
#include <stddef.h>  // size_t, ptrdiff_t, NULL
#include <stdint.h> // int16_t, int32_t, int64_t, uint16_t, uint32_t, uint64_t, uintptr_t
#include <string.h> // strstr

// ============================================================
// FL_ASSERT and friends
// ============================================================

FL_TEST("FL_ASSERT passes on true expression", test_assert_pass) {
    FL_ASSERT(1 == 1);
    FL_ASSERT(1 != 0);
}

FL_TEST("FL_ASSERT throws on false expression", test_assert_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT(1 == 2);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_DETAILS passes on true expression", test_assert_details_pass) {
    FL_ASSERT_DETAILS(2 > 1, "expected greater: %d > %d", 2, 1);
}

FL_TEST("FL_ASSERT_DETAILS throws with formatted details on false",
        test_assert_details_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_DETAILS(1 == 2, "expected %d got %d", 1, 2);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_FILE_LINE passes on true expression", test_assert_file_line_pass) {
    FL_ASSERT_FILE_LINE(1 == 1, __FILE__, __LINE__);
}

FL_TEST("FL_ASSERT_FILE_LINE throws on false expression", test_assert_file_line_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_FILE_LINE(1 == 2, "test_file.c", 42);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_DETAILS_FILE_LINE passes on true expression",
        test_assert_details_file_line_pass) {
    FL_ASSERT_DETAILS_FILE_LINE(3 > 2, "value %d", __FILE__, __LINE__, 3);
}

FL_TEST("FL_ASSERT_DETAILS_FILE_LINE throws on false expression",
        test_assert_details_file_line_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_DETAILS_FILE_LINE(1 == 2, "val=%s", "test_file.c", 99, "bad");
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_FAIL (unconditional throw)
// ============================================================

FL_TEST("FL_FAIL always throws fl_unexpected_failure", test_fail_macro) {
    bool volatile caught = false;
    FL_TRY {
        FL_FAIL("unconditional failure %d", 0);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_TRUE / FL_ASSERT_FALSE
// ============================================================

FL_TEST("FL_ASSERT_TRUE passes for non-zero expressions", test_assert_true_pass) {
    FL_ASSERT_TRUE(1);
    FL_ASSERT_TRUE(42);
    FL_ASSERT_TRUE(1 == 1);
}

FL_TEST("FL_ASSERT_TRUE throws fl_unexpected_failure for zero", test_assert_true_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_TRUE(0);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_FALSE passes for zero expressions", test_assert_false_pass) {
    FL_ASSERT_FALSE(0);
    FL_ASSERT_FALSE(1 == 2);
}

FL_TEST("FL_ASSERT_FALSE throws fl_unexpected_failure for non-zero",
        test_assert_false_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_FALSE(1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_ZERO_* - pass: value is zero; fail: value is non-zero
// Each variant is tested with the matching C type so -Wformat catches mismatches.
// ============================================================

FL_TEST("FL_ASSERT_ZERO_* passes for zero values of all types", test_zero_pass) {
    FL_ASSERT_ZERO_CHAR((char)0);
    FL_ASSERT_ZERO_SCHAR((signed char)0);
    FL_ASSERT_ZERO_UCHAR((unsigned char)0);
    FL_ASSERT_ZERO_INT((int)0);
    FL_ASSERT_ZERO_SINT((signed int)0);
    FL_ASSERT_ZERO_INT8((int8_t)0);
    FL_ASSERT_ZERO_INT16((int16_t)0);
    FL_ASSERT_ZERO_INT32((int32_t)0);
    FL_ASSERT_ZERO_INT64((int64_t)0);
    FL_ASSERT_ZERO_SIZE_T((size_t)0);
    FL_ASSERT_ZERO_INTPTR((intptr_t)0);
    FL_ASSERT_ZERO_PTRDIFF((ptrdiff_t)0);
    FL_ASSERT_ZERO_UINT((unsigned int)0);
    FL_ASSERT_ZERO_UINT8((uint8_t)0);
    FL_ASSERT_ZERO_UINT16((uint16_t)0);
    FL_ASSERT_ZERO_UINT32((uint32_t)0);
    FL_ASSERT_ZERO_UINT64((uint64_t)0);
    FL_ASSERT_ZERO_UINTPTR((uintptr_t)0);
}

FL_TEST("FL_ASSERT_ZERO_* throws fl_invalid_value for non-zero values of all types",
        test_zero_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_INT((int)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_CHAR((char)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_SCHAR((signed char)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_SINT((signed int)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_INT8((int8_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_INT16((int16_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_INT32((int32_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_INT64((int64_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_SIZE_T((size_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_PTRDIFF((ptrdiff_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_UINT((unsigned int)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_UINT16((uint16_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_UINT32((uint32_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_UINT64((uint64_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_ZERO_UINTPTR((uintptr_t)1);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_NON_ZERO_* - pass: value is non-zero; fail: value is zero
// ============================================================

FL_TEST("FL_ASSERT_NON_ZERO_* passes for non-zero values of all types",
        test_non_zero_pass) {
    FL_ASSERT_NON_ZERO_CHAR((char)1);
    FL_ASSERT_NON_ZERO_SCHAR((signed char)1);
    FL_ASSERT_NON_ZERO_UCHAR((unsigned char)1);
    FL_ASSERT_NON_ZERO_INT((int)1);
    FL_ASSERT_NON_ZERO_SINT((signed int)1);
    FL_ASSERT_NON_ZERO_INT8((int8_t)1);
    FL_ASSERT_NON_ZERO_INT16((int16_t)1);
    FL_ASSERT_NON_ZERO_INT32((int32_t)1);
    FL_ASSERT_NON_ZERO_INT64((int64_t)1);
    FL_ASSERT_NON_ZERO_SIZE_T((size_t)1);
    FL_ASSERT_NON_ZERO_INTPTR((intptr_t)1);
    FL_ASSERT_NON_ZERO_PTRDIFF((ptrdiff_t)1);
    FL_ASSERT_NON_ZERO_UINT((unsigned int)1);
    FL_ASSERT_NON_ZERO_UINT8((uint8_t)1);
    FL_ASSERT_NON_ZERO_UINT16((uint16_t)1);
    FL_ASSERT_NON_ZERO_UINT32((uint32_t)1);
    FL_ASSERT_NON_ZERO_UINT64((uint64_t)1);
    FL_ASSERT_NON_ZERO_UINTPTR((uintptr_t)1);
}

FL_TEST("FL_ASSERT_NON_ZERO_* throws fl_invalid_value for zero values of all types",
        test_non_zero_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_INT((int)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_CHAR((char)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_INT8((int8_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_INT16((int16_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_INT32((int32_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_INT64((int64_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_SIZE_T((size_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_PTRDIFF((ptrdiff_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_UINT((unsigned int)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_UINT16((uint16_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_UINT32((uint32_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_UINT64((uint64_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NON_ZERO_UINTPTR((uintptr_t)0);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - CHAR
// Macros: EQ, NEQ, LT, LE, GT, GE using %d
// ============================================================

FL_TEST("FL_ASSERT_*_CHAR passes for correct int relationships", test_binop_char_pass) {
    FL_ASSERT_EQ_CHAR(1, 1);
    FL_ASSERT_NEQ_CHAR(1, 2);
    FL_ASSERT_LT_CHAR(1, 2);
    FL_ASSERT_LE_CHAR(1, 1);
    FL_ASSERT_LE_CHAR(1, 2);
    FL_ASSERT_GT_CHAR(2, 1);
    FL_ASSERT_GE_CHAR(1, 1);
    FL_ASSERT_GE_CHAR(2, 1);
}

FL_TEST("FL_ASSERT_*_CHAR throws fl_unexpected_failure for violated int relationships",
        test_binop_char_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_CHAR(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_CHAR(1, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_CHAR(2, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_CHAR(2, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_CHAR(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_CHAR(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - INT
// Macros: EQ, NEQ, LT, LE, GT, GE using %d
// ============================================================

FL_TEST("FL_ASSERT_*_INT passes for correct int relationships", test_binop_int_pass) {
    FL_ASSERT_EQ_INT(1, 1);
    FL_ASSERT_NEQ_INT(1, 2);
    FL_ASSERT_LT_INT(1, 2);
    FL_ASSERT_LE_INT(1, 1);
    FL_ASSERT_LE_INT(1, 2);
    FL_ASSERT_GT_INT(2, 1);
    FL_ASSERT_GE_INT(1, 1);
    FL_ASSERT_GE_INT(2, 1);
}

FL_TEST("FL_ASSERT_*_INT throws fl_unexpected_failure for violated int relationships",
        test_binop_int_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_INT(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_INT(1, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_INT(2, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_INT(2, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_INT(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_INT(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - SINT
// Macros: EQ, NEQ, LT, LE, GT, GE using %d
// ============================================================

FL_TEST("FL_ASSERT_*_SINT passes for correct signed int relationships",
        test_binop_sint_pass) {
    FL_ASSERT_EQ_SINT(1, 1);
    FL_ASSERT_NEQ_SINT(1, 2);
    FL_ASSERT_LT_SINT(1, 2);
    FL_ASSERT_LE_SINT(1, 1);
    FL_ASSERT_LE_SINT(1, 2);
    FL_ASSERT_GT_SINT(2, 1);
    FL_ASSERT_GE_SINT(1, 1);
    FL_ASSERT_GE_SINT(2, 1);
}

FL_TEST("FL_ASSERT_*_SINT throws fl_unexpected_failure for violated signed int "
        "relationships",
        test_binop_sint_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_SINT(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_SINT(1, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_SINT(2, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_SINT(2, 1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_SINT(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_SINT(1, 2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - INT8  (PRId8)
// ============================================================

FL_TEST("FL_ASSERT_*_INT8 passes for correct int16_t relationships",
        test_binop_int8_pass) {
    FL_ASSERT_EQ_INT8((int8_t)1, (int8_t)1);
    FL_ASSERT_NEQ_INT8((int8_t)1, (int8_t)2);
    FL_ASSERT_LT_INT8((int8_t)1, (int8_t)2);
    FL_ASSERT_LE_INT8((int8_t)1, (int8_t)1);
    FL_ASSERT_LE_INT8((int8_t)1, (int8_t)2);
    FL_ASSERT_GT_INT8((int8_t)2, (int8_t)1);
    FL_ASSERT_GE_INT8((int8_t)1, (int8_t)1);
    FL_ASSERT_GE_INT8((int8_t)2, (int8_t)1);
}

FL_TEST(
    "FL_ASSERT_*_INT8 throws fl_unexpected_failure for violated int8_t relationships",
    test_binop_int8_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_INT8((int8_t)1, (int8_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_INT8((int8_t)1, (int8_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_INT8((int8_t)2, (int8_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_INT8((int8_t)2, (int8_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_INT8((int8_t)1, (int8_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_INT8((int8_t)1, (int8_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - INT16  (PRId16)
// ============================================================

FL_TEST("FL_ASSERT_*_INT16 passes for correct int16_t relationships",
        test_binop_int16_pass) {
    FL_ASSERT_EQ_INT16((int16_t)1, (int16_t)1);
    FL_ASSERT_NEQ_INT16((int16_t)1, (int16_t)2);
    FL_ASSERT_LT_INT16((int16_t)1, (int16_t)2);
    FL_ASSERT_LE_INT16((int16_t)1, (int16_t)1);
    FL_ASSERT_LE_INT16((int16_t)1, (int16_t)2);
    FL_ASSERT_GT_INT16((int16_t)2, (int16_t)1);
    FL_ASSERT_GE_INT16((int16_t)1, (int16_t)1);
    FL_ASSERT_GE_INT16((int16_t)2, (int16_t)1);
}

FL_TEST(
    "FL_ASSERT_*_INT16 throws fl_unexpected_failure for violated int16_t relationships",
    test_binop_int16_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_INT16((int16_t)1, (int16_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_INT16((int16_t)1, (int16_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_INT16((int16_t)2, (int16_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_INT16((int16_t)2, (int16_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_INT16((int16_t)1, (int16_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_INT16((int16_t)1, (int16_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - INT32  (PRId32)
// ============================================================

FL_TEST("FL_ASSERT_*_INT32 passes for correct int32_t relationships",
        test_binop_int32_pass) {
    FL_ASSERT_EQ_INT32((int32_t)1, (int32_t)1);
    FL_ASSERT_NEQ_INT32((int32_t)1, (int32_t)2);
    FL_ASSERT_LT_INT32((int32_t)1, (int32_t)2);
    FL_ASSERT_LE_INT32((int32_t)1, (int32_t)1);
    FL_ASSERT_LE_INT32((int32_t)1, (int32_t)2);
    FL_ASSERT_GT_INT32((int32_t)2, (int32_t)1);
    FL_ASSERT_GE_INT32((int32_t)1, (int32_t)1);
    FL_ASSERT_GE_INT32((int32_t)2, (int32_t)1);
}

FL_TEST(
    "FL_ASSERT_*_INT32 throws fl_unexpected_failure for violated int32_t relationships",
    test_binop_int32_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_INT32((int32_t)1, (int32_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_INT32((int32_t)1, (int32_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_INT32((int32_t)2, (int32_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_INT32((int32_t)2, (int32_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_INT32((int32_t)1, (int32_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_INT32((int32_t)1, (int32_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - UINT
// Macros: EQ, NEQ, LT, LE, GT, GE using %u
// ============================================================

FL_TEST("FL_ASSERT_*_UINT passes for correct unsigned int relationships",
        test_binop_uint_pass) {
    FL_ASSERT_EQ_UINT(1u, 1u);
    FL_ASSERT_NEQ_UINT(1u, 2u);
    FL_ASSERT_LT_UINT(1u, 2u);
    FL_ASSERT_LE_UINT(1u, 1u);
    FL_ASSERT_LE_UINT(1u, 2u);
    FL_ASSERT_GT_UINT(2u, 1u);
    FL_ASSERT_GE_UINT(1u, 1u);
    FL_ASSERT_GE_UINT(2u, 1u);
}

FL_TEST("FL_ASSERT_*_UINT throws fl_unexpected_failure for violated uint relationships",
        test_binop_uint_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_UINT(1u, 2u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_UINT(1u, 1u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_UINT(2u, 1u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_UINT(2u, 1u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_UINT(1u, 2u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_UINT(1u, 2u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - UCHAR
// Macros: EQ, NEQ, LT, LE, GT, GE using %u
// ============================================================

FL_TEST("FL_ASSERT_*_UCHAR passes for correct unsigned char relationships",
        test_binop_uchar_pass) {
    FL_ASSERT_EQ_UCHAR(1u, 1u);
    FL_ASSERT_NEQ_UCHAR(1u, 2u);
    FL_ASSERT_LT_UCHAR(1u, 2u);
    FL_ASSERT_LE_UCHAR(1u, 1u);
    FL_ASSERT_LE_UCHAR(1u, 2u);
    FL_ASSERT_GT_UCHAR(2u, 1u);
    FL_ASSERT_GE_UCHAR(1u, 1u);
    FL_ASSERT_GE_UCHAR(2u, 1u);
}

FL_TEST(
    "FL_ASSERT_*_UCHAR throws fl_unexpected_failure for violated uchar relationships",
    test_binop_uchar_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_UCHAR(1u, 2u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_UCHAR(1u, 1u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_UCHAR(2u, 1u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_UCHAR(2u, 1u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_UCHAR(1u, 2u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_UCHAR(1u, 2u);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - UINT8  (PRIu8)
// ============================================================

FL_TEST("FL_ASSERT_*_UINT8 passes for correct uint8_t relationships",
        test_binop_uint8_pass) {
    FL_ASSERT_EQ_UINT8((uint8_t)1, (uint8_t)1);
    FL_ASSERT_NEQ_UINT8((uint8_t)1, (uint8_t)2);
    FL_ASSERT_LT_UINT8((uint8_t)1, (uint8_t)2);
    FL_ASSERT_LE_UINT8((uint8_t)1, (uint8_t)1);
    FL_ASSERT_LE_UINT8((uint8_t)1, (uint8_t)2);
    FL_ASSERT_GT_UINT8((uint8_t)2, (uint8_t)1);
    FL_ASSERT_GE_UINT8((uint8_t)1, (uint8_t)1);
    FL_ASSERT_GE_UINT8((uint8_t)2, (uint8_t)1);
}

FL_TEST("FL_ASSERT_*_UINT8 throws fl_unexpected_failure for violated uint8_t "
        "relationships",
        test_binop_uint8_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_UINT8((uint8_t)1, (uint8_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_UINT8((uint8_t)1, (uint8_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_UINT8((uint8_t)2, (uint8_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_UINT8((uint8_t)2, (uint8_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_UINT8((uint8_t)1, (uint8_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_UINT8((uint8_t)1, (uint8_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - UINT16  (PRIu16)
// ============================================================

FL_TEST("FL_ASSERT_*_UINT16 passes for correct uint16_t relationships",
        test_binop_uint16_pass) {
    FL_ASSERT_EQ_UINT16((uint16_t)1, (uint16_t)1);
    FL_ASSERT_NEQ_UINT16((uint16_t)1, (uint16_t)2);
    FL_ASSERT_LT_UINT16((uint16_t)1, (uint16_t)2);
    FL_ASSERT_LE_UINT16((uint16_t)1, (uint16_t)1);
    FL_ASSERT_LE_UINT16((uint16_t)1, (uint16_t)2);
    FL_ASSERT_GT_UINT16((uint16_t)2, (uint16_t)1);
    FL_ASSERT_GE_UINT16((uint16_t)1, (uint16_t)1);
    FL_ASSERT_GE_UINT16((uint16_t)2, (uint16_t)1);
}

FL_TEST("FL_ASSERT_*_UINT16 throws fl_unexpected_failure for violated uint16_t "
        "relationships",
        test_binop_uint16_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_UINT16((uint16_t)1, (uint16_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_UINT16((uint16_t)1, (uint16_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_UINT16((uint16_t)2, (uint16_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_UINT16((uint16_t)2, (uint16_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_UINT16((uint16_t)1, (uint16_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_UINT16((uint16_t)1, (uint16_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - SIZE_T  (%zu)
// ============================================================

FL_TEST("FL_ASSERT_*_SIZE_T passes for correct size_t relationships",
        test_binop_size_t_pass) {
    FL_ASSERT_EQ_SIZE_T((size_t)1, (size_t)1);
    FL_ASSERT_NEQ_SIZE_T((size_t)1, (size_t)2);
    FL_ASSERT_LT_SIZE_T((size_t)1, (size_t)2);
    FL_ASSERT_LE_SIZE_T((size_t)1, (size_t)1);
    FL_ASSERT_GT_SIZE_T((size_t)2, (size_t)1);
    FL_ASSERT_GE_SIZE_T((size_t)1, (size_t)1);
}

FL_TEST(
    "FL_ASSERT_*_SIZE_T throws fl_unexpected_failure for violated size_t relationships",
    test_binop_size_t_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_SIZE_T((size_t)1, (size_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_SIZE_T((size_t)1, (size_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_SIZE_T((size_t)2, (size_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_SIZE_T((size_t)2, (size_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_SIZE_T((size_t)1, (size_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_SIZE_T((size_t)1, (size_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - PTRDIFF  (PRId64)
// ============================================================

FL_TEST("FL_ASSERT_*_PTRDIFF passes for correct ptrdiff_t relationships",
        test_binop_ptrdiff_pass) {
    FL_ASSERT_EQ_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)1);
    FL_ASSERT_NEQ_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)2);
    FL_ASSERT_LT_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)2);
    FL_ASSERT_LE_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)1);
    FL_ASSERT_GT_PTRDIFF((ptrdiff_t)2, (ptrdiff_t)1);
    FL_ASSERT_GE_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)1);
}

FL_TEST("FL_ASSERT_*_PTRDIFF throws fl_unexpected_failure for violated ptrdiff_t "
        "relationships",
        test_binop_ptrdiff_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_PTRDIFF((ptrdiff_t)2, (ptrdiff_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_PTRDIFF((ptrdiff_t)2, (ptrdiff_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_PTRDIFF((ptrdiff_t)1, (ptrdiff_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - UINT32  (PRIu32)
// ============================================================

FL_TEST("FL_ASSERT_*_UINT32 passes for correct uint32_t relationships",
        test_binop_uint32_pass) {
    FL_ASSERT_EQ_UINT32((uint32_t)1, (uint32_t)1);
    FL_ASSERT_NEQ_UINT32((uint32_t)1, (uint32_t)2);
    FL_ASSERT_LT_UINT32((uint32_t)1, (uint32_t)2);
    FL_ASSERT_LE_UINT32((uint32_t)1, (uint32_t)1);
    FL_ASSERT_GT_UINT32((uint32_t)2, (uint32_t)1);
    FL_ASSERT_GE_UINT32((uint32_t)1, (uint32_t)1);
}

FL_TEST("FL_ASSERT_*_UINT32 throws fl_unexpected_failure for violated uint32_t "
        "relationships",
        test_binop_uint32_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_UINT32((uint32_t)1, (uint32_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_UINT32((uint32_t)1, (uint32_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_UINT32((uint32_t)2, (uint32_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_UINT32((uint32_t)2, (uint32_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_UINT32((uint32_t)1, (uint32_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_UINT32((uint32_t)1, (uint32_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - UINT64  (PRIu64)
// ============================================================

FL_TEST("FL_ASSERT_*_UINT64 passes for correct uint64_t relationships",
        test_binop_uint64_pass) {
    FL_ASSERT_EQ_UINT64((uint64_t)1, (uint64_t)1);
    FL_ASSERT_NEQ_UINT64((uint64_t)1, (uint64_t)2);
    FL_ASSERT_LT_UINT64((uint64_t)1, (uint64_t)2);
    FL_ASSERT_LE_UINT64((uint64_t)1, (uint64_t)1);
    FL_ASSERT_LE_UINT64((uint64_t)1, (uint64_t)2);
    FL_ASSERT_GT_UINT64((uint64_t)2, (uint64_t)1);
    FL_ASSERT_GE_UINT64((uint64_t)1, (uint64_t)1);
    FL_ASSERT_GE_UINT64((uint64_t)2, (uint64_t)1);
}

FL_TEST("FL_ASSERT_*_UINT64 throws fl_unexpected_failure for violated uint64_t "
        "relationships",
        test_binop_uint64_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_UINT64((uint64_t)1, (uint64_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_UINT64((uint64_t)1, (uint64_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_UINT64((uint64_t)2, (uint64_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_UINT64((uint64_t)2, (uint64_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_UINT64((uint64_t)1, (uint64_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_UINT64((uint64_t)1, (uint64_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - INT64  (PRId64)
// ============================================================

FL_TEST("FL_ASSERT_*_INT64 passes for correct int64_t relationships",
        test_binop_int64_pass) {
    FL_ASSERT_EQ_INT64((int64_t)1, (int64_t)1);
    FL_ASSERT_NEQ_INT64((int64_t)1, (int64_t)2);
    FL_ASSERT_LT_INT64((int64_t)1, (int64_t)2);
    FL_ASSERT_LE_INT64((int64_t)1, (int64_t)1);
    FL_ASSERT_GT_INT64((int64_t)2, (int64_t)1);
    FL_ASSERT_GE_INT64((int64_t)1, (int64_t)1);
}

FL_TEST(
    "FL_ASSERT_*_INT64 throws fl_unexpected_failure for violated int64_t relationships",
    test_binop_int64_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_INT64((int64_t)1, (int64_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_INT64((int64_t)1, (int64_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_INT64((int64_t)2, (int64_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_INT64((int64_t)2, (int64_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_INT64((int64_t)1, (int64_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_INT64((int64_t)1, (int64_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - INTPTR  (PRId64)
// ============================================================

FL_TEST("FL_ASSERT_*_INTPTR passes for correct intptr_t relationships",
        test_binop_intptr_pass) {
    FL_ASSERT_EQ_INTPTR((intptr_t)1, (intptr_t)1);
    FL_ASSERT_NEQ_INTPTR((intptr_t)1, (intptr_t)2);
    FL_ASSERT_LT_INTPTR((intptr_t)1, (intptr_t)2);
    FL_ASSERT_LE_INTPTR((intptr_t)1, (intptr_t)1);
    FL_ASSERT_GT_INTPTR((intptr_t)2, (intptr_t)1);
    FL_ASSERT_GE_INTPTR((intptr_t)1, (intptr_t)1);
}

FL_TEST("FL_ASSERT_*_INTPTR throws fl_unexpected_failure for violated intptr_t "
        "relationships",
        test_binop_intptr_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_INTPTR((intptr_t)1, (intptr_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_INTPTR((intptr_t)1, (intptr_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_INTPTR((intptr_t)2, (intptr_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_INTPTR((intptr_t)2, (intptr_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_INTPTR((intptr_t)1, (intptr_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_INTPTR((intptr_t)1, (intptr_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_BINOP - UINTPTR  (PRIu64)
// ============================================================

FL_TEST("FL_ASSERT_*_UINTPTR passes for correct uintptr_t relationships",
        test_binop_uintptr_pass) {
    FL_ASSERT_EQ_UINTPTR((uintptr_t)1, (uintptr_t)1);
    FL_ASSERT_NEQ_UINTPTR((uintptr_t)1, (uintptr_t)2);
    FL_ASSERT_LT_UINTPTR((uintptr_t)1, (uintptr_t)2);
    FL_ASSERT_LE_UINTPTR((uintptr_t)1, (uintptr_t)1);
    FL_ASSERT_GT_UINTPTR((uintptr_t)2, (uintptr_t)1);
    FL_ASSERT_GE_UINTPTR((uintptr_t)1, (uintptr_t)1);
}

FL_TEST("FL_ASSERT_*_UINTPTR throws fl_unexpected_failure for violated uintptr_t "
        "relationships",
        test_binop_uintptr_fail) {
    bool volatile caught;

    caught = false;
    FL_TRY {
        FL_ASSERT_EQ_UINTPTR((uintptr_t)1, (uintptr_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_NEQ_UINTPTR((uintptr_t)1, (uintptr_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LT_UINTPTR((uintptr_t)2, (uintptr_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_LE_UINTPTR((uintptr_t)2, (uintptr_t)1);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GT_UINTPTR((uintptr_t)1, (uintptr_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);

    caught = false;
    FL_TRY {
        FL_ASSERT_GE_UINTPTR((uintptr_t)1, (uintptr_t)2);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// Pointer assertions: FL_ASSERT_NULL, FL_ASSERT_NOT_NULL
// FL_ASSERT_EQ_PTR, FL_ASSERT_NEQ_PTR
// ============================================================

FL_TEST("FL_ASSERT_NULL passes for NULL pointer", test_null_pass) {
    void *p = NULL;
    FL_ASSERT_NULL(p);
}

FL_TEST("FL_ASSERT_NULL throws fl_invalid_value for non-NULL pointer", test_null_fail) {
    bool volatile caught = false;
    int x                = 0;
    FL_TRY {
        FL_ASSERT_NULL(&x);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_NOT_NULL passes for non-NULL pointer", test_not_null_pass) {
    int x = 0;
    FL_ASSERT_NOT_NULL(&x);
}

FL_TEST("FL_ASSERT_NOT_NULL throws fl_invalid_value for NULL pointer",
        test_not_null_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_NOT_NULL(NULL);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_EQ_PTR passes when pointers are equal", test_eq_ptr_pass) {
    int x = 0;
    FL_ASSERT_EQ_PTR(&x, &x);
}

FL_TEST("FL_ASSERT_EQ_PTR throws fl_unexpected_failure when pointers differ",
        test_eq_ptr_fail) {
    bool volatile caught = false;
    int a = 0, b = 0;
    FL_TRY {
        FL_ASSERT_EQ_PTR(&a, &b);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_NEQ_PTR passes when pointers differ", test_neq_ptr_pass) {
    int a = 0, b = 0;
    FL_ASSERT_NEQ_PTR(&a, &b);
}

FL_TEST("FL_ASSERT_NEQ_PTR throws fl_unexpected_failure when pointers are equal",
        test_neq_ptr_fail) {
    bool volatile caught = false;
    int x                = 0;
    FL_TRY {
        FL_ASSERT_NEQ_PTR(&x, &x);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// String assertions: FL_ASSERT_STR_EQ, FL_ASSERT_STR_NEQ
// ============================================================

FL_TEST("FL_ASSERT_STR_EQ passes for equal strings", test_str_eq_pass) {
    FL_ASSERT_STR_EQ("hello", "hello");
}

FL_TEST("FL_ASSERT_STR_EQ throws fl_unexpected_failure for different strings",
        test_str_eq_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_STR_EQ("hello", "world");
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_STR_NEQ passes for different strings", test_str_neq_pass) {
    FL_ASSERT_STR_NEQ("hello", "world");
}

FL_TEST("FL_ASSERT_STR_NEQ throws fl_unexpected_failure for equal strings",
        test_str_neq_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_STR_NEQ("hello", "hello");
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// String assertions: FL_ASSERT_STR_CONTAINS
// ============================================================

FL_TEST("FL_ASSERT_STR_CONTAINS passes when substring is found",
        test_str_contains_pass) {
    FL_ASSERT_STR_CONTAINS("hello world", "world");
    FL_ASSERT_STR_CONTAINS("hello world", "hello");
    FL_ASSERT_STR_CONTAINS("hello world", "o wo");
}

FL_TEST("FL_ASSERT_STR_CONTAINS throws fl_unexpected_failure when substring is absent",
        test_str_contains_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_STR_CONTAINS("hello world", "goodbye");
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// Memory assertions: FL_ASSERT_MEM_EQ, FL_ASSERT_MEM_NEQ
// ============================================================

FL_TEST("FL_ASSERT_MEM_EQ passes for identical memory blocks", test_mem_eq_pass) {
    char a[4] = {1, 2, 3, 4};
    char b[4] = {1, 2, 3, 4};
    FL_ASSERT_MEM_EQ(a, b, sizeof a);
}

FL_TEST("FL_ASSERT_MEM_EQ throws fl_unexpected_failure for different memory blocks",
        test_mem_eq_fail) {
    bool volatile caught = false;
    char a[4]            = {1, 2, 3, 4};
    char b[4]            = {1, 2, 3, 5};
    FL_TRY {
        FL_ASSERT_MEM_EQ(a, b, sizeof a);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_MEM_NEQ passes for different memory blocks", test_mem_neq_pass) {
    char a[4] = {1, 2, 3, 4};
    char b[4] = {1, 2, 3, 5};
    FL_ASSERT_MEM_NEQ(a, b, sizeof a);
}

FL_TEST("FL_ASSERT_MEM_NEQ throws fl_unexpected_failure for identical memory blocks",
        test_mem_neq_fail) {
    bool volatile caught = false;
    char a[4]            = {1, 2, 3, 4};
    char b[4]            = {1, 2, 3, 4};
    FL_TRY {
        FL_ASSERT_MEM_NEQ(a, b, sizeof a);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ==================================================================
// Floating-point assertions: FL_ASSERT_FLOAT_EQ, FL_ASSERT_DOUBLE_EQ
// ==================================================================

FL_TEST("FL_ASSERT_FLOAT_EQ passes when values are within epsilon", test_float_eq_pass) {
    FL_ASSERT_FLOAT_EQ(1.0f, 1.0f, 0.001f);
    FL_ASSERT_FLOAT_EQ(1.0f, 1.0005f, 0.001f);
}

FL_TEST("FL_ASSERT_FLOAT_EQ throws fl_unexpected_failure when values exceed epsilon",
        test_float_eq_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_FLOAT_EQ(1.0f, 2.0f, 0.001f);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_DOUBLE_EQ passes when values are within epsilon",
        test_double_eq_pass) {
    FL_ASSERT_DOUBLE_EQ(1.0, 1.0, 0.001);
    FL_ASSERT_DOUBLE_EQ(1.0, 1.0005, 0.001);
}

FL_TEST("FL_ASSERT_DOUBLE_EQ throws fl_unexpected_failure when values exceed epsilon",
        test_double_eq_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_DOUBLE_EQ(1.0, 2.0, 0.001);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ====================================================================
// Floating-point assertions: FL_ASSERT_FLOAT_NEQ, FL_ASSERT_DOUBLE_NEQ
// ====================================================================

FL_TEST("FL_ASSERT_FLOAT_NEQ passes when values are not within epsilon",
        test_float_neq_pass) {
    FL_ASSERT_FLOAT_NEQ(1.0f, 1.1f, 0.001f);
    FL_ASSERT_FLOAT_NEQ(1.0f, 1.0011f, 0.001f);
}

FL_TEST(
    "FL_ASSERT_FLOAT_NEQ throws fl_unexpected_failure when values are within epsilon",
    test_float_neq_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_FLOAT_NEQ(1.0f, 1.0001f, 0.001f);
    }
    FL_CATCH_ALL {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_DOUBLE_NEQ passes when values are not within epsilon",
        test_double_neq_pass) {
    FL_ASSERT_DOUBLE_NEQ(1.0, 1.1, 0.001);
    FL_ASSERT_DOUBLE_NEQ(1.0, 1.0011, 0.001);
}

FL_TEST(
    "FL_ASSERT_DOUBLE_NEQ throws fl_unexpected_failure when values are within epsilon",
    test_double_neq_fail) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_DOUBLE_NEQ(1.0, 1.0, 0.001);
    }
    FL_CATCH(fl_unexpected_failure) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_SIZE_RANGE
// ============================================================

FL_TEST("FL_ASSERT_SIZE_RANGE passes when size is within bounds", test_size_range_pass) {
    FL_ASSERT_SIZE_RANGE((size_t)5, (size_t)0, (size_t)10, __FILE__, __LINE__);
    FL_ASSERT_SIZE_RANGE((size_t)0, (size_t)0, (size_t)10, __FILE__, __LINE__);
    FL_ASSERT_SIZE_RANGE((size_t)10, (size_t)0, (size_t)10, __FILE__, __LINE__);
}

FL_TEST("FL_ASSERT_SIZE_RANGE throws fl_invalid_value when size is below min",
        test_size_range_fail_below) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_SIZE_RANGE((size_t)0, (size_t)1, (size_t)10, __FILE__, __LINE__);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

FL_TEST("FL_ASSERT_SIZE_RANGE throws fl_invalid_value when size exceeds max",
        test_size_range_fail_above) {
    bool volatile caught = false;
    FL_TRY {
        FL_ASSERT_SIZE_RANGE((size_t)11, (size_t)0, (size_t)10, __FILE__, __LINE__);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// FL_ASSERT_MULTIPLICATION_OVERFLOW
// ============================================================

FL_TEST("FL_ASSERT_MULTIPLICATION_OVERFLOW passes for safe multiplications",
        test_mul_overflow_pass) {
    FL_ASSERT_MULTIPLICATION_OVERFLOW((size_t)2, (size_t)3, __FILE__, __LINE__);
    FL_ASSERT_MULTIPLICATION_OVERFLOW((size_t)0, SIZE_MAX, __FILE__, __LINE__);
    FL_ASSERT_MULTIPLICATION_OVERFLOW((size_t)1, SIZE_MAX, __FILE__, __LINE__);
}

FL_TEST("FL_ASSERT_MULTIPLICATION_OVERFLOW throws fl_invalid_value on overflow",
        test_mul_overflow_fail) {
    bool volatile caught = false;
    // SIZE_MAX/2 + 1 multiplied by 2 overflows size_t
    size_t a = SIZE_MAX / 2 + 1;
    FL_TRY {
        FL_ASSERT_MULTIPLICATION_OVERFLOW(a, (size_t)2, __FILE__, __LINE__);
    }
    FL_CATCH(fl_invalid_value) {
        caught = true;
    }
    FL_END_TRY;
    FL_ASSERT_TRUE(caught);
}

// ============================================================
// Suite registration
// ============================================================

FL_SUITE_BEGIN(ts)
FL_SUITE_ADD(test_assert_pass)
FL_SUITE_ADD(test_assert_fail)
FL_SUITE_ADD(test_assert_details_pass)
FL_SUITE_ADD(test_assert_details_fail)
FL_SUITE_ADD(test_assert_file_line_pass)
FL_SUITE_ADD(test_assert_file_line_fail)
FL_SUITE_ADD(test_assert_details_file_line_pass)
FL_SUITE_ADD(test_assert_details_file_line_fail)
FL_SUITE_ADD(test_fail_macro)
FL_SUITE_ADD(test_assert_true_pass)
FL_SUITE_ADD(test_assert_true_fail)
FL_SUITE_ADD(test_assert_false_pass)
FL_SUITE_ADD(test_assert_false_fail)
FL_SUITE_ADD(test_zero_pass)
FL_SUITE_ADD(test_zero_fail)
FL_SUITE_ADD(test_non_zero_pass)
FL_SUITE_ADD(test_non_zero_fail)
FL_SUITE_ADD(test_binop_char_pass)
FL_SUITE_ADD(test_binop_char_fail)
FL_SUITE_ADD(test_binop_int_pass)
FL_SUITE_ADD(test_binop_int_fail)
FL_SUITE_ADD(test_binop_sint_pass)
FL_SUITE_ADD(test_binop_sint_fail)
FL_SUITE_ADD(test_binop_int8_pass)
FL_SUITE_ADD(test_binop_int8_fail)
FL_SUITE_ADD(test_binop_int16_pass)
FL_SUITE_ADD(test_binop_int16_fail)
FL_SUITE_ADD(test_binop_int32_pass)
FL_SUITE_ADD(test_binop_int32_fail)
FL_SUITE_ADD(test_binop_int64_pass)
FL_SUITE_ADD(test_binop_int64_fail)
FL_SUITE_ADD(test_binop_uint_pass)
FL_SUITE_ADD(test_binop_uint_fail)
FL_SUITE_ADD(test_binop_uchar_pass)
FL_SUITE_ADD(test_binop_uchar_fail)
FL_SUITE_ADD(test_binop_uint8_pass)
FL_SUITE_ADD(test_binop_uint8_fail)
FL_SUITE_ADD(test_binop_uint16_pass)
FL_SUITE_ADD(test_binop_uint16_fail)
FL_SUITE_ADD(test_binop_uint32_pass)
FL_SUITE_ADD(test_binop_uint32_fail)
FL_SUITE_ADD(test_binop_uint64_pass)
FL_SUITE_ADD(test_binop_uint64_fail)
FL_SUITE_ADD(test_binop_size_t_pass)
FL_SUITE_ADD(test_binop_size_t_fail)
FL_SUITE_ADD(test_binop_ptrdiff_pass)
FL_SUITE_ADD(test_binop_ptrdiff_fail)
FL_SUITE_ADD(test_binop_intptr_pass)
FL_SUITE_ADD(test_binop_intptr_fail)
FL_SUITE_ADD(test_binop_uintptr_pass)
FL_SUITE_ADD(test_binop_uintptr_fail)
FL_SUITE_ADD(test_null_pass)
FL_SUITE_ADD(test_null_fail)
FL_SUITE_ADD(test_not_null_pass)
FL_SUITE_ADD(test_not_null_fail)
FL_SUITE_ADD(test_eq_ptr_pass)
FL_SUITE_ADD(test_eq_ptr_fail)
FL_SUITE_ADD(test_neq_ptr_pass)
FL_SUITE_ADD(test_neq_ptr_fail)
FL_SUITE_ADD(test_str_eq_pass)
FL_SUITE_ADD(test_str_eq_fail)
FL_SUITE_ADD(test_str_neq_pass)
FL_SUITE_ADD(test_str_neq_fail)
FL_SUITE_ADD(test_str_contains_pass)
FL_SUITE_ADD(test_str_contains_fail)
FL_SUITE_ADD(test_mem_eq_pass)
FL_SUITE_ADD(test_mem_eq_fail)
FL_SUITE_ADD(test_mem_neq_pass)
FL_SUITE_ADD(test_mem_neq_fail)
FL_SUITE_ADD(test_float_eq_pass)
FL_SUITE_ADD(test_float_eq_fail)
FL_SUITE_ADD(test_float_neq_pass)
FL_SUITE_ADD(test_float_neq_fail)
FL_SUITE_ADD(test_double_eq_pass)
FL_SUITE_ADD(test_double_eq_fail)
FL_SUITE_ADD(test_double_neq_pass)
FL_SUITE_ADD(test_double_neq_fail)
FL_SUITE_ADD(test_size_range_pass)
FL_SUITE_ADD(test_size_range_fail_below)
FL_SUITE_ADD(test_size_range_fail_above)
FL_SUITE_ADD(test_mul_overflow_pass)
FL_SUITE_ADD(test_mul_overflow_fail)
FL_SUITE_END;

FL_GET_TEST_SUITE("Assert Macros", ts);
