/**
 * @file win32_intrinsics.c
 * @author Douglas Cuthbertson
 * @brief An implementation of what should be intrinsic functions for those
 * cases where the optimizer decides not to use the intrinsic version. The
 * drawback is that you have to disable whole-program optimization (/GL) and
 * link-time code generation (/LTCG). Why? Also note that the option "/LTCG"
 * was deprecated in VS2015 and is no longer available in VS2022.
 *
 * Per VS2022 /GL (Whole program optimization):
 * (https://learn.microsoft.com/en-us/cpp/build/reference/gl-whole-program-optimization?view=msvc-170)
 * "Whole program optimization is off by default and must be explicitly
 * enabled. However, it's also possible to explicitly disable it with /GL-."
 *
 * These definitions should allow using functions such as memset() without the
 * need to link to the C runtime library.
 *
 * FIXME: verify this with a simple program, because my mental model of whole-program
 * optimization (aka link-time code generation in VS2022 and later) is that the compiler
 * or linker generates the necessary code rather than the code for intrinsics being
 * linked to the program from the C runtime library. I am probably wrong, though, and the
 * optimized code lives in the C runtime library. Inquiring minds want to know.
 *
 * @version 0.1
 * @date 2023-12-03
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */

// disable warning C28251: Inconsistent annotation for 'memcpy': this instance has no
// annotations.
#include "intrinsics_win32.h" // memcpy, memset

#include <faultline/fl_macros.h>

#include <stddef.h> // size_t

// force the use of the version of the functions below rather than using the
// intrinsic, compiler-generated version of memset.

#if !defined(__clang__) && !defined(__GNUC__)
#pragma function(memcpy)
#endif

void *CDECL memcpy(void *dst, void const *src, size_t n) {
    char       *out = dst;
    char const *in  = src;

    while (n-- > 0) {
        *out = *in;
    }

    return dst;
}

#if !defined(__clang__) && !defined(__GNUC__)
#pragma function(memset)
#endif

void *CDECL memset(void *dst, int value, size_t n) {
    unsigned char *p = dst;
    while (n-- > 0) {
        *p++ = (unsigned char)value;
    }

    return dst;
}
