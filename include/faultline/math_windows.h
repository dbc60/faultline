#ifndef FAULTLINE_MATH_WINDOWS_H_
#define FAULTLINE_MATH_WINDOWS_H_
/**
 * @file math_windows.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Math Library - Windows-specific implementation
 * @version 0.2.0
 * @date 2024-11-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // u16, u32, u64

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // _BitScanReverse

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Greatest power of 2 less than or equal to the given value
 *
 */
#define MATH_GREATEST_LOG2_BIT32(VAL, P2) \
    do {                                  \
        u32   v = (u32)(VAL);             \
        DWORD x;                          \
        _BitScanReverse(&x, v);           \
        (P2) = x;                         \
    } while (0)

#define MATH_GREATEST_LOG2_BIT64(VAL, P2) \
    do {                                  \
        u64   v = (VAL);                  \
        DWORD x;                          \
        _BitScanReverse64(&x, v);         \
        (P2) = x;                         \
    } while (0)

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_MATH_WINDOWS_H_
