/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

#ifndef _FNV512_H_
#define _FNV512_H_

/*
 *  Description:
 *      This file provides headers for the 512-bit version of
 *      the FNV-1a non-cryptographic hash algorithm.
 */

#include "FNVconfig.h"
#include "FNVErrorCodes.h"

#include <stdint.h>
#define FNV512size (512 / 8)

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
 *  This structure holds context information for an FNV512 hash
 */
#ifdef FNV_64bitIntegers
/* version if 64-bit integers supported */
typedef struct FNV512context_s {
    int      Computed; /* state */
    uint32_t Hash[FNV512size / 4];
} FNV512context;

#else
/* version if 64-bit integers NOT supported */
typedef struct FNV512context_s {
    int      Computed; /* state */
    uint16_t Hash[FNV512size / 2];
} FNV512context;

#endif /* FNV_64bitIntegers */

/*  Function Prototypes:
 *
 *    FNV512string: hash a zero-terminated string not including
 *                   the terminating zero
 *    FNV512stringBasis: also takes an offset_basis parameter
 *
 *    FNV512block: hash a specified length byte vector
 *    FNV512blockBasis: also takes an offset_basis parameter
 *
 *    FNV512file: hash the contents of a file
 *    FNV512fileBasis: also takes an offset_basis parameter
 *
 *    FNV512init: initializes an FNV1024 context
 *    FNV512initBasis: initializes an FNV1024 context with a
 *                      provided 128-byte vector basis
 *    FNV512blockin: hash in a specified length byte vector
 *    FNV512stringin: hash in a zero-terminated string not
 *                     including the terminating zero
 *    FMNV512filein: hash in the contents of a file
 *    FNV512result: returns the hash value
 *
 *    Hash is returned as an array of 8-bit unsigned integers
 */

#ifdef __cplusplus
extern "C" {
#endif

/* FNV512 */
extern int FNV512string(char const *in, uint8_t out[FNV512size]);
extern int FNV512stringBasis(char const *in, uint8_t out[FNV512size], uint8_t const basis[FNV512size]);
extern int FNV512block(void const *vin, long int length, uint8_t out[FNV512size]);
extern int FNV512blockBasis(void const *vin, long int length, uint8_t out[FNV512size], uint8_t const basis[FNV512size]);
extern int FNV512file(char const *fname, uint8_t out[FNV512size]);
extern int FNV512fileBasis(char const *fname, uint8_t out[FNV512size], uint8_t const basis[FNV512size]);
extern int FNV512init(FNV512context *const);
extern int FNV512initBasis(FNV512context *const, uint8_t const basis[FNV512size]);
extern int FNV512blockin(FNV512context *const, void const *vin, long int length);
extern int FNV512stringin(FNV512context *const, char const *in);
extern int FNV512filein(FNV512context *const, char const *fname);
extern int FNV512result(FNV512context *const, uint8_t out[FNV512size]);

#ifdef __cplusplus
}
#endif

#endif /* _FNV512_H_ */
