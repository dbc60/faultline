/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

/* This code implements the FNV (Fowler, Noll, Vo) non-cryptographic
 * hash function FNV-1a for 32-bit hashes.
 */

#include <stdio.h>

#include "fnv-private.h"
#include "FNV32.h"

/* 32-bit FNV_prime = 2^24 + 2^8 + 0x93 */
#define FNV32prime 0x01000193

/* FNV32 hash a zero-terminated string not including the zero
*****************************************************************/
int FNV32INTstring(char const *in, uint32_t *const out) {
    return FNV32INTstringBasis(in, out, FNV32basis);
} /* end FNV32INTstring */

/* FNV32 hash a zero-terminated string not including the zero
 * with a non-standard basis
 *****************************************************************/
int FNV32INTstringBasis(char const *in, uint32_t *const out, uint32_t basis) {
    uint8_t ch;

    if (!in || !out)
        return fnvNull; /* Null input pointer */
    while ((ch = *in++))
        basis = FNV32prime * (basis ^ ch);
    *out = basis;
    return fnvSuccess;
} /* end FNV32INTstringBasis */

/* FNV32 hash a zero-terminated string not including the zero
*****************************************************************/
int FNV32string(char const *in, uint8_t out[FNV32size]) {
    uint32_t temp;
    uint8_t  ch;

    if (!in || !out)
        return fnvNull; /* Null input pointer */
    temp = FNV32basis;
    while ((ch = *in++))
        temp = FNV32prime * (temp ^ ch);
    for (int i = 0; i < FNV32size; ++i)
        out[i] = ((uint8_t *)&temp)[i];
    return fnvSuccess;
} /* end FNV32string */

/* FNV32 hash a zero-terminated string not including the zero
 * with a non-standard basis
 *****************************************************************/
int FNV32stringBasis(char const *in, uint8_t out[FNV32size], uint8_t const basis[FNV32size]) {
    uint32_t temp;
    int      i;
    uint8_t  ch;

    if (!in || !out || !basis)
        return fnvNull; /* Null input pointer */
    temp = basis[0] + (basis[1] << 8) + (basis[2] << 16) + (basis[3] << 24);
    while ((ch = *in++))
        temp = FNV32prime * (temp ^ ch);
    out[0] = temp & 0xFF;
    for (i = 1; i < FNV32size; ++i) {
        temp >>= 8;
        out[i] = temp & 0xFF;
    }
    return fnvSuccess;
} /* end FNV32stringBasis */

/* FNV32 hash a counted block returning an integer
 ****************************************************************/
int FNV32INTblock(void const *vin, long int length, uint32_t *const out) {
    return FNV32INTblockBasis(vin, length, out, FNV32basis);
} /* end FNV32INTblock */

/* FNV32 hash a counted block with a non-standard basis
 ****************************************************************/
int FNV32INTblockBasis(void const *vin, long int length, uint32_t *const out, uint32_t basis) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp;

    if (!in || !out)
        return fnvNull; /* Null input pointer */
    if (length < 0)
        return fnvBadParam;
    for (temp = basis; length > 0; length--)
        temp = FNV32prime * (temp ^ *in++);
    *out = temp;
    return fnvSuccess;
} /* end FNV32INTblockBasis */

/* FNV32 hash a counted block returning a 4-byte vector
 ****************************************************************/
int FNV32block(void const *vin, long int length, uint8_t out[FNV32size]) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp;

    if (!in || !out)
        return fnvNull; /* Null input pointer */
    if (length < 0)
        return fnvBadParam;
    for (temp = FNV32basis; length > 0; length--)
        temp = FNV32prime * (temp ^ *in++);
    for (int i = 0; i < FNV32size; ++i)
        out[i] = ((uint8_t *)&temp)[i];
    return fnvSuccess;
} /* end FNV32block */

/* FNV32 hash a counted block with a non-standard basis
 ****************************************************************/
int FNV32blockBasis(void const *vin, long int length, uint8_t out[FNV32size], uint8_t const basis[FNV32size]) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp;

    if (!in || !out || !basis)
        return fnvNull; /* Null input pointer */
    if (length < 0)
        return fnvBadParam;
    temp = basis[0] + (basis[1] << 8) + (basis[2] << 16) + (basis[3] << 24);
    for (; length > 0; length--)
        temp = FNV32prime * (temp ^ *in++);
    for (int i = 0; i < FNV32size; ++i)
        out[i] = ((uint8_t *)&temp)[i];
    return fnvSuccess;
} /* end FNV32blockBasis */

/* hash the contents of a file, return 32-bit integer
 ******************************************************************/
int FNV32INTfile(char const *fname, uint32_t *const out) {
    return FNV32INTfileBasis(fname, out, FNV32basis);
} /* end FNV32INTfile */

/* hash the contents of a file, return 32-bit integer
 * with a non-standard basis
 ******************************************************************/
int FNV32INTfileBasis(char const *fname, uint32_t *const out, uint32_t basis) {
    FNV32context e32Context;
    int          error;

    if (!out)
        return fnvNull;
    if ((error = FNV32INTinitBasis(&e32Context, basis)))
        return error;
    if ((error = FNV32filein(&e32Context, fname)))
        return error;
    return FNV32INTresult(&e32Context, out);
} /* end FNV32INTfileBasis */

/* hash the contents of a file, return 4-byte vector
 ******************************************************************/
int FNV32file(char const *fname, uint8_t out[FNV32size]) {
    FNV32context e32Context;
    int          error;

    if (!out)
        return fnvNull;
    if ((error = FNV32init(&e32Context)))
        return error;
    if ((error = FNV32filein(&e32Context, fname)))
        return error;
    return FNV32result(&e32Context, out);
} /* end FNV32file */

/* hash the contents of a file, return 4-byte vector
 * with a non-standard basis
 ******************************************************************/
int FNV32fileBasis(char const *fname, uint8_t out[FNV32size], uint8_t const basis[FNV32size]) {
    FNV32context e32Context;
    int          error;

    if (!out)
        return fnvNull;
    if ((error = FNV32initBasis(&e32Context, basis)))
        return error;
    if ((error = FNV32filein(&e32Context, fname)))
        return error;
    return FNV32result(&e32Context, out);
} /* end FNV32fileBasis */

//**************************************************************
//       Set of init, input, and output functions below
//       to incrementally compute FNV32
//**************************************************************

/* initialize context
 ***************************************************************/
int FNV32init(FNV32context *const ctx) {
    return FNV32INTinitBasis(ctx, FNV32basis);
} /* end FNV32init */

/* initialize context with a provided 32-bit integer basis
 ***************************************************************/
int FNV32INTinitBasis(FNV32context *const ctx, uint32_t basis) {
    if (!ctx)
        return fnvNull;
    ctx->Hash     = basis;
    ctx->Computed = FNVinited + FNV32state;
    return fnvSuccess;
} /* end FNV32INTinitBasis */

/* initialize context with a provided 4-byte vector basis
 ***************************************************************/
int FNV32initBasis(FNV32context *const ctx, uint8_t const basis[FNV32size]) {
    if (!ctx || !basis)
        return fnvNull;
    ctx->Hash     = basis[0] + (basis[1] << 8) + (basis[2] << 16) + (basis[3] << 24);
    ctx->Computed = FNVinited + FNV32state;
    return fnvSuccess;
} /* end FNV32initBasis */

/* hash in a counted block
 ***************************************************************/
int FNV32blockin(FNV32context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV32state:
        ctx->Computed = FNVcomputed + FNV32state;
        break;
    case FNVcomputed + FNV32state:
        break;
    default:
        return fnvStateError;
    }
    for (temp = ctx->Hash; length > 0; length--)
        temp = FNV32prime * (temp ^ *in++);
    ctx->Hash = temp;
    return fnvSuccess;
} /* end FNV32blockin */

/* hash in a zero-terminated string not including the zero
 ***************************************************************/
int FNV32stringin(FNV32context *const ctx, char const *in) {
    uint32_t temp;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV32state:
        ctx->Computed = FNVcomputed + FNV32state;
        break;
    case FNVcomputed + FNV32state:
        break;
    default:
        return fnvStateError;
    }
    temp = ctx->Hash;
    while ((ch = (uint8_t)*in++))
        temp = FNV32prime * (temp ^ ch);
    ctx->Hash = temp;
    return fnvSuccess;
} /* end FNV32stringin */

/* hash in the contents of a file
 ******************************************************************/
int FNV32filein(FNV32context *const e32Context, char const *fname) {
    FILE    *fp;
    long int i;
    char     buf[1024];
    int      error;

    if (!e32Context || !fname)
        return fnvNull;
    switch (e32Context->Computed) {
    case FNVinited + FNV32state:
        e32Context->Computed = FNVcomputed + FNV32state;
        break;
    case FNVcomputed + FNV32state:
        break;
    default:
        return fnvStateError;
    }
    if (fopen_s(&fp, fname, "rb") != 0)
        return fnvBadParam;
    if ((error = FNV32blockin(e32Context, "", 0))) {
        fclose(fp);
        return error;
    }
    while ((i = (long)fread(buf, 1, sizeof(buf), fp)) > 0)
        if ((error = FNV32blockin(e32Context, buf, i))) {
            fclose(fp);
            return error;
        }
    error = ferror(fp);
    fclose(fp);
    if (error)
        return fnvBadParam;
    return fnvSuccess;
} /* end FNV32filein */

/* return hash as an integer
 ***************************************************************/
int FNV32INTresult(FNV32context *const ctx, uint32_t *const out) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV32state)
        return fnvStateError;
    ctx->Computed = FNVemptied + FNV32state;
    *out          = ctx->Hash;
    ctx->Hash     = 0;
    return fnvSuccess;
} /* end FNV32INTresult */

/* return hash as a 4-byte vector
 ***************************************************************/
int FNV32result(FNV32context *const ctx, uint8_t out[FNV32size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV32state)
        return fnvStateError;
    ctx->Computed = FNVemptied + FNV32state;
    for (int i = 0; i < FNV32size; ++i)
        out[i] = ((uint8_t *)&ctx->Hash)[i];
    ctx->Hash = 0;
    return fnvSuccess;
} /* end FNV32result */
