/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

#ifndef _FNV256_H_
#define _FNV256_H_

/*
 *  Description:
 *      This file provides headers for the 256-bit version of
 *      the FNV-1a non-cryptographic hash algorithm.
 */

#include "FNVconfig.h"
#include "FNVErrorCodes.h"

#include <stdint.h>
#define FNV256size (256 / 8)

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
 *  This structure holds context information for an FNV256 hash
 */
#ifdef FNV_64bitIntegers
/* version if 64-bit integers supported */
typedef struct FNV256context_s {
    int      Computed; /* state */
    uint32_t Hash[FNV256size / 4];
} FNV256context;

#else
/* version if 64-bit integers NOT supported */
typedef struct FNV256context_s {
    int      Computed; /* state */
    uint16_t Hash[FNV256size / 2];
} FNV256context;

#endif /* FNV_64bitIntegers */

/*  Function Prototypes:
 *
 *    FNV256string: hash a zero-terminated string not including
 *                  the terminating zero
 *    FNV246stgringBasis: also takes an offset_basis parameter
 *
 *    FNV256block: hash a specified length byte vector
 *    FNV256blockBasis: also takes an offset_basis parameter
 *
 *    FNV256file: hash the contents of a file
 *    FNV256fileBasis: also takes an offset_basis parameter
 *
 *    FNV256init: initializes an FNV256 context
 *    FNV256initBasis:  initializes an FNV256 context with a
 *                     provided 32-byte vector basis
 *    FNV256blockin: hash in a specified length byte vector
 *    FNV256stringin: hash in a zero-terminated string not
 *                    including the terminating zero
 *    FNV256filein: hash in the contents of a file
 *    FNV256result: returns the hash value
 *
 *    Hash is returned as an array of 8-bit unsigned integers
 */

#ifdef __cplusplus
extern "C" {
#endif

/* FNV256 */
extern int FNV256string(char const *in, uint8_t out[FNV256size]);
extern int FNV256stringBasis(char const *in, uint8_t out[FNV256size], uint8_t const basis[FNV256size]);
extern int FNV256block(void const *vin, long int length, uint8_t out[FNV256size]);
extern int FNV256blockBasis(void const *vin, long int length, uint8_t out[FNV256size], uint8_t const basis[FNV256size]);
extern int FNV256file(char const *fname, uint8_t out[FNV256size]);
extern int FNV256fileBasis(char const *fname, uint8_t out[FNV256size], uint8_t const basis[FNV256size]);
extern int FNV256init(FNV256context *const);
extern int FNV256initBasis(FNV256context *const, uint8_t const basis[FNV256size]);
extern int FNV256blockin(FNV256context *const, void const *vin, long int length);
extern int FNV256stringin(FNV256context *const, char const *in);
extern int FNV256filein(FNV256context *const, char const *fname);
extern int FNV256result(FNV256context *const, uint8_t out[FNV256size]);

#ifdef __cplusplus
}
#endif

#endif /* _FNV256_H_ */
