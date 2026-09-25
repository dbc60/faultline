#ifndef FL_EXCEPTION_ASSERT_H_
#define FL_EXCEPTION_ASSERT_H_

/**
 * @file fl_exception_assert.h
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2026-02-06
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <faultline/fl_try.h> // FL_ASSERT_IMPL, FL_ASSERT_REASON_IMPL, FL_THROW_DETAILS_FILE_LINE
#include <faultline/fl_exception.h>       // FL_THROW_EXCEPTION_FN
#include <faultline/fl_exception_types.h> // FLExceptionReason, fl_exception_handler_fn
#include <faultline/fl_macros.h>          // FL_THREAD_LOCAL, FL_UNUSED, FL_TYPEOF

#include <inttypes.h> // "%u", "%lld", etc.
#include <math.h>     // fabs
#include <string.h>   // strcmp, memcmp
#include <stddef.h>   // size_t, NULL
#include <stdbool.h>  // bool (the C++ overload set below)
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
FL_THROW_EXCEPTION_FN(fl_throw_assertion);

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
 * @brief test an assertion; if it fails throw an exception with a reason and details.
 *
 * The details are formatted into the per-thread scratch buffer (fl_details_buf in
 * fl_try.h); see that function for the lifetime rules.
 */
#define FL_ASSERT_DETAILS(e, d, ...)                                        \
    do {                                                                    \
        if (!(e)) {                                                         \
            char *fl_details_ = fl_details_buf();                           \
            snprintf(fl_details_, FL_MAX_DETAILS_LENGTH, d, ##__VA_ARGS__); \
            fl_throw_assertion((#e), fl_details_, __FILE__, __LINE__);      \
        }                                                                   \
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

#define FL_ASSERT_DETAILS_FILE_LINE(e, d, FILE, LINE, ...)                    \
    do {                                                                      \
        if (!(e)) {                                                           \
            char *fl_details_ = fl_details_buf();                             \
            snprintf(fl_details_, FL_MAX_DETAILS_LENGTH, (d), ##__VA_ARGS__); \
            fl_throw_assertion((#e), fl_details_, FILE, LINE);                \
        }                                                                     \
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

/**
 * @brief The longest text any scalar below formats to, including the sign and the
 * terminator. 20 digits covers an unsigned 64-bit value.
 */
#define FL_VALUE_TEXT_MAX 32

/*
 * FL_VALUE_TEXT(BUF, V) writes V to BUF using the conversion its type calls for,
 * so one assertion macro can serve every scalar type instead of one macro per
 * type. The two spellings below select the conversion the same way in each
 * dialect: _Generic in C, an overload set in C++, which the cxx build needs
 * because it compiles these suites with /TP and C++ has no _Generic.
 *
 * The associations name only the basic types. size_t, uint32_t, ptrdiff_t and
 * the rest are typedefs onto those, so naming them as well is a duplicate
 * association -- and which ones collide differs between x86 and x64. They still
 * work as arguments: each resolves to whichever basic type it is defined as.
 *
 * A type no association names reaches the default and formats as a pointer,
 * which /W4 warns about (C4477) and /WX rejects, so an unhandled type fails the
 * build rather than printing garbage.
 */
#if defined(__cplusplus)

// Overloading needs C++ linkage, and this header is inside extern "C".
extern "C++" {
static inline void fl_value_text(char *b, size_t n, bool v) {
    snprintf(b, n, "%d", v);
}

static inline void fl_value_text(char *b, size_t n, char v) {
    snprintf(b, n, "%c", v);
}

static inline void fl_value_text(char *b, size_t n, signed char v) {
    snprintf(b, n, "%hhd", v);
}

static inline void fl_value_text(char *b, size_t n, unsigned char v) {
    snprintf(b, n, "%hhu", v);
}

static inline void fl_value_text(char *b, size_t n, short v) {
    snprintf(b, n, "%hd", v);
}

static inline void fl_value_text(char *b, size_t n, unsigned short v) {
    snprintf(b, n, "%hu", v);
}

static inline void fl_value_text(char *b, size_t n, int v) {
    snprintf(b, n, "%d", v);
}

static inline void fl_value_text(char *b, size_t n, unsigned int v) {
    snprintf(b, n, "%u", v);
}

static inline void fl_value_text(char *b, size_t n, long v) {
    snprintf(b, n, "%ld", v);
}

static inline void fl_value_text(char *b, size_t n, unsigned long v) {
    snprintf(b, n, "%lu", v);
}

static inline void fl_value_text(char *b, size_t n, long long v) {
    snprintf(b, n, "%lld", v);
}

static inline void fl_value_text(char *b, size_t n, unsigned long long v) {
    snprintf(b, n, "%llu", v);
}

static inline void fl_value_text(char *b, size_t n, double v) {
    snprintf(b, n, "%g", v);
}

static inline void fl_value_text(char *b, size_t n, void const *v) {
    snprintf(b, n, "%p", v);
}
}

#define FL_VALUE_TEXT(BUF, V) fl_value_text((BUF), sizeof(BUF), (V))

#else
#define FL_VALUE_TEXT(BUF, V)                \
    snprintf((BUF), sizeof(BUF),             \
             _Generic((V),                   \
                 _Bool: "%d",                \
                 char: "%c",                 \
                 signed char: "%hhd",        \
                 unsigned char: "%hhu",      \
                 short: "%hd",               \
                 unsigned short: "%hu",      \
                 int: "%d",                  \
                 unsigned int: "%u",         \
                 long: "%ld",                \
                 unsigned long: "%lu",       \
                 long long: "%lld",          \
                 unsigned long long: "%llu", \
                 float: "%g",                \
                 double: "%g",               \
                 default: "%p"),             \
             (V))

#endif // __cplusplus

#define FL_ASSERT_ZERO_TYPED(V, FMT)                                                \
    do {                                                                            \
        if ((V) != 0) {                                                             \
            FL_ASSERT_REASON_IMPL(fl_invalid_value, "Expected: zero. Actual: " FMT, \
                                  (V));                                             \
        }                                                                           \
    } while (0)

#define FL_ASSERT_ZERO(V)                                                         \
    do {                                                                          \
        FL_TYPEOF(V) fl_value_ = (V);                                             \
        if (fl_value_ != 0) {                                                     \
            char fl_text_[FL_VALUE_TEXT_MAX];                                     \
            FL_VALUE_TEXT(fl_text_, fl_value_);                                   \
            FL_ASSERT_REASON_IMPL(fl_invalid_value, "Expected: zero. Actual: %s", \
                                  fl_text_);                                      \
        }                                                                         \
    } while (0)

#define FL_ASSERT_NON_ZERO_TYPED(V, FMT)                                                \
    do {                                                                                \
        if ((V) == 0) {                                                                 \
            FL_ASSERT_REASON_IMPL(fl_invalid_value, "Expected: non-zero. Actual: " FMT, \
                                  (V));                                                 \
        }                                                                               \
    } while (0)

// Base macro used to implement all typed binary assertions (==, !=, <, etc.).
// lhs and rhs are captured into temporaries of the given type so each is
// evaluated exactly once, whether the assertion passes or fails. Callers must
// still avoid arguments whose type differs from `type` in a way that would lose
// information when narrowed to it.
#define FL_ASSERT_BINOP_TYPED(type, lhs, rhs, op, op_str, fmt)                  \
    do {                                                                        \
        type fl_lhs_ = (lhs);                                                   \
        type fl_rhs_ = (rhs);                                                   \
        if (!(fl_lhs_ op fl_rhs_)) {                                            \
            FL_ASSERT_IMPL("Expected: " fmt " " op_str " " fmt ". Actual: " fmt \
                           " and " fmt,                                         \
                           fl_lhs_, fl_rhs_, fl_lhs_, fl_rhs_);                 \
        }                                                                       \
    } while (0)

#define FL_ASSERT_NON_ZERO(V)                                                         \
    do {                                                                              \
        FL_TYPEOF(V) fl_value_ = (V);                                                 \
        if (fl_value_ == 0) {                                                         \
            char fl_text_[FL_VALUE_TEXT_MAX];                                         \
            FL_VALUE_TEXT(fl_text_, fl_value_);                                       \
            FL_ASSERT_REASON_IMPL(fl_invalid_value, "Expected: non-zero. Actual: %s", \
                                  fl_text_);                                          \
        }                                                                             \
    } while (0)

/*
 * The type-deducing counterpart of FL_ASSERT_BINOP_TYPED above, and the form to use by
 * default. Both operands are captured into temporaries of the left one's type, so each is
 * evaluated exactly once and the comparison is never between a signed and an unsigned
 * operand. Use FL_ASSERT_BINOP_TYPED instead where the comparison is meant to happen in a
 * type neither operand has, e.g. narrowing a count to int on purpose.
 */
#define FL_ASSERT_BINOP(LHS, RHS, OP, OP_STR)                                      \
    do {                                                                           \
        FL_TYPEOF(LHS) fl_lhs_ = (LHS);                                            \
        FL_TYPEOF(LHS) fl_rhs_ = (FL_TYPEOF(LHS))(RHS);                            \
        if (!(fl_lhs_ OP fl_rhs_)) {                                               \
            char fl_l_[FL_VALUE_TEXT_MAX];                                         \
            char fl_r_[FL_VALUE_TEXT_MAX];                                         \
            FL_VALUE_TEXT(fl_l_, fl_lhs_);                                         \
            FL_VALUE_TEXT(fl_r_, fl_rhs_);                                         \
            FL_ASSERT_IMPL("Expected: %s " OP_STR " %s. Actual: %s and %s", fl_l_, \
                           fl_r_, fl_l_, fl_r_);                                   \
        }                                                                          \
    } while (0)

#define FL_ASSERT_EQ(V1, V2)  FL_ASSERT_BINOP(V1, V2, ==, "==")
#define FL_ASSERT_NEQ(V1, V2) FL_ASSERT_BINOP(V1, V2, !=, "!=")
#define FL_ASSERT_LT(V1, V2)  FL_ASSERT_BINOP(V1, V2, <, "<")
#define FL_ASSERT_LE(V1, V2)  FL_ASSERT_BINOP(V1, V2, <=, "<=")
#define FL_ASSERT_GT(V1, V2)  FL_ASSERT_BINOP(V1, V2, >, ">")
#define FL_ASSERT_GE(V1, V2)  FL_ASSERT_BINOP(V1, V2, >=, ">=")

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
#define FL_ASSERT_STR_EQ(V1, V2)                                                     \
    do {                                                                             \
        const char *fl_expected_ = (V1);                                             \
        const char *fl_actual_   = (V2);                                             \
        if (!fl_expected_ || !fl_actual_ || strcmp(fl_expected_, fl_actual_) != 0) { \
            FL_ASSERT_IMPL("Expected: \"%s\". Actual: \"%s\"",                       \
                           fl_expected_ ? fl_expected_ : "(null)",                   \
                           fl_actual_ ? fl_actual_ : "(null)");                      \
        }                                                                            \
    } while (0)

#define FL_ASSERT_STR_NEQ(V1, V2)                                                    \
    do {                                                                             \
        const char *fl_expected_ = (V1);                                             \
        const char *fl_actual_   = (V2);                                             \
        if (!fl_expected_ || !fl_actual_ || strcmp(fl_expected_, fl_actual_) == 0) { \
            FL_ASSERT_IMPL("Expected: different strings. Actual: \"%s\"",            \
                           fl_actual_ ? fl_actual_ : "(null)");                      \
        }                                                                            \
    } while (0)

#define FL_ASSERT_STR_CONTAINS(haystack, needle)                                        \
    do {                                                                                \
        const char *fl_haystack_ = (haystack);                                          \
        const char *fl_needle_   = (needle);                                            \
        if (!fl_haystack_ || !fl_needle_ || strstr(fl_haystack_, fl_needle_) == NULL) { \
            FL_ASSERT_IMPL("Expected: \"%s\" to contain \"%s\"",                        \
                           fl_haystack_ ? fl_haystack_ : "(null)",                      \
                           fl_needle_ ? fl_needle_ : "(null)");                         \
        }                                                                               \
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

#define FL_ASSERT_MULTIPLICATION_OVERFLOW(a, b, file, line)                              \
    do {                                                                                 \
        if ((a) > 0 && (b) > SIZE_MAX / (a)) {                                           \
            FL_THROW_DETAILS_FILE_LINE(fl_invalid_value,                                 \
                                       "multiplication overflow: %zu * %zu", file, line, \
                                       (size_t)(a), (size_t)(b));                        \
        }                                                                                \
    } while (0)

// To cover a type of your own, give it an FL_VALUE_TEXT association (C) and an
// fl_value_text overload (C++); everything above it works unchanged.
//
// The portability trap here is naming a typedef in a _Generic association. size_t,
// uint32_t, uint64_t, ptrdiff_t, intptr_t and uintptr_t are typedefs onto the basic
// types, so naming one alongside the type it is defined as is a duplicate
// association, and which pairs collide differs by architecture: on x64 size_t
// collides with unsigned long long, on x86 with unsigned int, and ptrdiff_t and
// intptr_t both collapse onto int there. Associating only the basic types avoids it
// with no per-platform #ifdef -- those typedefs still resolve to whichever basic
// type they are defined as.

#if defined(__cplusplus)
}
#endif

#endif // FL_EXCEPTION_ASSERT_H_
