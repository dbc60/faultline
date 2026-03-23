/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 *
 * See fnv-private.h for terms of use and redistribution.
 */

#ifndef _FNVconfig_H_
#define _FNVconfig_H_

/*  Description:
 *      This file provides configuration ifdefs for the
 *      FNV-1a non-cryptographic hash algorithms. */

/*      FNV_64bitIntegers - Define this if your system supports
 *          64-bit arithmetic including 32-bit x 32-bit
 *          multiplication producing a 64-bit product. If
 *          undefined, it will be assumed that 32-bit arithmetic
 *          is supported including 16-bit x 16-bit multiplication
 *          producing a 32-bit result.
 */

#include <stdint.h>

/* Check if 64-bit integers are supported in the target */

#ifdef UINT64_MAX
#define FNV_64bitIntegers
#else
#undef FNV_64bitIntegers
#endif

/*      The following allows overriding the
 *      above configuration setting.
 */

#ifdef FNV_TEST_PROGRAM
#ifdef TEST_FNV_64bitIntegers
#ifndef FNV_64bitIntegers
#define FNV_64bitIntegers
#endif
#else
#undef FNV_64bitIntegers
#endif
#ifndef FNV_64bitIntegers /* causes an error if uint64_t is used */
#undef uint64_t
#define uint64_t no_64_bit_integers
#endif
#endif

#endif /* _FNVconfig_H_ */
