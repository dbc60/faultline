#ifndef INDEX_WINDOWS_H_
#define INDEX_WINDOWS_H_

/**
 * @file index_windows.h
 * @author Douglas Cuthbertson
 * @brief This is the Windows version of COMPUTE_LARGE_INDEX and COMPUTE_LSB2IDX that use
 * _BitScanReverse64 and _BitScanForward to quickly find the index of a non-empty bin in
 * the large_map field of an instance of Arena.
 * @version 0.2
 * @date 2023-03-05
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // u32, flag64
#include <faultline/size.h>                 // TWO_SIZE_T_SIZES

#ifdef _MSC_VER
#include <intrin.h> // _BitScanForward, _BitScanReverse
#else
#include <x86intrin.h> // _BitScanForward, _BitScanReverse (clang/GCC)
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // _BitScanForward, _BitScanReverse

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
#define COMPUTE_LSB2IDX32(X, I)                    \
    do {                                           \
        unsigned long J;                           \
        u08           ok = _BitScanForward(&J, X); \
        I                = ok ? (u32)J : 0;        \
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
#define COMPUTE_LSB2IDX64(X, I)                      \
    do {                                             \
        unsigned long J;                             \
        u08           ok = _BitScanForward64(&J, X); \
        I                = ok ? (u32)J : 0;          \
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
#define COMPUTE_MSB2IDX32(X, I)                    \
    do {                                           \
        unsigned long J;                           \
        u08           ok = _BitScanReverse(&J, X); \
        I                = ok ? (u32)J : 0;        \
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
#define COMPUTE_MSB2IDX64(X, I)                      \
    do {                                             \
        unsigned long J;                             \
        u08           ok = _BitScanReverse64(&J, X); \
        I                = ok ? (u32)J : 0;          \
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
#define INDEX_BY_VALUE32(VAL, CNT, P2, IDX)                                \
    do {                                                                   \
        u32 X     = (VAL) >> (P2);                                         \
        u32 MAX_X = (1ul << ((CNT) - 1)) - 1;                              \
        if (X == 0) {                                                      \
            IDX = 0;                                                       \
        } else if (X > MAX_X) {                                            \
            IDX = (CNT) - 1;                                               \
        } else {                                                           \
            u32 K;                                                         \
            u08 ok = _BitScanReverse((DWORD *)&K, X);                      \
            if (ok) {                                                      \
                IDX = (u32)((K << 1) + (((VAL) >> (K + ((P2) - 1)) & 1))); \
            } else {                                                       \
                IDX = 0;                                                   \
            }                                                              \
        }                                                                  \
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
#define INDEX_BY_VALUE64(VAL, CNT, P2, IDX)                                \
    do {                                                                   \
        flag64 X     = (VAL) >> (P2);                                      \
        flag64 MAX_X = (1ull << ((CNT) - 1)) - 1;                          \
        if (X > MAX_X) {                                                   \
            IDX = (CNT) - 1;                                               \
        } else {                                                           \
            u32 K;                                                         \
            u08 ok = _BitScanReverse64((DWORD *)&K, X);                    \
            if (ok) {                                                      \
                IDX = (u32)((K << 1) + (((VAL) >> (K + ((P2) - 1)) & 1))); \
            } else {                                                       \
                IDX = 0;                                                   \
            }                                                              \
        }                                                                  \
    } while (0)

/**
 * @brief calculate the bin index for a value.
 *
 * Compute an index from a value given the number of indices over which values should be
 * distributed and the power-of-2 (exponent) of the smallest value that should have an
 * index of zero.
 *
 * Shifting val by exp eliminates the lower bits of val that don't differentiate values
 * common to a bin.
 *
 * The first bin's lower limit is (1ull << (exp)) and the lower limit of each subsequent
 * bin is twice the previous bin's lower limit. For example, if exp is 10, the first
 * bin's lower limit is 1024, the next bin's lower limit is 2048, followed by 4096, and
 * so on.
 *
 * The _BitScanReverse64(u32 *index, u64 mask) function scans a 64-bit mask from the most
 * significant bit to the least significant bit and sets the value pointed to by index to
 * the zero-based position of the first set bit found. It returns a nonzero value if a
 * set bit is found, or zero if mask is zero.
 *
 * Example:
 *  flag64 val = 1024; // 0100 0000 0000
 *  u32 cnt = 48;
 *  u32 exp = 10; // the smallest value to be mapped to index 0 is 1024
 *    x = val >> exp; // x = 1
 *    k = 0; // the most significant bit of x is at index 0
 *    idx = (k << 1) + ((val >> (k + (exp - 1))) & 1); // idx = (0 << 1) + ((1024 >> (0 +
 * (10 - 1))) & 1) = 0
 *
 *  flag64 val = 1536; // 1,536 = 0000 0110 0000 0000
 *  u32 cnt = 48;
 *  u32 exp = 10; // the smallest value to be mapped to index 0 is 1024
 *    x = val >> exp; // x = 1
 *    k = 0; // the most significant bit of x is at index 0
 *    idx = (k << 1) + ((val >> (k + (exp - 1))) & 1); // idx = (0 << 1) + ((1536 >> (0 +
 * (10 - 1))) & 1) = 1
 *
 *  flag64 val = 3000; // 3000 = 1011 1011 1000
 *  u32 cnt = 48;
 *  u32 exp = 10; // the smallest value to be mapped to index 0 is 1024
 *    x = val >> exp; // x = 2
 *    k = 1; // the most significant bit of x is at index 1
 *    idx = (k << 1) + ((val >> (k + (exp - 1))) & 1); // idx = (1 << 1) + ((3000 >> (1 +
 * (10 - 1))) & 1) = 2
 *
 *  flag64 val = 65536; // 65,536 = 00001 0000 0000 0000 0000
 *  u32 cnt = 48;
 *  u32 exp = 10; // the smallest value to be mapped to index 0 is 1024
 *    x = val >> exp; // x = 65536 >> 10 = 64 = 0100 0000
 *    k = 6; // the most significant bit of x is at index 6
 *    idx = (k << 1) + ((val >> (k + (exp - 1))) & 1); // idx = (6 << 1) + ((65536 >> (6
 * + (10 - 1))) & 1) = 12
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
        // return the last index
        idx = cnt - 1;
    } else {
        u32 k;
        // find k, the largest power of 2 that is not greater than log2(x)
        u08 ok = _BitScanReverse64((DWORD *)&k, x);
        if (ok) {
            // "k << 1" ensures that the index is even, and the second term ensures that
            // the index is odd if the value is odd. "k << 1" also ensures that each
            // even/odd pair of bins covers an entire power-of-2 range. The second term
            // is the least significant bit of the value shifted to the right by (k +
            // (exp - 1)) and determines which of the two bins covering a power-of-2
            // range is selected.
            idx = (u32)((k << 1) + ((val >> (k + (exp - 1))) & 1));
        }
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

#endif // INDEX_WINDOWS_H_
