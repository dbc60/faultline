#ifndef INDEX_LINUX_H_
#define INDEX_LINUX_H_

/**
 * @file index_linux.h
 * @author Douglas Cuthbertson
 * @brief Linux/GCC implementations of bit-scan macros, index-by-value macros/function,
 * and bin limit macros. Uses __builtin_ctz/__builtin_clz and their 64-bit variants.
 * This is a drop-in replacement for index_windows.h.
 * @version 0.2
 * @date 2023-03-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // u32, u64, flag64
#include <faultline/size.h>                 // TWO_SIZE_T_SIZES

#define INDEX_BIN_ALIGNMENT TWO_SIZE_T_SIZES

/**
 * @brief return the zero-based index of the least-significant set bit (1 bit) in X.
 *
 * If X is zero or the least-significant bit in X is zero, return zero. Otherwise, return
 * the zero-based index of the least-significant set bit (1 bit) in X.
 *
 * @param X is a 32-bit value.
 * @param I set to the zero-based index of the least-significant set bit (1 bit) in X.
 */
#define COMPUTE_LSB2IDX32(X, I)                              \
    do {                                                     \
        u32 J = (X);                                         \
        I     = J ? (u32)__builtin_ctz((unsigned int)J) : 0; \
    } while (0)

/**
 * @brief return the zero-based index of the least-significant set bit (1 bit) in X.
 *
 * If X is zero or the least-significant bit in X is zero, return zero. Otherwise, return
 * the zero-based index of the least-significant set bit (1 bit) in X.
 *
 * @param X is a 64-bit value.
 * @param I set to the zero-based index of the least-significant set bit (1 bit) in X.
 */
#define COMPUTE_LSB2IDX64(X, I)                                      \
    do {                                                             \
        u64 J = (X);                                                 \
        I     = J ? (u32)__builtin_ctzll((unsigned long long)J) : 0; \
    } while (0)

/**
 * @brief return the zero-based index of the most-significant set bit (1 bit) in X.
 *
 * If X is zero or the most-significant bit in X is zero, return zero. Otherwise, return
 * the zero-based index of the most-significant set bit (1 bit) in X.
 *
 * @param X is a 32-bit value.
 * @param I set to the zero-based index of the most-significant set bit (1 bit) in X.
 */
#define COMPUTE_MSB2IDX32(X, I)                                     \
    do {                                                            \
        u32 J = (X);                                                \
        I     = J ? (u32)(31 - __builtin_clz((unsigned int)J)) : 0; \
    } while (0)

/**
 * @brief return the zero-based index of the most-significant set bit (1 bit) in X.
 *
 * If X is zero or the most-significant bit in X is zero, return zero. Otherwise, return
 * the zero-based index of the most-significant set bit (1 bit) in X.
 *
 * @param X is a 64-bit value.
 * @param I set to the zero-based index of the most-significant set bit (1 bit) in X.
 */
#define COMPUTE_MSB2IDX64(X, I)                                             \
    do {                                                                    \
        u64 J = (X);                                                        \
        I     = J ? (u32)(63 - __builtin_clzll((unsigned long long)J)) : 0; \
    } while (0)

/**
 * @brief Map 32-bit values to indices.
 *
 * Compute an index from a value given the number of indices over which values should be
 * distributed and the power of 2 for the smallest value that should have an index of
 * zero.
 *
 * @param VAL the value to be indexed.
 * @param CNT the number of indices. This must be a power of 2, such as 32 or 64.
 * @param P2 the power-of-2 (exponent) of the smallest value to be mapped to index 0.
 * @return IDX the zero-based index for the value.
 */
#define INDEX_BY_VALUE32(VAL, CNT, P2, IDX)                              \
    do {                                                                 \
        u32 X     = (VAL) >> (P2);                                       \
        u32 MAX_X = (1ul << ((CNT) - 1)) - 1;                            \
        if (X == 0) {                                                    \
            IDX = 0;                                                     \
        } else if (X > MAX_X) {                                          \
            IDX = (CNT) - 1;                                             \
        } else {                                                         \
            u32 K = (u32)(31 - __builtin_clz((unsigned int)X));          \
            IDX   = (u32)((K << 1) + (((VAL) >> (K + ((P2) - 1)) & 1))); \
        }                                                                \
    } while (0)

/**
 * @brief Map 64-bit values to indices.
 *
 * Compute an index from a value given the number of indices over which values should be
 * distributed and the power of 2 for the smallest value that should have an index of
 * zero.
 *
 * @param VAL the value to be indexed.
 * @param CNT the number of indices. This must be a power of 2, such as 32 or 64.
 * @param P2 the power-of-2 (exponent) of the smallest value to be mapped to index 0.
 * @return IDX the zero-based index for the value.
 */
#define INDEX_BY_VALUE64(VAL, CNT, P2, IDX)                              \
    do {                                                                 \
        flag64 X     = (VAL) >> (P2);                                    \
        flag64 MAX_X = (1ull << ((CNT) - 1)) - 1;                        \
        if (X == 0) {                                                    \
            IDX = 0;                                                     \
        } else if (X > MAX_X) {                                          \
            IDX = (CNT) - 1;                                             \
        } else {                                                         \
            u32 K = (u32)(63 - __builtin_clzll((unsigned long long)X));  \
            IDX   = (u32)((K << 1) + (((VAL) >> (K + ((P2) - 1)) & 1))); \
        }                                                                \
    } while (0)

/**
 * @brief calculate the bin index for a value.
 *
 * See index_windows.h for detailed documentation and worked examples.
 * This implementation uses GCC/Clang __builtin_clzll for the MSB computation.
 *
 * @param val the value to be indexed
 * @param cnt the number of indices
 * @param exp the power-of-2 (exponent) of the smallest value to be mapped to index 0
 * @return u32 the zero-based index for the value
 */
static inline u32 index_by_value(flag64 val, u32 cnt, u32 exp) {
    u32    idx   = 0;
    flag64 x     = val >> exp;
    flag64 max_x = (1ull << (cnt - 1)) - 1;
    if (x > max_x) {
        idx = cnt - 1;
    } else if (x != 0) {
        u32 k = (u32)(63 - __builtin_clzll((unsigned long long)x));
        idx   = (u32)((k << 1) + ((val >> (k + (exp - 1))) & 1));
    }

    return idx;
}

/**
 * @brief INDEX_BIN_TO_LOWER_LIMIT(IDX, P2) returns the lower limit of a bin given the
 * bin's index (IDX) and power-of-two exponent for the lower limit of the first bin.
 *
 * The smallest chunk is (1ull << (P2)), and each bin has a range that is half that
 * value. The first bin's lower limit is therefore (1ull << (P2)), and the next bin's
 * lower limit is just ((1ull << (P2)) + (1ull << (P2) - 1)).
 *
 * @param IDX the index of a bin
 * @param P2 the power-of-two exponent for the lower limit of the first bin.
 * @return the lower limit of the bin in bytes.
 */
#define INDEX_BIN_TO_LOWER_LIMIT(IDX, P2) \
    ((1ull << ((P2) + ((IDX) >> 1)))      \
     + (((IDX) % 2) ? 1ull << ((P2) - 1 + ((IDX) >> 1)) : 0))

/**
 * @brief INDEX_BIN_TO_UPPER_LIMIT(IDX, P2) returns the upper limit of a bin given the
 * bin's index (IDX) and power-of-two exponent for the lower limit of the first bin.
 *
 * The upper limit is just the lower limit plus the range for the bin less the alignment
 * requirement for each chunk (normally 16-bytes).
 *
 * @param IDX the index of a bin
 * @param P2 the power-of-two exponent for the lower limit of the first bin.
 * @return the upper limit of the bin in bytes.
 */
#define INDEX_BIN_TO_UPPER_LIMIT(IDX, P2)                                    \
    (INDEX_BIN_TO_LOWER_LIMIT(IDX, P2) + (1ull << ((P2) - 1 + ((IDX) >> 1))) \
     - INDEX_BIN_ALIGNMENT)

#endif // INDEX_LINUX_H_
