/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

/* This file implements the FNV (Fowler, Noll, Vo) non-cryptographic
 * hash function FNV-1a for 64-bit hashes.
 */

#include <stdio.h>

#include "FNVconfig.h"
#include "fnv-private.h"
#include "FNV64.h"

//*****************************************************************
// CODE THAT IS THE SAME FOR 32-BIT and 64-BIT ARITHMETIC
//*****************************************************************

/* hash the contents of a file, return byte vector
 ******************************************************************/
int FNV64file(char const *fname, uint8_t out[FNV64size]) {
    FNV64context e64Context;
    int          error;

    if (!out)
        return fnvNull;
    if ((error = FNV64init(&e64Context)))
        return error;
    if ((error = FNV64filein(&e64Context, fname)))
        return error;
    return FNV64result(&e64Context, out);
} /* end FNV64file */

/* hash the contents of a file, return 64-bit integer
 * with a non-standard basis
 ******************************************************************/
int FNV64fileBasis(char const *fname, uint8_t out[FNV64size], uint8_t const basis[FNV64size]) {
    FNV64context e64Context;
    int          error;

    if (!out)
        return fnvNull;
    if ((error = FNV64initBasis(&e64Context, basis)))
        return error;
    if ((error = FNV64filein(&e64Context, fname)))
        return error;
    return FNV64result(&e64Context, out);
} /* end FNV64fileBasis */

/* hash in the contents of a file
 ******************************************************************/
int FNV64filein(FNV64context *const e64Context, char const *fname) {
    FILE    *fp;
    long int i;
    char     buf[1024];
    int      error;

    if (!e64Context || !fname)
        return fnvNull;
    switch (e64Context->Computed) {
    case FNVinited + FNV64state:
        e64Context->Computed = FNVcomputed + FNV64state;
        break;
    case FNVcomputed + FNV64state:
        break;
    default:
        return fnvStateError;
    }
    if (fopen_s(&fp, fname, "rb") != 0)
        return fnvBadParam;
    if ((error = FNV64blockin(e64Context, "", 0))) {
        fclose(fp);
        return error;
    }
    while ((i = (long)fread(buf, 1, sizeof(buf), fp)) > 0)
        if ((error = FNV64blockin(e64Context, buf, i))) {
            fclose(fp);
            return error;
        }
    error = ferror(fp);
    fclose(fp);
    if (error)
        return fnvBadParam;
    return fnvSuccess;
}

//*****************************************************************
// START VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//*****************************************************************
#ifdef FNV_64bitIntegers

/* 64-bit FNV_prime = 2^40 + 2^8 + 0xb3 */
#define FNV64prime 0x00000100000001B3

/* FNV64 hash a zero-terminated string not including the zero
 * to a 64-bit integer  (64-bit)
 ******************************************************************/
int FNV64INTstring(char const *in, uint64_t *const out) {
    return FNV64INTstringBasis(in, out, FNV64basis);
} /* end FNV64INTstring */

/* FNV64 hash a zero-terminated string not including the zero
 * to a 64-bit integer  (64-bit) with a non-standard basis
 ******************************************************************/
int FNV64INTstringBasis(char const *in, uint64_t *const out, uint64_t basis) {
    uint64_t temp;
    uint8_t  ch;

    if (!in || !out)
        return fnvNull; /* Null input pointer */
    temp = basis;
    while ((ch = *in++))
        temp = FNV64prime * (temp ^ ch);
    *out = temp;
    return fnvSuccess;
} /* end FNV64INTstringBasis */

/* FNV64 hash a zero-terminated string to a 64-bit integer
 * to a byte vector  (64-bit)
 ******************************************************************/
int FNV64string(char const *in, uint8_t out[FNV64size]) {
    uint64_t temp;
    uint8_t  ch;

    if (!in || !out)
        return fnvNull; /* Null input pointer */
    temp = FNV64basis;
    while ((ch = *in++))
        temp = FNV64prime * (temp ^ ch);
    for (int i = 0; i < FNV64size; ++i)
        out[i] = ((uint8_t *)&temp)[i];
    return fnvSuccess;
} /* end FNV64string */

/* FNV64 hash a zero-terminated string to a 64-bit integer
 * to a byte vector  (64-bit) with a non-standard basis
 ******************************************************************/
int FNV64stringBasis(char const *in, uint8_t out[FNV64size], uint8_t const basis[FNV64size]) {
    uint64_t temp;
    int      i;
    uint8_t  ch;

    if (!in || !out || !basis)
        return fnvNull; /* Null input pointer */
    temp = basis[7];
    for (i = FNV64size - 2; i >= 0; --i)
        temp = (temp << 8) + basis[i];
    while ((ch = *in++))
        temp = FNV64prime * (temp ^ ch);
    for (i = 0; i < FNV64size; ++i)
        out[i] = ((uint8_t *)&temp)[i];
    return fnvSuccess;
} /* end FNV64stringBasis */

/* FNV64 hash a counted block to a 64-bit integer  (64-bit)
 ******************************************************************/
int FNV64INTblock(void const *vin, long int length, uint64_t *const out) {
    return FNV64INTblockBasis(vin, length, out, FNV64basis);
} /* end FNV64INTblock */

/* FNV64 hash a counted block to a 64-bit integer  (64-bit)
 * with a non-standard basis
 ******************************************************************/
int FNV64INTblockBasis(void const *vin, long int length, uint64_t *const out, uint64_t basis) {
    uint8_t const *in = (uint8_t const *)vin;
    uint64_t       temp;

    if (!in || !out)
        return fnvNull; /* Null input/out pointer */
    if (length < 0)
        return fnvBadParam;
    for (temp = basis; length > 0; length--)
        temp = FNV64prime * (temp ^ *in++);
    *out = temp;
    return fnvSuccess;
} /* end FNV64INTblockBasis */

/* FNV64 hash a counted block to a byte vector  (64-bit)
 ******************************************************************/
int FNV64block(void const *vin, long int length, uint8_t out[FNV64size]) {
    uint8_t const *in = (uint8_t const *)vin;
    uint64_t       temp;

    if (!in || !out)
        return fnvNull; /* Null input/out pointer */
    if (length < 0)
        return fnvBadParam;
    for (temp = FNV64basis; length > 0; length--)
        temp = FNV64prime * (temp ^ *in++);
    for (int i = 0; i < FNV64size; ++i)
        out[i] = ((uint8_t *)&temp)[i];
    return fnvSuccess;
} /* end FNV64block */

/* FNV64 hash a counted block to a byte vector  (64-bit)
 * with a non-standard basis
 ******************************************************************/
int FNV64blockBasis(void const *vin, long int length, uint8_t out[FNV64size], uint8_t const basis[FNV64size]) {
    uint8_t const *in = (uint8_t const *)vin;
    uint64_t       temp;
    int            i;

    if (!in || !out || !basis)
        return fnvNull; /* Null input/out pointer */
    if (length < 0)
        return fnvBadParam;
    temp = basis[7];
    for (i = FNV64size - 2; i >= 0; --i)
        temp = (temp << 8) + basis[i];
    for (; length > 0; length--)
        temp = FNV64prime * (temp ^ *in++);
    for (i = 0; i < FNV64size; ++i)
        out[i] = ((uint8_t *)&temp)[i];
    return fnvSuccess;
} /* end FNV64blockBasis */

//*****************************************************************
// Set of init, input, and output functions below
// to incrementally compute FNV64
//*****************************************************************

/* initialize context  (64-bit)
 ******************************************************************/
int FNV64init(FNV64context *const ctx) {
    return FNV64INTinitBasis(ctx, FNV64basis);
} /* end FNV64init */

/* initialize context with a provided 64-bit integer basis  (64-bit)
 ******************************************************************/
int FNV64INTinitBasis(FNV64context *const ctx, uint64_t basis) {
    if (!ctx)
        return fnvNull;
    ctx->Hash     = basis;
    ctx->Computed = FNVinited + FNV64state;
    return fnvSuccess;
} /* end FNV64INTinitBasis */

/* initialize context with a provided 8-byte vector basis  (64-bit)
 ******************************************************************/
int FNV64initBasis(FNV64context *const ctx, uint8_t const basis[FNV64size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV64size; ++i)
        ((uint8_t *)&ctx->Hash)[i] = basis[i];
    ctx->Computed = FNVinited + FNV64state;
    return fnvSuccess;
} /* end FNV64initBasis */

/* hash in a counted block  (64-bit)
 ******************************************************************/
int FNV64blockin(FNV64context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint64_t       temp;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV64state:
        ctx->Computed = FNVcomputed + FNV64state;
        break;
    case FNVcomputed + FNV64state:
        break;
    default:
        return fnvStateError;
    }
    for (temp = ctx->Hash; length > 0; length--)
        temp = FNV64prime * (temp ^ *in++);
    ctx->Hash = temp;
    return fnvSuccess;
} /* end FNV64blockin */

/* hash in a zero-terminated string not including the zero (64-bit)
 ******************************************************************/
int FNV64stringin(FNV64context *const ctx, char const *in) {
    uint64_t temp;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV64state:
        ctx->Computed = FNVcomputed + FNV64state;
        break;
    case FNVcomputed + FNV64state:
        break;
    default:
        return fnvStateError;
    }
    temp = ctx->Hash;
    while ((ch = (uint8_t)*in++))
        temp = FNV64prime * (temp ^ ch);
    ctx->Hash = temp;
    return fnvSuccess;
} /* end FNV64stringin */

/* return hash as 64-bit int (64-bit)
 ******************************************************************/
int FNV64INTresult(FNV64context *const ctx, uint64_t *const out) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV64state)
        return fnvStateError;
    ctx->Computed = FNVemptied + FNV64state;
    *out          = ctx->Hash;
    ctx->Hash     = 0;
    return fnvSuccess;
} /* end FNV64INTresult */

/* return hash as 8-byte vector (64-bit)
 ******************************************************************/
int FNV64result(FNV64context *const ctx, uint8_t out[FNV64size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV64state)
        return fnvStateError;
    ctx->Computed = FNVemptied + FNV64state;
    for (int i = 0; i < FNV64size; ++i)
        out[i] = ((uint8_t *)&ctx->Hash)[i];
    ctx->Hash = 0;
    return fnvSuccess;
} /* end FNV64result */

/* hash the contents of a file, return 64-bit integer
 ******************************************************************/
int FNV64INTfile(char const *fname, uint64_t *const out) {
    FNV64context e64Context;
    int          error;

    if (!out)
        return fnvNull;
    if ((error = FNV64init(&e64Context)))
        return error;
    if ((error = FNV64filein(&e64Context, fname)))
        return error;
    return FNV64INTresult(&e64Context, out);
} /* end FNV64INTfile */

/* hash the contents of a file, return 64-bit integer
 * with a non-standard basis
 ******************************************************************/
int FNV64INTfileBasis(char const *fname, uint64_t *const out, uint64_t basis) {
    FNV64context e64Context;
    int          error;

    if (!out)
        return fnvNull;
    if ((error = FNV64INTinitBasis(&e64Context, basis)))
        return error;
    if ((error = FNV64filein(&e64Context, fname)))
        return error;
    return FNV64INTresult(&e64Context, out);
} /* end FNV64INTfileBasis */

//***************************************************************
// END VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//***************************************************************
#else /*  FNV_64bitIntegers */
//***************************************************************
// START VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//***************************************************************

/* 64-bit FNV_prime = 2^40 + 2^8 + 0xb3 */
/* #define FNV64prime 0x00000100000001B3 */
#define FNV64primeX 0x01B3
#define FNV64shift  8

/* FNV64 hash a zero-terminated string not including the zero
 ******************************************************************/
int FNV64string(char const *in, uint8_t out[FNV64size]) {
    FNV64context ctx;
    int          error;

    if ((error = FNV64init(&ctx)))
        return error;
    if ((error = FNV64stringin(&ctx, in)))
        return error;
    return FNV64result(&ctx, out);
} /* end FNV64string */

/* FNV64 hash a zero-terminated string not including the zero
 * with a non-standard offset_basis
 ******************************************************************/
int FNV64stringBasis(char const *in, uint8_t out[FNV64size], uint8_t const basis[FNV64size]) {
    FNV64context ctx;
    int          error;

    if ((error = FNV64initBasis(&ctx, basis)))
        return error;
    if ((error = FNV64stringin(&ctx, in)))
        return error;
    return FNV64result(&ctx, out);
} /* end FNV64stringBasis */

/* FNV64 hash a counted block
 ******************************************************************/
int FNV64block(void const *vin, long int length, uint8_t out[FNV64size]) {
    FNV64context ctx;
    int          error;

    if ((error = FNV64init(&ctx)))
        return error;
    if ((error = FNV64blockin(&ctx, vin, length)))
        return error;
    return FNV64result(&ctx, out);
} /* end FNV64block */

/* FNV64 hash a counted block with a non-standard offset_basis
 ******************************************************************/
int FNV64blockBasis(void const *vin, long int length, uint8_t out[FNV64size], uint8_t const basis[FNV64size]) {
    FNV64context ctx;
    int          error;

    if ((error = FNV64initBasis(&ctx, basis)))
        return error;
    if ((error = FNV64blockin(&ctx, vin, length)))
        return error;
    return FNV64result(&ctx, out);
} /* end FNV64blockBasis */

//*****************************************************************
//        Set of init, input, and output functions below
//        to incrementally compute FNV64
//*****************************************************************

/* initialize context  (32-bit)
 ******************************************************************/
int FNV64init(FNV64context *const ctx) {
    if (!ctx)
        return fnvNull;
    ctx->Hash[0]  = 0xCBF2;
    ctx->Hash[1]  = 0x9CE4;
    ctx->Hash[2]  = 0x8422;
    ctx->Hash[3]  = 0x2325;
    ctx->Computed = FNVinited + FNV64state;
    return fnvSuccess;
} /* end FNV64init */

/* initialize context with a non-standard basis (32-bit)
 ******************************************************************/
int FNV64initBasis(FNV64context *const ctx, uint8_t const basis[FNV64size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV64size / 2; ++i) {
        uint32_t temp = *basis++;
        ctx->Hash[i]  = (temp << 8) + *basis++;
    }
    ctx->Computed = FNVinited + FNV64state;
    return fnvSuccess;
} /* end FNV64initBasis */

/* hash in a counted block  (32-bit)
 ******************************************************************/
int FNV64blockin(FNV64context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp[FNV64size / 2];
    uint32_t       temp2[2];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV64state:
        ctx->Computed = FNVcomputed + FNV64state;
        break;
    case FNVcomputed + FNV64state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV64size / 2; ++i)
        temp[i] = ctx->Hash[i];
    for (; length > 0; length--) {
        /* temp = FNV64prime * ( temp ^ *in++ ); */
        temp[3] ^= *in++;
        temp2[1] = temp[3] << FNV64shift;
        temp2[0] = temp[2] << FNV64shift;
        for (i = 0; i < 4; ++i)
            temp[i] *= FNV64primeX;
        temp[1] += temp2[1];
        temp[0] += temp2[0];
        for (i = 2; i >= 0; --i) {
            temp[i] += temp[i + 1] >> 16;
            temp[i + 1] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV64size / 2; ++i)
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
} /* end FNV64blockin */

/* hash in a zero-terminated string not including the zero  (32-bit)
 ******************************************************************/
int FNV64stringin(FNV64context *const ctx, char const *in) {
    uint32_t temp[FNV64size / 2];
    uint32_t temp2[2];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV64state:
        ctx->Computed = FNVcomputed + FNV64state;
        break;
    case FNVcomputed + FNV64state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV64size / 2; ++i)
        temp[i] = ctx->Hash[i];
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV64prime * ( temp ^ ch ); */
        temp[3] ^= ch;
        temp2[1] = temp[3] << FNV64shift;
        temp2[0] = temp[2] << FNV64shift;
        for (i = 0; i < 4; ++i)
            temp[i] *= FNV64primeX;
        temp[1] += temp2[1];
        temp[0] += temp2[0];
        for (i = 2; i >= 0; --i) {
            temp[i] += temp[i + 1] >> 16;
            temp[i + 1] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV64size / 2; ++i)
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
} /* end FNV64stringin */

/* return hash  (32-bit)
 ******************************************************************/
int FNV64result(FNV64context *const ctx, uint8_t out[FNV64size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV64state)
        return fnvStateError;
    for (int i = 0; i < FNV64size / 2; ++i) {
        out[2 * i]     = ctx->Hash[i] >> 8;
        out[2 * i + 1] = ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV64state;
    return fnvSuccess;
} /* end FNV64result */

#endif /*  FNV_64bitIntegers */
//*****************************************************************
// END VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//*****************************************************************
