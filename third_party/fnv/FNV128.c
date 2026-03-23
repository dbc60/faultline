/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

/* This file implements the FNV (Fowler, Noll, Vo) non-cryptographic
 * hash function FNV-1a for 128-bit hashes.
 */

#include <stdio.h>

#include "FNVconfig.h"
#include "fnv-private.h"
#include "FNV128.h"

//*****************************************************************
// COMMON CODE FOR 64- AND 32-BIT INTEGER MODES
//*****************************************************************

/* FNV128 hash a zero-terminated string not including the zero
 ******************************************************************/
int FNV128string(char const *in, uint8_t out[FNV128size]) {
    FNV128context ctx;
    int           error;

    if ((error = FNV128init(&ctx)))
        return error;
    if ((error = FNV128stringin(&ctx, in)))
        return error;
    return FNV128result(&ctx, out);
} /* end FNV128string */

/* FNV128 hash a zero-terminated string not including the zero
 ******************************************************************/
int FNV128stringBasis(char const *in, uint8_t out[FNV128size], uint8_t const basis[FNV128size]) {
    FNV128context ctx;
    int           error;

    if ((error = FNV128initBasis(&ctx, basis)))
        return error;
    if ((error = FNV128stringin(&ctx, in)))
        return error;
    return FNV128result(&ctx, out);
} /* end FNV128stringBasis */

/* FNV128 hash a counted block  (64/32-bit)
 ******************************************************************/
int FNV128block(void const *vin, long int length, uint8_t out[FNV128size]) {
    FNV128context ctx;
    int           error;

    if ((error = FNV128init(&ctx)))
        return error;
    if ((error = FNV128blockin(&ctx, vin, length)))
        return error;
    return FNV128result(&ctx, out);
} /* end FNV128block */

/* FNV128 hash a counted block  (64/32-bit)
 ******************************************************************/
int FNV128blockBasis(void const *vin, long int length, uint8_t out[FNV128size], uint8_t const basis[FNV128size]) {
    FNV128context ctx;
    int           error;

    if ((error = FNV128initBasis(&ctx, basis)))
        return error;
    if ((error = FNV128blockin(&ctx, vin, length)))
        return error;
    return FNV128result(&ctx, out);
} /* end FNV128blockBasis */

/* hash the contents of a file
 ******************************************************************/
int FNV128file(char const *fname, uint8_t out[FNV128size]) {
    FNV128context e128Context;
    int           error;

    if (!out)
        return fnvNull;
    if ((error = FNV128init(&e128Context)))
        return error;
    if ((error = FNV128filein(&e128Context, fname)))
        return error;
    return FNV128result(&e128Context, out);
} /* end FNV128file */

/* hash the contents of a file with a non-standard basis
 ******************************************************************/
int FNV128fileBasis(char const *fname, uint8_t out[FNV128size], uint8_t const basis[FNV128size]) {
    FNV128context e128Context;
    int           error;

    if (!out)
        return fnvNull;
    if ((error = FNV128initBasis(&e128Context, basis)))
        return error;
    if ((error = FNV128filein(&e128Context, fname)))
        return error;
    return FNV128result(&e128Context, out);
} /* end FNV128fileBasis */

/* hash in the contents of a file
 ******************************************************************/
int FNV128filein(FNV128context *const e128Context, char const *fname) {
    FILE    *fp;
    long int i;
    char     buf[1024];
    int      error;

    if (!e128Context || !fname)
        return fnvNull;
    switch (e128Context->Computed) {
    case FNVinited + FNV128state:
        e128Context->Computed = FNVcomputed + FNV128state;
        break;
    case FNVcomputed + FNV128state:
        break;
    default:
        return fnvStateError;
    }
    if (fopen_s(&fp, fname, "rb") != 0)
        return fnvBadParam;
    if ((error = FNV128blockin(e128Context, "", 0))) {
        fclose(fp);
        return error;
    }
    while ((i = (long)fread(buf, 1, sizeof(buf), fp)) > 0)
        if ((error = FNV128blockin(e128Context, buf, i))) {
            fclose(fp);
            return error;
        }
    error = ferror(fp);
    fclose(fp);
    if (error)
        return fnvBadParam;
    return fnvSuccess;
} /* end FNV128filein */

//*****************************************************************
//        START VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//*****************************************************************
#ifdef FNV_64bitIntegers

/* 128-bit FNV_prime = 2^88 + 2^8 + 0x3b */
/* 0x00000000 01000000 00000000 0000013B */
#define FNV128primeX 0x013B
#define FNV128shift  24

//*****************************************************************
//         Set of init, input, and output functions below
//         to incrementally compute FNV128
//*****************************************************************/

/* initialize context  (64-bit)
 ******************************************************************/
int FNV128init(FNV128context *const ctx) {
    uint32_t const FNV128basis[FNV128size / 4] = {0x6C62272E, 0x07BB0142, 0x62B82175, 0x6295C58D};

    if (!ctx)
        return fnvNull;
    for (int i = 0; i < 4; ++i)
        ctx->Hash[i] = FNV128basis[i];
    ctx->Computed = FNVinited + FNV128state;
    return fnvSuccess;
} /* end FNV128init */

/* initialize context with a provided 16-byte vector basis  (64-bit)
 ******************************************************************/
int FNV128initBasis(FNV128context *const ctx, uint8_t const basis[FNV128size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV128size / 4; ++i) {
        uint32_t temp = *basis++ << 24;
        temp += *basis++ << 16;
        temp += *basis++ << 8;
        ctx->Hash[i] = temp + *basis++;
    }
    ctx->Computed = FNVinited + FNV128state;
    return fnvSuccess;
} /* end FNV128initBasis */

/* hash in a counted block  (64-bit)
 ******************************************************************/
int FNV128blockin(FNV128context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint64_t       temp[FNV128size / 4];
    uint64_t       temp2[2];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV128state:
        ctx->Computed = FNVcomputed + FNV128state;
        break;
    case FNVcomputed + FNV128state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV128size / 4; ++i)
        temp[i] = ctx->Hash[i];
    for (; length > 0; length--) {
        /* temp = FNV128prime * ( temp ^ *in++ ); */
        temp[FNV128size / 4 - 1] ^= *in++;
        temp2[1] = temp[3] << FNV128shift;
        temp2[0] = temp[2] << FNV128shift;
        for (i = 0; i < FNV128size / 4; ++i)
            temp[i] *= FNV128primeX;
        temp[1] += temp2[1];
        temp[0] += temp2[0];
        for (i = 3; i > 0; --i) {
            temp[i - 1] += temp[i] >> 32;
            temp[i] &= 0xFFFFFFFF;
        }
    }
    for (i = 0; i < FNV128size / 4; ++i)
        ctx->Hash[i] = (uint32_t)temp[i];
    return fnvSuccess;
} /* end FNV128blockin */

/* hash in a zero-terminated string not including the zero  (64-bit)
 ******************************************************************/
int FNV128stringin(FNV128context *const ctx, char const *in) {
    uint64_t temp[FNV128size / 4];
    uint64_t temp2[2];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV128state:
        ctx->Computed = FNVcomputed + FNV128state;
        break;
    case FNVcomputed + FNV128state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV128size / 4; ++i)
        temp[i] = ctx->Hash[i];
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV128prime * ( temp ^ ch ); */
        temp[3] ^= ch;
        temp2[1] = temp[3] << FNV128shift;
        temp2[0] = temp[2] << FNV128shift;
        for (i = 0; i < FNV128size / 4; ++i)
            temp[i] *= FNV128primeX;
        temp[1] += temp2[1];
        temp[0] += temp2[0];
        for (i = 3; i > 0; --i) {
            temp[i - 1] += temp[i] >> 32;
            temp[i] &= 0xFFFFFFFF;
        }
    }
    for (i = 0; i < FNV128size / 4; ++i)
        ctx->Hash[i] = (uint32_t)temp[i];
    return fnvSuccess;
} /* end FNV128stringin */

/* return hash as 16-byte vector   (64-bit)
 ******************************************************************/
int FNV128result(FNV128context *const ctx, uint8_t out[FNV128size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV128state)
        return fnvStateError;
    for (int i = 0; i < FNV128size / 4; ++i) {
        out[4 * i]     = ctx->Hash[i] >> 24;
        out[4 * i + 1] = (uint8_t)(ctx->Hash[i] >> 16);
        out[4 * i + 2] = (uint8_t)(ctx->Hash[i] >> 8);
        out[4 * i + 3] = (uint8_t)ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV128state;
    return fnvSuccess;
} /* end FNV128result */

//****************************************************************
// END VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//****************************************************************
#else /*  FNV_64bitIntegers */
//****************************************************************
// START VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//****************************************************************

/* 128-bit FNV_prime = 2^88 + 2^8 + 0x3b */
/* 0x00000000 01000000 00000000 0000013B */
#define FNV128primeX 0x013B
#define FNV128shift  8

//*****************************************************************
//         Set of init, input, and output functions below
//         to incrementally compute FNV128
//*****************************************************************

/* initialize context  (32-bit)
 ******************************************************************/
int FNV128init(FNV128context *const ctx) {
    uint16_t const FNV128basis[FNV128size / 2] = {0x6C62, 0x272E, 0x07BB, 0x0142, 0x62B8, 0x2175, 0x6295, 0xC58D};

    if (!ctx)
        return fnvNull;
    for (int i = 0; i < FNV128size / 2; ++i)
        ctx->Hash[i] = FNV128basis[i];
    ctx->Computed = FNVinited + FNV128state;
    return fnvSuccess;
} /* end FNV128init */

/* initialize context with a provided 16-byte vector basis  (32-bit)
 ******************************************************************/
int FNV128initBasis(FNV128context *const ctx, uint8_t const basis[FNV128size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV128size / 2; ++i) {
        uint32_t temp = *basis++;
        ctx->Hash[i]  = (temp << 8) + *basis++;
    }
    ctx->Computed = FNVinited + FNV128state;
    return fnvSuccess;
} /* end FNV128initBasis */

/* hash in a counted block  (32-bit)
 *****************************************************************/
int FNV128blockin(FNV128context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp[FNV128size / 2];
    uint32_t       temp2[3];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV128state:
        ctx->Computed = FNVcomputed + FNV128state;
        break;
    case FNVcomputed + FNV128state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV128size / 2; ++i)
        temp[i] = ctx->Hash[i];
    for (; length > 0; length--) {
        /* temp = FNV128prime * ( temp ^ *in++ ); */
        temp[FNV128size / 2 - 1] ^= *in++;
        for (i = 2; i >= 0; --i)
            temp2[i] = temp[i + 5] << FNV128shift;
        for (i = 0; i < (FNV128size / 2); ++i)
            temp[i] *= FNV128primeX;
        for (i = 2; i >= 0; --i)
            temp[i] += temp2[i];
        for (i = FNV128size / 2 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 16;
            temp[i] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV128size / 2; ++i)
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
} /* end FNV128blockin */

/* hash in a zero-terminated string not including the zero  (32-bit)
 ******************************************************************/
int FNV128stringin(FNV128context *const ctx, char const *in) {
    uint32_t temp[FNV128size / 2];
    uint32_t temp2[3];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV128state:
        ctx->Computed = FNVcomputed + FNV128state;
        break;
    case FNVcomputed + FNV128state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV128size / 2; ++i)
        temp[i] = ctx->Hash[i];
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV128prime * ( temp ^ *in++ ); */
        temp[FNV128size / 2 - 1] ^= ch;
        for (i = 2; i >= 0; --i)
            temp2[i] = temp[i + 5] << FNV128shift;
        for (i = 0; i < (FNV128size / 2); ++i)
            temp[i] *= FNV128primeX;
        for (i = 2; i >= 0; --i)
            temp[i] += temp2[i];
        for (i = FNV128size / 2 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 16;
            temp[i] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV128size / 2; ++i)
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
} /* end FNV128stringin */

/* return hash  (32-bit)
 ******************************************************************/
int FNV128result(FNV128context *const ctx, uint8_t out[FNV128size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV128state)
        return fnvStateError;
    for (int i = 0; i < FNV128size / 2; ++i) {
        out[2 * i]     = ctx->Hash[i] >> 8;
        out[2 * i + 1] = ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV128state;
    return fnvSuccess;
} /* end FNV128result */

#endif /*  FNV_64bitIntegers */
//******************************************************************
//        END VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//******************************************************************
