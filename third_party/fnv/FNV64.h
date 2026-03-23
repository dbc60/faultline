/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

#ifndef _FNV64_H_
#define _FNV64_H_

/* Description:
 *      This file provides headers for the 64-bit version of
 *      the FNV-1a non-cryptographic hash algorithm.
 */

#include "FNVconfig.h"
#include "FNVErrorCodes.h"

#include <stdint.h>
#define FNV64size (64 / 8)
#ifdef FNV_64bitIntegers
#define FNV64basis 0xCBF29CE484222325
#endif

/* If you do not have the ISO standard stdint.h header file, then
 * you must typedef the following types:
 *
 *    type             meaning
 *  uint64_t        unsigned 64-bit integer (ifdef FNV_64bitIntegers)
 *  uint32_t        unsigned 32-bit integer
 *  uint16_t        unsigned 16-bit integer
 *  uint8_t         unsigned 8-bit integer (i.e., unsigned char)
 */

/*
 *  This structure holds context information for an FNV64 hash
 */
#ifdef FNV_64bitIntegers
/* version if 64-bit integers supported */
typedef struct FNV64context_s {
    int      Computed; /* state */
    uint64_t Hash;
} FNV64context;

#else
/* version if 64-bit integers NOT supported */
typedef struct FNV64context_s {
    int      Computed; /* state */
    uint16_t Hash[FNV64size / 2];
} FNV64context;

#endif /* FNV_64bitIntegers */

/*  Function Prototypes:
 *
 *    FNV64string: hash a zero-terminated string not including
 *                 the terminating zero
 *    FNV164stringBasis: also takes an offset_basis parameter
 *
 *    FNV64block: hash a specified length byte vector
 *    FNV64blockBasis: also takes an offset_basis parameter
 *
 *    FNV64file: hash the contents of a file
 *    FNV128fileBasis: also takes an offset_basis parameter
 *
 *    FNV64init: initializes an FNV64 context
 *    FNV64initBasis: initializes an FNV64 context with a
 *                    provided 8-byte vector basis
 *    FNV64blockin: hash in a specified length byte vector
 *    FNV64stringin: hash in a zero-terminated string not
 *                   including the terminating zero
 *    FNV128filein: hash in the contents of a file
 *    FNV64result: returns the hash value
 *
 * Hash is returned as an 8-byte vector by the functions above.
 *    If 64-bit integers are supported, the following return
 *    a 64-bit integer.
 *
 *    FNV64INTstring: hash a zero-terminated string not including
 *                 the terminating zero
 *    FNV32INTstringBasis: also takes an offset_basis parameter
 *
 *    FNV64INTblock: hash a specified length byte vector
 *    FNV32INTblockBasis: also takes an offset_basis parameter
 *
 *    FNV64INTfile: hash the contents of a file
 *    FNV32INTfileBasis: also takes an offset_basis parameter
 *
 *    FNV64INTinitBasis: initializes an FNV32 context with a
 *                     provided 64-bit integer basis
 *    FNV64INTresult: returns the hash value
 */

#ifdef __cplusplus
extern "C" {
#endif

/* FNV64 */
extern int FNV64string(char const *in, uint8_t out[FNV64size]);
extern int FNV64stringBasis(char const *in, uint8_t out[FNV64size], uint8_t const basis[FNV64size]);
extern int FNV64block(void const *vin, long int length, uint8_t out[FNV64size]);
extern int FNV64blockBasis(void const *vin, long int length, uint8_t out[FNV64size], uint8_t const basis[FNV64size]);
extern int FNV64file(char const *fname, uint8_t out[FNV64size]);
extern int FNV64fileBasis(char const *fname, uint8_t out[FNV64size], uint8_t const basis[FNV64size]);
extern int FNV64init(FNV64context *const);
extern int FNV64initBasis(FNV64context *const, uint8_t const basis[FNV64size]);
extern int FNV64blockin(FNV64context *const, void const *vin, long int length);
extern int FNV64stringin(FNV64context *const, char const *in);
extern int FNV64filein(FNV64context *const, char const *fname);
extern int FNV64result(FNV64context *const, uint8_t out[FNV64size]);

#ifdef FNV_64bitIntegers
extern int FNV64INTstring(const char *in, uint64_t *const out);
extern int FNV64INTstringBasis(char const *in, uint64_t *const out, uint64_t basis);
extern int FNV64INTblock(void const *vin, long int length, uint64_t *const out);
extern int FNV64INTblockBasis(void const *vin, long int length, uint64_t *const out, uint64_t basis);
extern int FNV64INTfile(char const *fname, uint64_t *const out);
extern int FNV64INTfileBasis(char const *fname, uint64_t *const out, uint64_t basis);
extern int FNV64INTinitBasis(FNV64context *const, uint64_t basis);
extern int FNV64INTresult(FNV64context *const, uint64_t *const out);
#endif /* FNV_64bitIntegers */

#ifdef __cplusplus
}
#endif

#endif /* _FNV64_H_ */
