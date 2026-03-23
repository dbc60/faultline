/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

#ifndef _FNV1024_H_
#define _FNV1024_H_

/*
 *  Description:
 *      This file provides headers for the 1024-bit version of
 *      the FNV-1a non-cryptographic hash algorithm.
 */

#include "FNVconfig.h"
#include "FNVErrorCodes.h"

#include <stdint.h>
#define FNV1024size (1024 / 8)

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
 *  This structure holds context information for an FNV1024 hash
 */
#ifdef FNV_64bitIntegers
/* version if 64-bit integers supported */
typedef struct FNV1024context_s {
    int      Computed; /* state */
    uint32_t Hash[FNV1024size / 4];
} FNV1024context;

#else
/* version if 64-bit integers NOT supported */
typedef struct FNV1024context_s {
    int      Computed; /* state */
    uint16_t Hash[FNV1024size / 2];
} FNV1024context;

#endif /* FNV_64bitIntegers */

/* Function Prototypes:
 *
 *    FNV1024string: hash a zero-terminated string not including
 *                   the terminating zero
 *    FNV1024stringBasis: also takes an offset_basis parameter
 *
 *    FNV1024block: hash a specified length byte vector
 *    FNV1024blockBasis: also takes an offset_basis parameter
 *
 *    FNV1024file: hash the contents of a file
 *    FNV1024fileBasis: also takes an offset_basis parameter
 *
 *    FNV1024init: initializes an FNV1024 context
 *    FNV1024initBasis: initializes an FNV1024 context with a
 *                      provided 128-byte vector basis
 *    FNV1024blockin: hash in a specified length byte vector
 *    FNV1024stringin: hash in a zero-terminated string not
 *                     including the terminating zero
 *    FNV1024filein: hash in the contents of a file
 *    FNV1024result: returns the hash value
 *
 *    Hash is returned as an array of 8-bit unsigned integers
 */

#ifdef __cplusplus
extern "C" {
#endif

/* FNV1024 */
extern int FNV1024string(char const *in, uint8_t out[FNV1024size]);
extern int FNV1024stringBasis(char const *in, uint8_t out[FNV1024size], uint8_t const basis[FNV1024size]);
extern int FNV1024block(void const *vin, long int length, uint8_t out[FNV1024size]);
extern int FNV1024blockBasis(void const *vin, long int length, uint8_t out[FNV1024size], uint8_t const basis[FNV1024size]);
extern int FNV1024file(char const *fname, uint8_t out[FNV1024size]);
extern int FNV1024fileBasis(char const *fname, uint8_t out[FNV1024size], uint8_t const basis[FNV1024size]);
extern int FNV1024init(FNV1024context *const);
extern int FNV1024initBasis(FNV1024context *const, uint8_t const basis[FNV1024size]);
extern int FNV1024blockin(FNV1024context *const, void const *vin, long int length);
extern int FNV1024stringin(FNV1024context *const, char const *in);
extern int FNV1024filein(FNV1024context *const, char const *fname);
extern int FNV1024result(FNV1024context *const, uint8_t out[FNV1024size]);

#ifdef __cplusplus
}
#endif

#endif /* _FNV1024_H_ */
