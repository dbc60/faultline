/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

#ifndef _FNV128_H_
#define _FNV128_H_

/*
 *  Description:
 *      This file provides headers for the 128-bit version of
 *      the FNV-1a non-cryptographic hash algorithm.
 */

#include "FNVconfig.h"
#include "FNVErrorCodes.h"

#include <stdint.h>
#define FNV128size (128 / 8)

/* If you do not have the ISO standard stdint.h header file, then
 * you must typedef the following types:
 *
 *    type              meaning
 *  uint64_t    unsigned 64-bit integer (ifdef FNV_64bitIntegers)
 *  uint32_t    unsigned 32-bit integer
 *  uint16_t    unsigned 16-bit integer
 *  uint8_t     unsigned 8-bit integer (i.e., unsigned char)
 */

/*
 *  This structure holds context information for an FNV128 hash
 */
#ifdef FNV_64bitIntegers
/* version if 64-bit integers supported */
typedef struct FNV128context_s {
    int      Computed; /* state */
    uint32_t Hash[FNV128size / 4];
} FNV128context;

#else
/* version if 64-bit integers NOT supported */
typedef struct FNV128context_s {
    int      Computed; /* state */
    uint16_t Hash[FNV128size / 2];
} FNV128context;

#endif /* FNV_64bitIntegers */

/*  Function Prototypes:
 *
 *    FNV128string: hash a zero-terminated string not including
 *                  the terminating zero
 *    FNV128stringBasis: also takes an offset_basis parameter
 *
 *    FNV128block: hash a specified length byte vector
 *    FNV128blockBasis: also takes an offset_basis parameter
 *
 *    FNV128file: hash the contents of a file
 *    FNV128fileBasis: also takes an offset_basis parameter
 *
 *    FNV128init: initializes an FNV128 context
 *    FNV128initBasis: initializes an FNV128 context with a
 *                     provided 16-byte vector basis
 *    FNV128blockin: hash in a specified length byte vector
 *    FNV128stringin: hash in a zero-terminated string not
 *                    including the terminating zero
 *    FNV128filein: hash in the contents of a file
 *    FNV128result: returns the hash value
 *
 *    Hash is returned as an array of 8-bit unsigned integers
 */

#ifdef __cplusplus
extern "C" {
#endif

/* FNV128 */
extern int FNV128string(char const *in, uint8_t out[FNV128size]);
extern int FNV128stringBasis(char const *in, uint8_t out[FNV128size], uint8_t const basis[FNV128size]);
extern int FNV128block(void const *vin, long int length, uint8_t out[FNV128size]);
extern int FNV128blockBasis(void const *vin, long int length, uint8_t out[FNV128size], uint8_t const basis[FNV128size]);
extern int FNV128file(char const *fname, uint8_t out[FNV128size]);
extern int FNV128fileBasis(char const *fname, uint8_t out[FNV128size], uint8_t const basis[FNV128size]);
extern int FNV128init(FNV128context *const);
extern int FNV128initBasis(FNV128context *const, uint8_t const basis[FNV128size]);
extern int FNV128blockin(FNV128context *const, void const *vin, long int length);
extern int FNV128stringin(FNV128context *const, char const *in);
extern int FNV128filein(FNV128context *const, char const *fname);
extern int FNV128result(FNV128context *const, uint8_t out[FNV128size]);

#ifdef __cplusplus
}
#endif

#endif /* _FNV128_H_ */
