#ifndef FAULTLINE_MATH_LINUX_H_
#define FAULTLINE_MATH_LINUX_H_
/**
 * @file math_linux.h
 * @author Douglas Cuthbertson
 * @brief FaultLine Math Library - Linux/GCC/Clang x86 implementation
 * @version 0.2.0
 * @date 2024-11-17
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_abbreviated_types.h> // u32, u64

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Greatest power of 2 less than or equal to the given value
 *
 */
#define MATH_GREATEST_LOG2_BIT32(VAL, P2)                   \
    do {                                                    \
        u32 v = (u32)(VAL);                                 \
        (P2)  = (u32)(31 - __builtin_clz((unsigned int)v)); \
    } while (0)

#define MATH_GREATEST_LOG2_BIT64(VAL, P2)                           \
    do {                                                            \
        u64 v = (VAL);                                              \
        (P2)  = (u32)(63 - __builtin_clzll((unsigned long long)v)); \
    } while (0)

#if defined(__cplusplus)
}
#endif

#endif // FAULTLINE_MATH_LINUX_H_
