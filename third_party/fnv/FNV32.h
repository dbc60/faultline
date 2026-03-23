/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

#ifndef _FNV32_H_
#define _FNV32_H_

/*
 *  Description:
 *      This file provides headers for the 32-bit version of
 *      the FNV-1a non-cryptographic hash algorithm.
 */

#include "FNVconfig.h"
#include "FNVErrorCodes.h"

#include <stdint.h>
#define FNV32size  (32 / 8)
#define FNV32basis 0x811C9DC5

/* If you do not have the ISO standard stdint.h header file, then
 * you must typedef the following types:
 *
 *    type              meaning
 *  uint32_t         unsigned 32-bit integer
 *  uint8_t          unsigned 8-bit integer (i.e., unsigned char)
 */

/*
 *  This structure holds context information for an FNV32 hash
 */
typedef struct FNV32context_s {
    int      Computed; /* state */
    uint32_t Hash;
} FNV32context;

/*
 * Function Prototypes:
 *
 *    FNV32string: hash a zero-terminated string not including
 *                 the terminating zero
 *    FNV32stringBasis: also takes an offset_basis parameter
 *
 *    FNV32block: hash a specified length byte vector
 *    FNV32blockBasis: also takes an offset_basis parameter
 *
 *    FNV32file: hash the contents of a file
 *    FNV32fileBasis: also takes an offset_basis parameter
 *
 *    FNV32init:  initializes an FNV32 context
 *    FNV32initBasis: initializes an FNV32 context with a
 *                    provided 4-byte vector basis
 *    FNV32blockin:  hash in a specified length byte vector
 *    FNV32stringin: hash in a zero-terminated string not
 *                   including the terminating zero
 *    FNV32filein: hash in the contents of a file
 *    FNV32result: returns the hash value
 *
 * Hash is returned as a 4-byte vector by the functions above,
 *    and the following return a 32-bit unsigned integer:
 *
 *    FNV32INTstring: hash a zero-terminated string not including
 *                 the terminating zero
 *    FNV32INTstringBasis: also takes an offset_basis parameter
 *
 *    FNV32INTblock: hash a specified length byte vector
 *    FNV32INTblockBasis: also takes an offset_basis parameter
 *
 *    FNV32INTfile: hash the contents of a file
 *    FNV32INTfileBasis: also takes an offset_basis parameter
 *
 *    FNV32INTinitBasis: initializes an FNV32 context with a
 *                     provided 32-bit integer basis
 *    FNV32INTresult: returns the hash value
 */

#ifdef __cplusplus
extern "C" {
#endif

/* FNV32 */
extern int FNV32INTstring(char const *in, uint32_t *const out);
extern int FNV32INTstringBasis(char const *in, uint32_t *const out, uint32_t basis);
extern int FNV32string(char const *in, uint8_t out[FNV32size]);
extern int FNV32stringBasis(char const *in, uint8_t out[FNV32size], uint8_t const basis[FNV32size]);
extern int FNV32INTblock(void const *vin, long int length, uint32_t *const out);
extern int FNV32INTblockBasis(void const *vin, long int length, uint32_t *const out, uint32_t basis);
extern int FNV32block(void const *vin, long int length, uint8_t out[FNV32size]);
extern int FNV32blockBasis(void const *vin, long int length, uint8_t out[FNV32size], uint8_t const basis[FNV32size]);
extern int FNV32INTfile(char const *fname, uint32_t *const out);
extern int FNV32INTfileBasis(char const *fname, uint32_t *const out, uint32_t basis);
extern int FNV32file(char const *fname, uint8_t out[FNV32size]);
extern int FNV32fileBasis(char const *fname, uint8_t out[FNV32size], uint8_t const basis[FNV32size]);
extern int FNV32init(FNV32context *const);
extern int FNV32INTinitBasis(FNV32context *const, uint32_t basis);
extern int FNV32initBasis(FNV32context *const, uint8_t const basis[FNV32size]);
extern int FNV32blockin(FNV32context *const, void const *vin, long int length);
extern int FNV32stringin(FNV32context *const, char const *in);
extern int FNV32filein(FNV32context *const, char const *fname);
extern int FNV32INTresult(FNV32context *const, uint32_t *const out);
extern int FNV32result(FNV32context *const, uint8_t out[FNV32size]);

#ifdef __cplusplus
}
#endif

#endif /* _FNV32_H_ */
