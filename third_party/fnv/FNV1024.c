/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

/* This file implements the FNV (Fowler, Noll, Vo) non-cryptographic
 * hash function FNV-1a for 1024-bit hashes.
 */

#include <stdio.h>

#include "fnv-private.h"
#include "FNV1024.h"

//*****************************************************************
//  COMMON CODE FOR 64- AND 32-BIT INTEGER MODES
//*****************************************************************

/* FNV1024 hash a zero-terminated string not including the zero
 ******************************************************************/
int FNV1024string(char const *in, uint8_t out[FNV1024size]) {
    FNV1024context ctx;
    int            error;

    if ((error = FNV1024init(&ctx)))
        return error;
    if ((error = FNV1024stringin(&ctx, in)))
        return error;
    return FNV1024result(&ctx, out);
} /* end FNV1024string */

/* FNV1024 hash a zero-terminated string not including the zero
 * with a non-standard basis
 ******************************************************************/
int FNV1024stringBasis(char const *in, uint8_t out[FNV1024size], uint8_t const basis[FNV1024size]) {
    FNV1024context ctx;
    int            error;

    if ((error = FNV1024initBasis(&ctx, basis)))
        return error;
    if ((error = FNV1024stringin(&ctx, in)))
        return error;
    return FNV1024result(&ctx, out);
} /* end FNV1024stringBasis */

/* FNV1024 hash a counted block  (64/32-bit)
 ******************************************************************/
int FNV1024block(void const *vin, long int length, uint8_t out[FNV1024size]) {
    FNV1024context ctx;
    int            error;

    if ((error = FNV1024init(&ctx)))
        return error;
    if ((error = FNV1024blockin(&ctx, vin, length)))
        return error;
    return FNV1024result(&ctx, out);
} /* end FNV1024block */

/* FNV1024 hash a counted block  (64/32-bit)
 * with a non-standard basis
 ******************************************************************/
int FNV1024blockBasis(void const *vin, long int length, uint8_t out[FNV1024size], uint8_t const basis[FNV1024size]) {
    FNV1024context ctx;
    int            error;

    if ((error = FNV1024initBasis(&ctx, basis)))
        return error;
    if ((error = FNV1024blockin(&ctx, vin, length)))
        return error;
    return FNV1024result(&ctx, out);
} /* end FNV1024blockBasis */

/* hash the contents of a file
 ******************************************************************/
int FNV1024file(char const *fname, uint8_t out[FNV1024size]) {
    FNV1024context e1024Context;
    int            error;

    if (!out)
        return fnvNull;
    if ((error = FNV1024init(&e1024Context)))
        return error;
    if ((error = FNV1024filein(&e1024Context, fname)))
        return error;
    return FNV1024result(&e1024Context, out);
} /* end FNV1024file */

/* hash the contents of a file with a non-standard basis
 ******************************************************************/
int FNV1024fileBasis(char const *fname, uint8_t out[FNV1024size], uint8_t const basis[FNV1024size]) {
    FNV1024context e1024Context;
    int            error;

    if (!out)
        return fnvNull;
    if ((error = FNV1024initBasis(&e1024Context, basis)))
        return error;
    if ((error = FNV1024filein(&e1024Context, fname)))
        return error;
    return FNV1024result(&e1024Context, out);
} /* end FMV1024fileBasis */

/* hash in the contents of a file
 ******************************************************************/
int FNV1024filein(FNV1024context *const e1024Context, char const *fname) {
    FILE    *fp;
    long int i;
    char     buf[1024];
    int      error;

    if (!e1024Context || !fname)
        return fnvNull;
    switch (e1024Context->Computed) {
    case FNVinited + FNV1024state:
        e1024Context->Computed = FNVcomputed + FNV1024state;
        break;
    case FNVcomputed + FNV1024state:
        break;
    default:
        return fnvStateError;
    }
    if (fopen_s(&fp, fname, "rb") != 0)
        return fnvBadParam;
    if ((error = FNV1024blockin(e1024Context, "", 0))) {
        fclose(fp);
        return error;
    }
    while ((i = (long)fread(buf, 1, sizeof(buf), fp)) > 0)
        if ((error = FNV1024blockin(e1024Context, buf, i))) {
            fclose(fp);
            return error;
        }
    error = ferror(fp);
    fclose(fp);
    if (error)
        return fnvBadParam;
    return fnvSuccess;
} /* end FNV1024filein */

//****************************************************************//
// START VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//****************************************************************//
#ifdef FNV_64bitIntegers

/* 1024-bit FNV_prime = 2^680 + 2^8 + 0x8d =
   0x0000000000000000 0000000000000000
     0000000000000000 0000000000000000
     0000000000000000 0000010000000000
     0000000000000000 0000000000000000
     0000000000000000 0000000000000000
     0000000000000000 0000000000000000
     0000000000000000 0000000000000000
     0000000000000000 000000000000018D */
#define FNV1024primeX 0x018D
#define FNV1024shift  8

//***************************************************************//
//         Set of init, input, and output functions below
//         to incrementally compute FNV1024
//**************************************************************//

/* initialize context  (64-bit)
 ******************************************************************/
int FNV1024init(FNV1024context *const ctx) {
    uint32_t const FNV1024basis[FNV1024size / 4]
        = {0x00000000, 0x00000000, 0x005F7A76, 0x758ECC4D, 0x32E56D5A, 0x591028B7, 0x4B29FC42, 0x23FDADA1,
           0x6C3BF34E, 0xDA3674DA, 0x9A21D900, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
           0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0004C6D7,
           0xEB6E7380, 0x2734510A, 0x555F256C, 0xC005AE55, 0x6BDE8CC9, 0xC6A93B21, 0xAFF4B16C, 0x71EE90B3};

    if (!ctx)
        return fnvNull;
    for (int i = 0; i < FNV1024size / 4; ++i)
        ctx->Hash[i] = FNV1024basis[i];
    ctx->Computed = FNVinited + FNV1024state;
    return fnvSuccess;
} /* end FNV1024init */

/* initialize context with a provided 128-byte vector basis  (64-bit)
 ******************************************************************/
int FNV1024initBasis(FNV1024context *const ctx, uint8_t const basis[FNV1024size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV1024size / 4; ++i) {
        uint32_t temp = *basis++ << 24;
        temp += *basis++ << 16;
        temp += *basis++ << 8;
        ctx->Hash[i] = temp + *basis++;
    }
    ctx->Computed = FNVinited + FNV1024state;
    return fnvSuccess;
} /* end FNV1024initBasis */

/* hash in a counted block  (64-bit)
 ******************************************************************/
int FNV1024blockin(FNV1024context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint64_t       temp[FNV1024size / 4];
    uint64_t       temp2[11];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV1024state:
        ctx->Computed = FNVcomputed + FNV1024state;
        break;
    case FNVcomputed + FNV1024state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV1024size / 4; ++i)
        temp[i] = ctx->Hash[i]; // copy into temp
    for (; length > 0; length--) {
        /* temp = FNV1024prime * ( temp ^ *in++ ); */
        temp[FNV1024size / 4 - 1] ^= *in++;
        for (i = 0; i < 11; ++i)
            temp2[10 - i] = temp[FNV1024size / 4 - 1 - i] << FNV1024shift;
        for (i = 0; i < FNV1024size / 4; ++i)
            temp[i] *= FNV1024primeX;
        for (i = 0; i < 11; ++i)
            temp[i] += temp2[i];
        for (i = FNV1024size / 4 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 32; // propagate carries
            temp[i] &= 0xFFFFFFFF;
        }
    } /* end for length */
    for (i = 0; i < FNV1024size / 4; ++i)
        ctx->Hash[i] = (uint32_t)temp[i]; // store back into hash
    return fnvSuccess;
} /* end FNV1024blockin */

/* hash in a zero-terminated string not including the zero  (64-bit)
 ******************************************************************/
int FNV1024stringin(FNV1024context *const ctx, char const *in) {
    uint64_t temp[FNV1024size / 4];
    uint64_t temp2[11];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV1024state:
        ctx->Computed = FNVcomputed + FNV1024state;
        break;
    case FNVcomputed + FNV1024state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV1024size / 4; ++i)
        temp[i] = ctx->Hash[i]; // copy into temp
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV1024prime * ( temp ^ ch ); */
        temp[FNV1024size / 4 - 1] ^= ch;
        for (i = 0; i < 11; ++i)
            temp2[10 - i] = temp[FNV1024size / 4 - 1 - i] << FNV1024shift;
        for (i = 0; i < FNV1024size / 4; ++i)
            temp[i] *= FNV1024primeX;
        for (i = 0; i < 11; ++i)
            temp[i] += temp2[i];
        for (i = FNV1024size / 4 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 32;
            temp[i] &= 0xFFFFFFFF;
        }
    }
    for (i = 0; i < FNV1024size / 4; ++i)
        ctx->Hash[i] = (uint32_t)temp[i]; // store back into hash
    return fnvSuccess;
} /* end FNV1024stringin */

/* return hash  (64-bit)
 ******************************************************************/
int FNV1024result(FNV1024context *const ctx, uint8_t out[FNV1024size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV1024state)
        return fnvStateError;
    for (int i = 0; i < FNV1024size / 4; ++i) {
        out[4 * i]     = ctx->Hash[i] >> 24;
        out[4 * i + 1] = (uint8_t)(ctx->Hash[i] >> 16);
        out[4 * i + 2] = (uint8_t)(ctx->Hash[i] >> 8);
        out[4 * i + 3] = (uint8_t)ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV1024state;
    return fnvSuccess;
} /* end FNV1024result */

//****************************************************************//
//        END VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//***************************************************************//
#else /*  FNV_64bitIntegers */
//***************************************************************//
//      START VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//***************************************************************//

/* version for when you only have 32-bit arithmetic
 ******************************************************************/

/*
 1024-bit FNV_prime = 2^680 + 2^8 + 0x8d =
   0x00000000 00000000 00000000 00000000
     00000000 00000000 00000000 00000000
     00000000 00000000 00000100 00000000
     00000000 00000000 00000000 00000000
     00000000 00000000 00000000 00000000
     00000000 00000000 00000000 00000000
     00000000 00000000 00000000 00000000
     00000000 00000000 00000000 0000018D */
#define FNV1024primeX 0x018D
#define FNV1024shift  8

//*****************************************************************
//         Set of init, input, and output functions below
//         to incrementally compute FNV1024
//*****************************************************************

/* initialize context  (32-bit)
 ******************************************************************/
int FNV1024init(FNV1024context *const ctx) {
    uint16_t const FNV1024basis[FNV1024size / 2]
        = {0x0000, 0x0000, 0x0000, 0x0000, 0x005F, 0x7A76, 0x758E, 0xCC4D, 0x32E5, 0x6D5A, 0x5910, 0x28B7, 0x4B29,
           0xFC42, 0x23FD, 0xADA1, 0x6C3B, 0xF34E, 0xDA36, 0x74DA, 0x9A21, 0xD900, 0x0000, 0x0000, 0x0000, 0x0000,
           0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
           0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0004, 0xC6D7, 0xEB6E, 0x7380, 0x2734, 0x510A,
           0x555F, 0x256C, 0xC005, 0xAE55, 0x6BDE, 0x8CC9, 0xC6A9, 0x3B21, 0xAFF4, 0xB16C, 0x71EE, 0x90B3};

    if (!ctx)
        return fnvNull;
    for (int i = 0; i < FNV1024size / 2; ++i)
        ctx->Hash[i] = FNV1024basis[i];
    ctx->Computed = FNVinited + FNV1024state;
    return fnvSuccess;
} /* end FNV1024init */

/* initialize context with a provided 128-byte vector basis  (32-bit)
 ******************************************************************/
int FNV1024initBasis(FNV1024context *const ctx, uint8_t const basis[FNV1024size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV1024size / 2; ++i) {
        uint32_t temp = *basis++;
        ctx->Hash[i]  = (temp << 8) + *basis++;
    }
    ctx->Computed = FNVinited + FNV1024state;
    return fnvSuccess;
} /* end FNV1024initBasis */

/* hash in a counted block  (32-bit)
 ******************************************************************/
int FNV1024blockin(FNV1024context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp[FNV1024size / 2];
    uint32_t       temp2[22];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV1024state:
        ctx->Computed = FNVcomputed + FNV1024state;
        break;
    case FNVcomputed + FNV1024state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV1024size / 2; ++i)
        temp[i] = ctx->Hash[i]; // copy into temp
    for (; length > 0; length--) {
        /* temp = FNV1024prime * ( temp ^ *in++ ); */
        temp[FNV1024size / 2 - 1] ^= *in++;
        for (i = 0; i < 22; ++i)
            temp2[21 - i] = temp[FNV1024size / 2 - 1 - i] << FNV1024shift;
        for (i = 0; i < FNV1024size / 2; ++i)
            temp[i] *= FNV1024primeX;
        for (i = 0; i < 22; ++i)
            temp[i] += temp2[i];
        for (i = FNV1024size / 2 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 16; // propagate carries
            temp[i] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV1024size / 2; ++i)
        ctx->Hash[i] = temp[i]; // store back into hash
    return fnvSuccess;
} /* end FNV1024blockin */

/* hash in a zero-terminated string not including the zero  (32-bit)
 ******************************************************************/
int FNV1024stringin(FNV1024context *const ctx, char const *in) {
    uint32_t temp[FNV1024size / 2];
    uint32_t temp2[22];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV1024state:
        ctx->Computed = FNVcomputed + FNV1024state;
        break;
    case FNVcomputed + FNV1024state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV1024size / 2; ++i)
        temp[i] = ctx->Hash[i]; // copy into temp
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV1024prime * ( temp ^ *in++ ); */
        temp[FNV1024size / 2 - 1] ^= ch;
        for (i = 0; i < 22; ++i)
            temp2[21 - i] = temp[FNV1024size / 2 - 1 - i] << FNV1024shift;
        for (i = 0; i < FNV1024size / 2; ++i)
            temp[i] *= FNV1024primeX;
        for (i = 0; i < 22; ++i)
            temp[i] += temp2[i];
        for (i = FNV1024size / 2 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 16; // propagate carries
            temp[i] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV1024size / 2; ++i)
        ctx->Hash[i] = temp[i]; // store back into hash
    return fnvSuccess;
} /* end FNV1024stringin */

/* return hash  (32-bit)
 ******************************************************************/
int FNV1024result(FNV1024context *const ctx, uint8_t out[FNV1024size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV1024state)
        return fnvStateError;
    for (int i = 0; i < FNV1024size / 2; ++i) {
        out[2 * i]     = ctx->Hash[i] >> 8;
        out[2 * i + 1] = ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV1024state;
    return fnvSuccess;
} /* end FNV1024result */

#endif /*  FNV_64bitIntegers */
//****************************************************************//
//        END VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//****************************************************************//
