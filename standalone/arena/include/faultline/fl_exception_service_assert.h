#ifndef FL_EXCEPTION_SERVICE_ASSERT_H_
#define FL_EXCEPTION_SERVICE_ASSERT_H_

/**
 * @file fl_exception_service_assert.h
 * @author Douglas Cuthbertson
 * @brief Standalone override — all FL_ASSERT_* macros map to assert() (debug-only).
 *
 * FL_ASSERT_SIZE_RANGE and FL_ASSERT_MULTIPLICATION_OVERFLOW are also mapped to
 * assert() rather than throwing exceptions, consistent with the "programmer errors
 * abort in debug, compile out in release" contract.
 * @version 0.1
 * @date 2026-05-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 *
 */
#include <assert.h>                         // assert
#include <stddef.h>                         // size_t, SIZE_MAX
#include <faultline/fl_exception_service.h> // extern reason declarations

#if defined(__cplusplus)
extern "C" {
#endif

/* fl_unexpected_failure is declared for source compatibility; not used at runtime. */
extern FLExceptionReason fl_unexpected_failure;

#define FL_ASSERT(e)                                     assert(e)
#define FL_ASSERT_FILE_LINE(e, F, L)                     (assert(e), (void)(F), (void)(L))
#define FL_ASSERT_DETAILS(e, d, ...)                     assert(e)
#define FL_ASSERT_DETAILS_FILE_LINE(e, d, F, L, ...)     (assert(e), (void)(F), (void)(L))

#define FL_ASSERT_NOT_NULL(p) assert((p) != NULL)
#define FL_ASSERT_NULL(p)     assert((p) == NULL)
#define FL_ASSERT_TRUE(expr)  assert(expr)
#define FL_ASSERT_FALSE(expr) assert(!(expr))

#define FL_ASSERT_ZERO_SIZE_T(v)  assert((v) == 0)
#define FL_ASSERT_GE_SIZE_T(a, b) assert((a) >= (b))

#define FL_ASSERT_BINOP_TYPED(lhs, rhs, op, op_str, fmt) assert((lhs)op(rhs))

#define FL_ASSERT_EQ_CHAR(V1, V2)  assert((V1) == (V2))
#define FL_ASSERT_NEQ_CHAR(V1, V2) assert((V1) != (V2))
#define FL_ASSERT_LT_CHAR(V1, V2)  assert((V1) < (V2))
#define FL_ASSERT_LE_CHAR(V1, V2)  assert((V1) <= (V2))
#define FL_ASSERT_GT_CHAR(V1, V2)  assert((V1) > (V2))
#define FL_ASSERT_GE_CHAR(V1, V2)  assert((V1) >= (V2))

#define FL_ASSERT_EQ_INT(V1, V2)  assert((V1) == (V2))
#define FL_ASSERT_NEQ_INT(V1, V2) assert((V1) != (V2))
#define FL_ASSERT_LT_INT(V1, V2)  assert((V1) < (V2))
#define FL_ASSERT_LE_INT(V1, V2)  assert((V1) <= (V2))
#define FL_ASSERT_GT_INT(V1, V2)  assert((V1) > (V2))
#define FL_ASSERT_GE_INT(V1, V2)  assert((V1) >= (V2))

#define FL_ASSERT_EQ_SIZE_T(V1, V2)  assert((V1) == (V2))
#define FL_ASSERT_NEQ_SIZE_T(V1, V2) assert((V1) != (V2))
#define FL_ASSERT_LT_SIZE_T(V1, V2)  assert((V1) < (V2))
#define FL_ASSERT_LE_SIZE_T(V1, V2)  assert((V1) <= (V2))
#define FL_ASSERT_GT_SIZE_T(V1, V2)  assert((V1) > (V2))
/* FL_ASSERT_GE_SIZE_T already defined above as the two-argument form */

#define FL_ASSERT_EQ_UINT(V1, V2)  assert((V1) == (V2))
#define FL_ASSERT_NEQ_UINT(V1, V2) assert((V1) != (V2))
#define FL_ASSERT_LT_UINT(V1, V2)  assert((V1) < (V2))
#define FL_ASSERT_LE_UINT(V1, V2)  assert((V1) <= (V2))
#define FL_ASSERT_GT_UINT(V1, V2)  assert((V1) > (V2))
#define FL_ASSERT_GE_UINT(V1, V2)  assert((V1) >= (V2))

#define FL_ASSERT_EQ_UINT32(V1, V2)  assert((V1) == (V2))
#define FL_ASSERT_NEQ_UINT32(V1, V2) assert((V1) != (V2))
#define FL_ASSERT_LT_UINT32(V1, V2)  assert((V1) < (V2))
#define FL_ASSERT_LE_UINT32(V1, V2)  assert((V1) <= (V2))
#define FL_ASSERT_GT_UINT32(V1, V2)  assert((V1) > (V2))
#define FL_ASSERT_GE_UINT32(V1, V2)  assert((V1) >= (V2))

#define FL_ASSERT_EQ_UINT64(V1, V2)  assert((V1) == (V2))
#define FL_ASSERT_NEQ_UINT64(V1, V2) assert((V1) != (V2))
#define FL_ASSERT_LT_UINT64(V1, V2)  assert((V1) < (V2))
#define FL_ASSERT_LE_UINT64(V1, V2)  assert((V1) <= (V2))
#define FL_ASSERT_GT_UINT64(V1, V2)  assert((V1) > (V2))
#define FL_ASSERT_GE_UINT64(V1, V2)  assert((V1) >= (V2))

#define FL_ASSERT_EQ_PTR(V1, V2)  assert((V1) == (V2))
#define FL_ASSERT_NEQ_PTR(V1, V2) assert((V1) != (V2))

#define FL_ASSERT_SIZE_RANGE(sz, lo, hi, f, l) \
    assert((size_t)(sz) >= (size_t)(lo) && (size_t)(sz) <= (size_t)(hi))

#define FL_ASSERT_MULTIPLICATION_OVERFLOW(a, b, f, l) \
    assert(!((size_t)(a) > 0 && (size_t)(b) > SIZE_MAX / (size_t)(a)))

#if defined(__cplusplus)
}
#endif

#endif // FL_EXCEPTION_SERVICE_ASSERT_H_
