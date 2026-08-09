#ifndef FAULTLINE_BITSCAN_WINDOWS_H_
#define FAULTLINE_BITSCAN_WINDOWS_H_

/**
 * @file bitscan_windows.h
 * @author Douglas Cuthbertson
 * @brief 64-bit bit-scan primitives for the Windows toolchains.
 * @version 0.2.0
 * @date 2026-08-09
 *
 * A 32-bit target provides only the 32-bit _BitScanForward and _BitScanReverse
 * intrinsics; the _BitScanForward64 and _BitScanReverse64 forms exist solely on
 * 64-bit targets. These wrappers present one spelling to callers, using the
 * native 64-bit intrinsic where it exists and composing two 32-bit scans where it
 * does not.
 *
 * GCC and clang take their own builtins rather than the MSVC-compatible
 * intrinsics. Those builtins are width-independent, and reaching for the
 * intrinsics instead would mean pulling in intrin.h, which on MinGW carries
 * deprecated 3DNow! headers that a -Werror build rejects.
 *
 * Both take the index by u32 pointer rather than the intrinsics' unsigned long
 * pointer, so callers hold a u32 and need no cast.
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // u08, u32, u64

#if defined(_MSC_VER)
#include <intrin.h> // _BitScanForward, _BitScanReverse, and the 64-bit forms
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Find the least-significant set bit in a 64-bit mask.
 *
 * @param index set to the zero-based position of the first set bit found. Left
 * untouched when the mask is zero.
 * @param mask the value to scan.
 * @return u08 nonzero when a set bit was found, zero when the mask is zero.
 */
static inline u08 fl_bit_scan_forward64(u32 *index, u64 mask) {
    u08 found = 0;

    if (mask != 0) {
#if !defined(_MSC_VER)
        *index = (u32)__builtin_ctzll(mask);
        found  = 1;
#elif defined(_M_IX86)
        unsigned long i = 0;
        // Scan the low half first: the least-significant set bit lives there when
        // the low half is non-zero.
        if (_BitScanForward(&i, (unsigned long)(mask & 0xFFFFFFFFull))) {
            *index = (u32)i;
        } else {
            _BitScanForward(&i, (unsigned long)(mask >> 32));
            *index = (u32)i + 32;
        }
        found = 1;
#else
        unsigned long i = 0;
        _BitScanForward64(&i, mask);
        *index = (u32)i;
        found  = 1;
#endif
    }

    return found;
}

/**
 * @brief Find the most-significant set bit in a 64-bit mask.
 *
 * @param index set to the zero-based position of the first set bit found. Left
 * untouched when the mask is zero.
 * @param mask the value to scan.
 * @return u08 nonzero when a set bit was found, zero when the mask is zero.
 */
static inline u08 fl_bit_scan_reverse64(u32 *index, u64 mask) {
    u08 found = 0;

    if (mask != 0) {
#if !defined(_MSC_VER)
        *index = (u32)(63 - __builtin_clzll(mask));
        found  = 1;
#elif defined(_M_IX86)
        unsigned long i = 0;
        // Scan the high half first: the most-significant set bit lives there when
        // the high half is non-zero.
        if (_BitScanReverse(&i, (unsigned long)(mask >> 32))) {
            *index = (u32)i + 32;
        } else {
            _BitScanReverse(&i, (unsigned long)(mask & 0xFFFFFFFFull));
            *index = (u32)i;
        }
        found = 1;
#else
        unsigned long i = 0;
        _BitScanReverse64(&i, mask);
        *index = (u32)i;
        found  = 1;
#endif
    }

    return found;
}

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_BITSCAN_WINDOWS_H_
