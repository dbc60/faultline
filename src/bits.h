#ifndef FAULTLINE_BITS_H_
#define FAULTLINE_BITS_H_

/**
 * @file bits.h
 * @author Douglas Cuthbertson
 * @brief Do you like to twiddle your bits? Macros for bit twiddling are here.
 * @version 0.1
 * @date 2024-11-04
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // u64

#include <limits.h> // CHAR_BIT

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Given a non-negative value V and a power-of-2 P2, ALIGN_UP(V, P2) calculates
 * the smallest multiple of P2 >= V.
 *
 * For example, ALIGN_UP(x, 16) rounds up x to the smallest multiple of 16 greater than
 * or equal to x. If x is 47, the result is 48. If x is 48, the result is 48. If x is 49,
 * the result is 64.
 */
#define ALIGN_UP(V, P2) (((V) + ((P2) - 1)) & ~((u64)(P2) - 1))

/**
 * @brief Given a non-negative value V and a power-of-2 P2, ALIGN_DOWN(V, P2) calculates
 * the largest multiple of P2 <= V.
 *
 * For example, ALIGN_DOWN(x, 16) rounds down x to the largest multiple of 16 that is
 * less than or equal to x. If x is 49, the result is 48. If x is 48, the result is 48.
 * If x is 47, the result is 32.
 */
#define ALIGN_DOWN(V, P2) ((V) & ~((u64)(P2) - 1))

/**
 * @brief unsigned negation
 *
 * -(X) on an unsigned X causes the Visual Studio compiler to emit a warning that
 * applying unary minus to an unsigned type still results in an unsigned type. It's as if
 * the compiler writers didn't enjoy bit twiddling. Instead of suppressing the warning,
 * use this macro which defines the equivalent expression "~(X)+1".
 */
#define UNSIGNED_NEGATION(X) (~(X) + 1)

/// isolate the least-significant bit set in X.
#define BIT_LSB(X) ((X) & UNSIGNED_NEGATION(X))

/// mask of all bits left of the least-significant bit set in X.
#define BIT_MASK_LEFT(X) (((X) << 1) | UNSIGNED_NEGATION((X) << 1))

/// mask of the least-significant bit set in X and all bits to its left.
#define BIT_MASK_LEFT_LSB(X) ((X) | UNSIGNED_NEGATION(X))

/**
 * @brief BITS_LEFT_LSB(X, K, J) returns the set of J bits starting K bits to the left of
 * the least-significant bit (LSB) of X.
 *
 * For example, if J = 2 and K = 8, it extracts 2 bits that start 8 bits to the left of
 * the LSB of X by first shifting X to the right 8 bits and then masking the 2 least-
 * significant bits of that result.
 *
 * Note that the desired bits will be the J least-significant bits of the result.
 */
#define BITS_LEFT_LSB(X, K, J) (((X) >> (K)) & ~(~0ull << (J)))

/**
 * @brief BITS_RIGHT_MSB(X, K, J) returns the set of J bits starting K bits to the right
 * of the most-significant bit (MSB) of X.
 *
 * For example, if J = 2 and K = 8, it extracts 2 bits that start 8 bits to the right of
 * the MSB of X by first shifting X to the left 8 bits and then masking the 2 most-
 * significant bits of that result.
 *
 * Note that the desired bits will be the J most-significant bits of the result.
 */
#define BITS_RIGHT_MSB(X, K, J) (((X) << (K)) & ~(~0ull >> (J)))

/**
 * @brief BITS_RIGHT_MSB_LEAST(X, K, J) returns the set of J bits which appear K bits
 * to the right of the most significant bit (MSB) of X and shifted down to the
 * least-significant bits of the result.
 *
 * This macro is similar to BITS_RIGHT_MSB, except that the desired bits are the J least-
 * significant bits of the result instead of the J most-significant bits.
 *
 * For example, if J = 2 and K = 8, it extracts 2 bits starting from 8 bits to the right
 * of the MSB of X by first shifting X to the left 8 bits, then masking the 2 most-
 * significant bits of that value, and finally right-shifting the bits to the J least-
 * significant bits of the result.
 *
 * This macro is particularly useful for traversing a digital search tree where the
 * pointers to the subtrees are in a two-element array, and where J=1. It enables the
 * traversal code to choose the left or right subtree based on some value X and a
 * "decision bit" K, as in, "next = child[BITS_RIGHT_MSB_LEAST(X, K, 1)];".
 */
#define BITS_RIGHT_MSB_LEAST(X, K, J) (BITS_RIGHT_MSB(X, K, J) >> (U64_BIT - (J)))

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_BITS_H_
