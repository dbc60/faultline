#ifndef FL_EXCEPTION_SERVICE_ASSERT_H_
#define FL_EXCEPTION_SERVICE_ASSERT_H_

/**
 * @file fl_exception_service_assert.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-02-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_exception_service.h> // FL_THROW_EXCEPTION_SERVICE_FN
#include <faultline/fl_exception_types.h>   // FLExceptionReason, fl_exception_handler_fn
#include <faultline/fl_macros.h>            // FL_THREAD_LOCAL, FL_UNUSED

#include <inttypes.h> // "%u", "%lld", etc.
#include <math.h>     // fabs
#include <string.h>   // strcmp, memcmp
#include <stddef.h>   // size_t, NULL
#include <stdio.h>    // snprintf

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief this is primarily used in various FL_ASSERT macros to indicate an unexpected
 * failure.
 */
extern FLExceptionReason fl_unexpected_failure;

/**
 * @brief Throw an exception when an assertion fails. Include the file name and
 * line number on which the assertion failed.
 *
 * @param reason a string representing the assertion failure.
 * @param file_name the full path to the file in which the assertion failed.
 * @param line the line number of the failed assertion.
 */
FL_THROW_EXCEPTION_SERVICE_FN(fl_throw_assertion);

/**
 * @brief test an assertion; if it fails throw an exception where the reason is
 * a string representing the assertion.
 */
#define FL_ASSERT(e)                                            \
    do {                                                        \
        if (!(e)) {                                             \
            fl_throw_assertion((#e), NULL, __FILE__, __LINE__); \
        }                                                       \
    } while (0)

/**
 * @brief test an assertion; if it fails throw an exception with a reason and details
 */
#define FL_ASSERT_DETAILS(e, d, ...)                                     \
    do {                                                                 \
        if (!(e)) {                                                      \
            static FL_THREAD_LOCAL char _details[FL_MAX_DETAILS_LENGTH]; \
            snprintf(_details, sizeof _details, d, ##__VA_ARGS__);       \
            fl_throw_assertion((#e), _details, __FILE__, __LINE__);      \
        }                                                                \
    } while (0)

/**
 * @brief test an assertion; if it fails throw an exception where the reason is
 * a string representing the assertion.
 */
#define FL_ASSERT_FILE_LINE(e, FILE, LINE)              \
    do {                                                \
        if (!(e)) {                                     \
            fl_throw_assertion((#e), NULL, FILE, LINE); \
        }                                               \
    } while (0)

#define FL_ASSERT_DETAILS_FILE_LINE(e, d, FILE, LINE, ...)               \
    do {                                                                 \
        if (!(e)) {                                                      \
            static FL_THREAD_LOCAL char _details[FL_MAX_DETAILS_LENGTH]; \
            snprintf(_details, sizeof _details, (d), ##__VA_ARGS__);     \
            fl_throw_assertion((#e), _details, FILE, LINE);              \
        }                                                                \
    } while (0)

// Unconditional failure
#define FL_FAIL(msg, ...) FL_ASSERT_IMPL("Failure: " msg, ##__VA_ARGS__)

// Boolean assertions
#define FL_ASSERT_TRUE(expr)                                             \
    do {                                                                 \
        if (!(expr)) {                                                   \
            FL_ASSERT_IMPL("Expected: true: %s. Actual: false.", #expr); \
        }                                                                \
    } while (0)

#define FL_ASSERT_FALSE(expr)                                            \
    do {                                                                 \
        if (expr) {                                                      \
            FL_ASSERT_IMPL("Expected: false: %s. Actual: true.", #expr); \
        }                                                                \
    } while (0)

#define FL_ASSERT_ZERO_TYPED(V, FMT)                                                \
    do {                                                                            \
        if ((V) != 0) {                                                             \
            FL_ASSERT_REASON_IMPL(fl_invalid_value, "Expected: zero. Actual: " FMT, \
                                  (V));                                             \
        }                                                                           \
    } while (0)

// Caveat: %c formats a null byte invisibly; consider using FL_ASSERT_ZERO_INT instead
#define FL_ASSERT_ZERO_CHAR(V)    FL_ASSERT_ZERO_TYPED((V), "%c")
#define FL_ASSERT_ZERO_SCHAR(V)   FL_ASSERT_ZERO_TYPED((V), "%hhd")
#define FL_ASSERT_ZERO_UCHAR(V)   FL_ASSERT_ZERO_TYPED((V), "%hhu")
#define FL_ASSERT_ZERO_INT(V)     FL_ASSERT_ZERO_TYPED((V), "%d")
#define FL_ASSERT_ZERO_SINT(V)    FL_ASSERT_ZERO_TYPED((V), "%d")
#define FL_ASSERT_ZERO_INT8(V)    FL_ASSERT_ZERO_TYPED((V), "%" PRId8)
#define FL_ASSERT_ZERO_INT16(V)   FL_ASSERT_ZERO_TYPED((V), "%" PRId16)
#define FL_ASSERT_ZERO_INT32(V)   FL_ASSERT_ZERO_TYPED((V), "%" PRId32)
#define FL_ASSERT_ZERO_INT64(V)   FL_ASSERT_ZERO_TYPED((V), "%" PRId64)
#define FL_ASSERT_ZERO_SIZE_T(V)  FL_ASSERT_ZERO_TYPED((V), "%zu")
#define FL_ASSERT_ZERO_PTRDIFF(V) FL_ASSERT_ZERO_TYPED((V), "%" PRId64)
#define FL_ASSERT_ZERO_UINT(V)    FL_ASSERT_ZERO_TYPED((V), "%u")
#define FL_ASSERT_ZERO_UINT8(V)   FL_ASSERT_ZERO_TYPED((V), "%" PRIu8)
#define FL_ASSERT_ZERO_UINT16(V)  FL_ASSERT_ZERO_TYPED((V), "%" PRIu16)
#define FL_ASSERT_ZERO_UINT32(V)  FL_ASSERT_ZERO_TYPED((V), "%" PRIu32)
#define FL_ASSERT_ZERO_UINT64(V)  FL_ASSERT_ZERO_TYPED((V), "%" PRIu64)
#define FL_ASSERT_ZERO_INTPTR(V)  FL_ASSERT_ZERO_TYPED((V), "%" PRId64)
#define FL_ASSERT_ZERO_UINTPTR(V) FL_ASSERT_ZERO_TYPED((V), "%" PRIu64)

#define FL_ASSERT_NON_ZERO_TYPED(V, FMT)                                                \
    do {                                                                                \
        if ((V) == 0) {                                                                 \
            FL_ASSERT_REASON_IMPL(fl_invalid_value, "Expected: non-zero. Actual: " FMT, \
                                  (V));                                                 \
        }                                                                               \
    } while (0)

// Caveat: %c formats a null byte invisibly; consider using FL_ASSERT_NON_ZERO_INT
// instead
#define FL_ASSERT_NON_ZERO_CHAR(V)    FL_ASSERT_NON_ZERO_TYPED((V), "%c")
#define FL_ASSERT_NON_ZERO_SCHAR(V)   FL_ASSERT_NON_ZERO_TYPED((V), "%hhd")
#define FL_ASSERT_NON_ZERO_UCHAR(V)   FL_ASSERT_NON_ZERO_TYPED((V), "%hhu")
#define FL_ASSERT_NON_ZERO_INT(V)     FL_ASSERT_NON_ZERO_TYPED((V), "%d")
#define FL_ASSERT_NON_ZERO_SINT(V)    FL_ASSERT_NON_ZERO_TYPED((V), "%d")
#define FL_ASSERT_NON_ZERO_INT8(V)    FL_ASSERT_NON_ZERO_TYPED((V), "%" PRId8)
#define FL_ASSERT_NON_ZERO_INT16(V)   FL_ASSERT_NON_ZERO_TYPED((V), "%" PRId16)
#define FL_ASSERT_NON_ZERO_INT32(V)   FL_ASSERT_NON_ZERO_TYPED((V), "%" PRId32)
#define FL_ASSERT_NON_ZERO_INT64(V)   FL_ASSERT_NON_ZERO_TYPED((V), "%" PRId64)
#define FL_ASSERT_NON_ZERO_SIZE_T(V)  FL_ASSERT_NON_ZERO_TYPED((V), "%zu")
#define FL_ASSERT_NON_ZERO_PTRDIFF(V) FL_ASSERT_NON_ZERO_TYPED((V), "%" PRId64)
#define FL_ASSERT_NON_ZERO_UINT(V)    FL_ASSERT_NON_ZERO_TYPED((V), "%u")
#define FL_ASSERT_NON_ZERO_UINT8(V)   FL_ASSERT_NON_ZERO_TYPED((V), "%" PRIu8)
#define FL_ASSERT_NON_ZERO_UINT16(V)  FL_ASSERT_NON_ZERO_TYPED((V), "%" PRIu16)
#define FL_ASSERT_NON_ZERO_UINT32(V)  FL_ASSERT_NON_ZERO_TYPED((V), "%" PRIu32)
#define FL_ASSERT_NON_ZERO_UINT64(V)  FL_ASSERT_NON_ZERO_TYPED((V), "%" PRIu64)
#define FL_ASSERT_NON_ZERO_INTPTR(V)  FL_ASSERT_NON_ZERO_TYPED((V), "%" PRId64)
#define FL_ASSERT_NON_ZERO_UINTPTR(V) FL_ASSERT_NON_ZERO_TYPED((V), "%" PRIu64)

// Base macro used to implement all typed binary assertions (==, !=, <, etc.)
#define FL_ASSERT_BINOP_TYPED(lhs, rhs, op, op_str, fmt)                        \
    do {                                                                        \
        if (!((lhs)op(rhs))) {                                                  \
            FL_ASSERT_IMPL("Expected: " fmt " " op_str " " fmt ". Actual: " fmt \
                           " and " fmt,                                         \
                           (lhs), (rhs), (lhs), (rhs));                         \
        }                                                                       \
    } while (0)

// Caveat: %c formats a null byte invisibly; consider using FL_ASSERT_*_INT instead
#define FL_ASSERT_EQ_CHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%c")
#define FL_ASSERT_NEQ_CHAR(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%c")
#define FL_ASSERT_LT_CHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%c")
#define FL_ASSERT_LE_CHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%c")
#define FL_ASSERT_GT_CHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%c")
#define FL_ASSERT_GE_CHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%c")

#define FL_ASSERT_EQ_INT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%d")
#define FL_ASSERT_NEQ_INT(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%d")
#define FL_ASSERT_LT_INT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%d")
#define FL_ASSERT_LE_INT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%d")
#define FL_ASSERT_GT_INT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%d")
#define FL_ASSERT_GE_INT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%d")

#define FL_ASSERT_EQ_SINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%d")
#define FL_ASSERT_NEQ_SINT(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%d")
#define FL_ASSERT_LT_SINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%d")
#define FL_ASSERT_LE_SINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%d")
#define FL_ASSERT_GT_SINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%d")
#define FL_ASSERT_GE_SINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%d")

#define FL_ASSERT_EQ_INT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRId8)
#define FL_ASSERT_NEQ_INT8(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRId8)
#define FL_ASSERT_LT_INT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRId8)
#define FL_ASSERT_LE_INT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRId8)
#define FL_ASSERT_GT_INT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRId8)
#define FL_ASSERT_GE_INT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRId8)

#define FL_ASSERT_EQ_INT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRId16)
#define FL_ASSERT_NEQ_INT16(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRId16)
#define FL_ASSERT_LT_INT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRId16)
#define FL_ASSERT_LE_INT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRId16)
#define FL_ASSERT_GT_INT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRId16)
#define FL_ASSERT_GE_INT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRId16)

#define FL_ASSERT_EQ_INT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRId32)
#define FL_ASSERT_NEQ_INT32(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRId32)
#define FL_ASSERT_LT_INT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRId32)
#define FL_ASSERT_LE_INT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRId32)
#define FL_ASSERT_GT_INT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRId32)
#define FL_ASSERT_GE_INT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRId32)

#define FL_ASSERT_EQ_INT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRId64)
#define FL_ASSERT_NEQ_INT64(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRId64)
#define FL_ASSERT_LT_INT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRId64)
#define FL_ASSERT_LE_INT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRId64)
#define FL_ASSERT_GT_INT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRId64)
#define FL_ASSERT_GE_INT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRId64)

// Caveat: %c formats a null byte invisibly; consider using FL_ASSERT_*_UINT instead
#define FL_ASSERT_EQ_UCHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%c")
#define FL_ASSERT_NEQ_UCHAR(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%c")
#define FL_ASSERT_LT_UCHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%c")
#define FL_ASSERT_LE_UCHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%c")
#define FL_ASSERT_GT_UCHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%c")
#define FL_ASSERT_GE_UCHAR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%c")

#define FL_ASSERT_EQ_UINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%u")
#define FL_ASSERT_NEQ_UINT(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%u")
#define FL_ASSERT_LT_UINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%u")
#define FL_ASSERT_LE_UINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%u")
#define FL_ASSERT_GT_UINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%u")
#define FL_ASSERT_GE_UINT(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%u")

#define FL_ASSERT_EQ_SIZE_T(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%zu")
#define FL_ASSERT_NEQ_SIZE_T(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%zu")
#define FL_ASSERT_LT_SIZE_T(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%zu")
#define FL_ASSERT_LE_SIZE_T(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%zu")
#define FL_ASSERT_GT_SIZE_T(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%zu")
#define FL_ASSERT_GE_SIZE_T(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%zu")

#define FL_ASSERT_EQ_PTRDIFF(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRId64)
#define FL_ASSERT_NEQ_PTRDIFF(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRId64)
#define FL_ASSERT_LT_PTRDIFF(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRId64)
#define FL_ASSERT_LE_PTRDIFF(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRId64)
#define FL_ASSERT_GT_PTRDIFF(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRId64)
#define FL_ASSERT_GE_PTRDIFF(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRId64)

#define FL_ASSERT_EQ_UINT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRIu8)
#define FL_ASSERT_NEQ_UINT8(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRIu8)
#define FL_ASSERT_LT_UINT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRIu8)
#define FL_ASSERT_LE_UINT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRIu8)
#define FL_ASSERT_GT_UINT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRIu8)
#define FL_ASSERT_GE_UINT8(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRIu8)

#define FL_ASSERT_EQ_UINT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRIu16)
#define FL_ASSERT_NEQ_UINT16(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRIu16)
#define FL_ASSERT_LT_UINT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRIu16)
#define FL_ASSERT_LE_UINT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRIu16)
#define FL_ASSERT_GT_UINT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRIu16)
#define FL_ASSERT_GE_UINT16(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRIu16)

#define FL_ASSERT_EQ_UINT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRIu32)
#define FL_ASSERT_NEQ_UINT32(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRIu32)
#define FL_ASSERT_LT_UINT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRIu32)
#define FL_ASSERT_LE_UINT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRIu32)
#define FL_ASSERT_GT_UINT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRIu32)
#define FL_ASSERT_GE_UINT32(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRIu32)

#define FL_ASSERT_EQ_UINT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRIu64)
#define FL_ASSERT_NEQ_UINT64(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRIu64)
#define FL_ASSERT_LT_UINT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRIu64)
#define FL_ASSERT_LE_UINT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRIu64)
#define FL_ASSERT_GT_UINT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRIu64)
#define FL_ASSERT_GE_UINT64(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRIu64)

#define FL_ASSERT_EQ_INTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRId64)
#define FL_ASSERT_NEQ_INTPTR(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRId64)
#define FL_ASSERT_LT_INTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRId64)
#define FL_ASSERT_LE_INTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRId64)
#define FL_ASSERT_GT_INTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRId64)
#define FL_ASSERT_GE_INTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRId64)

#define FL_ASSERT_EQ_UINTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, ==, "==", "%" PRIu64)
#define FL_ASSERT_NEQ_UINTPTR(V1, V2) FL_ASSERT_BINOP_TYPED(V1, V2, !=, "!=", "%" PRIu64)
#define FL_ASSERT_LT_UINTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <, "<", "%" PRIu64)
#define FL_ASSERT_LE_UINTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, <=, "<=", "%" PRIu64)
#define FL_ASSERT_GT_UINTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >, ">", "%" PRIu64)
#define FL_ASSERT_GE_UINTPTR(V1, V2)  FL_ASSERT_BINOP_TYPED(V1, V2, >=, ">=", "%" PRIu64)

// Pointer checks
#define FL_ASSERT_NULL(PTR)                                                       \
    do {                                                                          \
        if ((PTR) != NULL) {                                                      \
            FL_ASSERT_REASON_IMPL(fl_invalid_value, "Expected: NULL. Actual: %p", \
                                  (PTR));                                         \
        }                                                                         \
    } while (0)

#define FL_ASSERT_NOT_NULL(PTR)                                                     \
    do {                                                                            \
        if ((PTR) == NULL) {                                                        \
            FL_ASSERT_REASON_IMPL(fl_invalid_value,                                 \
                                  "Expected: non-NULL pointer. Actual: %p", (PTR)); \
        }                                                                           \
    } while (0)

#define FL_ASSERT_EQ_PTR(V1, V2)                                                    \
    do {                                                                            \
        if ((V1) != (V2)) {                                                         \
            FL_ASSERT_IMPL("Expected: pointer %p. Actual: pointer %p", (V1), (V2)); \
        }                                                                           \
    } while (0)

#define FL_ASSERT_NEQ_PTR(V1, V2)                                                  \
    do {                                                                           \
        if ((V1) == (V2)) {                                                        \
            FL_ASSERT_IMPL("Expected: different pointers. Actual: both %p", (V2)); \
        }                                                                          \
    } while (0)

// String comparisons
#define FL_ASSERT_STR_EQ(V1, V2)                                                   \
    do {                                                                           \
        const char *_e = (V1);                                                     \
        const char *_a = (V2);                                                     \
        if (!_e || !_a || strcmp(_e, _a) != 0) {                                   \
            FL_ASSERT_IMPL("Expected: \"%s\". Actual: \"%s\"", _e ? _e : "(null)", \
                           _a ? _a : "(null)");                                    \
        }                                                                          \
    } while (0)

#define FL_ASSERT_STR_NEQ(V1, V2)                                         \
    do {                                                                  \
        const char *_e = (V1);                                            \
        const char *_a = (V2);                                            \
        if (!_e || !_a || strcmp(_e, _a) == 0) {                          \
            FL_ASSERT_IMPL("Expected: different strings. Actual: \"%s\"", \
                           _a ? _a : "(null)");                           \
        }                                                                 \
    } while (0)

#define FL_ASSERT_STR_CONTAINS(haystack, needle)                                     \
    do {                                                                             \
        const char *_h = (haystack);                                                 \
        const char *_n = (needle);                                                   \
        if (!_h || !_n || strstr(_h, _n) == NULL) {                                  \
            FL_ASSERT_IMPL("Expected: \"%s\" to contain \"%s\"", _h ? _h : "(null)", \
                           _n ? _n : "(null)");                                      \
        }                                                                            \
    } while (0)

// Memory comparison
#define FL_ASSERT_MEM_EQ(V1, V2, size)                                        \
    do {                                                                      \
        if (memcmp((V1), (V2), (size)) != 0) {                                \
            FL_ASSERT_IMPL("Expected: identical memory blocks for %zu bytes", \
                           (size_t)(size));                                   \
        }                                                                     \
    } while (0)

#define FL_ASSERT_MEM_NEQ(V1, V2, size)                                       \
    do {                                                                      \
        if (memcmp((V1), (V2), (size)) == 0) {                                \
            FL_ASSERT_IMPL("Expected: different memory blocks for %zu bytes", \
                           (size_t)(size));                                   \
        }                                                                     \
    } while (0)

#define FL_ASSERT_FLOAT_EQ(V1, V2, epsilon)                                        \
    do {                                                                           \
        if (fabs((V1) - (V2)) > (epsilon)) {                                       \
            FL_ASSERT_IMPL("Expected: %.6f ± %.6f. Actual: %.6f", (V1), (epsilon), \
                           (V2));                                                  \
        }                                                                          \
    } while (0)

#define FL_ASSERT_FLOAT_NEQ(V1, V2, epsilon)                                       \
    do {                                                                           \
        if (fabs((V1) - (V2)) <= (epsilon)) {                                      \
            FL_ASSERT_IMPL("Expected: %.6f ± %.6f. Actual: %.6f", (V1), (epsilon), \
                           (V2));                                                  \
        }                                                                          \
    } while (0)

#define FL_ASSERT_DOUBLE_EQ(V1, V2, epsilon)                                          \
    do {                                                                              \
        if (fabs((V1) - (V2)) > (epsilon)) {                                          \
            FL_ASSERT_IMPL("Expected: %.6lf ± %.6lf. Actual: %.6lf", (V1), (epsilon), \
                           (V2));                                                     \
        }                                                                             \
    } while (0)

#define FL_ASSERT_DOUBLE_NEQ(V1, V2, epsilon)                                         \
    do {                                                                              \
        if (fabs((V1) - (V2)) <= (epsilon)) {                                         \
            FL_ASSERT_IMPL("Expected: %.6lf ± %.6lf. Actual: %.6lf", (V1), (epsilon), \
                           (V2));                                                     \
        }                                                                             \
    } while (0)

#define FL_ASSERT_SIZE_RANGE(size, min, max, file, line)                               \
    do {                                                                               \
        if ((size) < (min) || (size) > (max)) {                                        \
            FL_THROW_DETAILS_FILE_LINE(fl_invalid_value,                               \
                                       "size %zu out of range [%zu, %zu]", file, line, \
                                       (size_t)(size), (size_t)(min), (size_t)(max));  \
        }                                                                              \
    } while (0)

#define FL_ASSERT_MULTIPLICATION_OVERFLOW(a, b, file, line)                        \
    do {                                                                           \
        if ((a) > 0 && (b) > SIZE_MAX / (a)) {                                     \
            FL_THROW_DETAILS_FILE_LINE(fl_invalid_value,                           \
                                       "multiplication overflow: %zu * %zu", file, \
                                       line, (size_t)(a), (size_t)(b));            \
        }                                                                          \
    } while (0)

// Generic macros (type-agnostic), but requires C11 or later for _Generic support
// Customize the fallback behavior of the generic assertions. Override with:
//  #define FL_ASSERT_EQ_CUSTOM MyCustomType: MY_ASSERT_EQ

// I found that using _Generic to define type-agnostic macros leads to a portability
// issue that would require platform-specific #ifdef guards to fix, so I'm just deleting
// the definitions.

#if defined(__cplusplus)
}
#endif

#endif // FL_EXCEPTION_SERVICE_ASSERT_H_
