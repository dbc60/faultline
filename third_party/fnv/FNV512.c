/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

/* This file implements the FNV (Fowler, Noll, Vo) non-cryptographic
 * hash function FNV-1a for 512-bit hashes.
 */

#include <stdio.h>

#include "fnv-private.h"
#include "FNV512.h"

//*****************************************************************
//  COMMON CODE FOR 64- AND 32-BIT INTEGER MODES
//*****************************************************************

/* FNV512 hash a zero-terminated string not including the zero
 ******************************************************************/
int FNV512string(char const *in, uint8_t out[FNV512size]) {
    FNV512context ctx;
    int           error;

    if ((error = FNV512init(&ctx)))
        return error;
    if ((error = FNV512stringin(&ctx, in)))
        return error;
    return FNV512result(&ctx, out);
} /* end FNV512string */

/* FNV512 hash a zero-terminated string not including the zero
 * with a non-standard basis
 ******************************************************************/
int FNV512stringBasis(char const *in, uint8_t out[FNV512size], uint8_t const basis[FNV512size]) {
    FNV512context ctx;
    int           error;

    if ((error = FNV512initBasis(&ctx, basis)))
        return error;
    if ((error = FNV512stringin(&ctx, in)))
        return error;
    return FNV512result(&ctx, out);
} /* end FNV512stringBasis */

/* FNV512 hash a counted block  (64/32-bit)
 ******************************************************************/
int FNV512block(void const *vin, long int length, uint8_t out[FNV512size]) {
    FNV512context ctx;
    int           error;

    if ((error = FNV512init(&ctx)))
        return error;
    if ((error = FNV512blockin(&ctx, vin, length)))
        return error;
    return FNV512result(&ctx, out);
} /* end FNV512block */

/* FNV512 hash a counted block with a non-standard basis  (64/32-bit)
 ******************************************************************/
int FNV512blockBasis(void const *vin, long int length, uint8_t out[FNV512size], uint8_t const basis[FNV512size]) {
    FNV512context ctx;
    int           error;

    if ((error = FNV512initBasis(&ctx, basis)))
        return error;
    if ((error = FNV512blockin(&ctx, vin, length)))
        return error;
    return FNV512result(&ctx, out);
} /* end FNV512blockBasis */

/* hash the contents of a file
 ******************************************************************/
int FNV512file(char const *fname, uint8_t out[FNV512size]) {
    FNV512context e512Context;
    int           error;

    if (!out)
        return fnvNull;
    if ((error = FNV512init(&e512Context)))
        return error;
    if ((error = FNV512filein(&e512Context, fname)))
        return error;
    return FNV512result(&e512Context, out);
} /* end FNV512file */

/* hash the contents of a file with a non-standard basis
 ******************************************************************/
int FNV512fileBasis(char const *fname, uint8_t out[FNV512size], uint8_t const basis[FNV512size]) {
    FNV512context e512Context;
    int           error;

    if (!out)
        return fnvNull;
    if ((error = FNV512initBasis(&e512Context, basis)))
        return error;
    if ((error = FNV512filein(&e512Context, fname)))
        return error;
    return FNV512result(&e512Context, out);
} /* end FNV512fileBasis */

/* hash in the contents of a file
 ******************************************************************/
int FNV512filein(FNV512context *const e512Context, char const *fname) {
    FILE    *fp;
    long int i;
    char     buf[1024];
    int      error;

    if (!e512Context || !fname)
        return fnvNull;
    switch (e512Context->Computed) {
    case FNVinited + FNV512state:
        e512Context->Computed = FNVcomputed + FNV512state;
        break;
    case FNVcomputed + FNV512state:
        break;
    default:
        return fnvStateError;
    }
    if (fopen_s(&fp, fname, "rb") != 0)
        return fnvBadParam;
    if ((error = FNV512blockin(e512Context, "", 0))) {
        fclose(fp);
        return error;
    }
    while ((i = (long)fread(buf, 1, sizeof(buf), fp)) > 0)
        if ((error = FNV512blockin(e512Context, buf, i))) {
            fclose(fp);
            return error;
        }
    error = ferror(fp);
    fclose(fp);
    if (error)
        return fnvBadParam;
    return fnvSuccess;
} /* end FNV512filein */

//*****************************************************************
//        START VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//*****************************************************************
#ifdef FNV_64bitIntegers

/* 512-bit FNV_prime = 2^344 + 2^8 + 0x57 =
   0x0000000000000000 0000000000000000
     0000000001000000 0000000000000000
     0000000000000000 0000000000000000
     0000000000000000 0000000000000157 */
#define FNV512primeX 0x0157
#define FNV512shift  24

//*****************************************************************
//         Set of init, input, and output functions below
//         to incrementally compute FNV512
//*****************************************************************

/* initialize context  (64-bit)
 ******************************************************************/
int FNV512init(FNV512context *const ctx) {
    uint32_t const FNV512basis[FNV512size / 4]
        = {0xB86DB0B1, 0x171F4416, 0xDCA1E50F, 0x309990AC, 0xAC87D059, 0xC9000000, 0x00000000, 0x00000D21,
           0xE948F68A, 0x34C192F6, 0x2EA79BC9, 0x42DBE7CE, 0x18203641, 0x5F56E34B, 0xAC982AAC, 0x4AFE9FD9};

    if (!ctx)
        return fnvNull;
    for (int i = 0; i < FNV512size / 4; ++i)
        ctx->Hash[i] = FNV512basis[i];
    ctx->Computed = FNVinited + FNV512state;
    return fnvSuccess;
} /* end FNV512init */

/* initialize context with a provided 64-byte vector basis  (64-bit)
 ******************************************************************/
int FNV512initBasis(FNV512context *const ctx, uint8_t const basis[FNV512size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV512size / 4; ++i) {
        uint32_t temp = *basis++ << 24;
        temp += *basis++ << 16;
        temp += *basis++ << 8;
        ctx->Hash[i] = temp + *basis++;
    }
    ctx->Computed = FNVinited + FNV512state;
    return fnvSuccess;
} /* end FNV512initBasis */

/* hash in a counted block  (64-bit)
 ******************************************************************/
int FNV512blockin(FNV512context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint64_t       temp[FNV512size / 4];
    uint64_t       temp2[6];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV512state:
        ctx->Computed = FNVcomputed + FNV512state;
        break;
    case FNVcomputed + FNV512state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV512size / 4; ++i)
        temp[i] = ctx->Hash[i]; // copy into temp
    for (; length > 0; length--) {
        /* temp = FNV512prime * ( temp ^ *in++ ); */
        temp[FNV512size / 4 - 1] ^= *in++;
        for (i = 0; i < 6; ++i)
            temp2[5 - i] = temp[FNV512size / 4 - 1 - i] << FNV512shift;
        for (i = 0; i < FNV512size / 4; ++i)
            temp[i] *= FNV512primeX;
        for (i = 0; i < 6; ++i)
            temp[i] += temp2[i];
        for (i = FNV512size / 4 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 32; // propagate carries
            temp[i] &= 0xFFFFFFFF;
        }
    } /* end for length */
    for (i = 0; i < FNV512size / 4; ++i)
        ctx->Hash[i] = (uint32_t)temp[i]; // store back into hash
    return fnvSuccess;
} /* end FNV512blockin */

/* hash in a zero-terminated string not including the zero  (64-bit)
 ******************************************************************/
int FNV512stringin(FNV512context *const ctx, char const *in) {
    uint64_t temp[FNV512size / 4];
    uint64_t temp2[6];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV512state:
        ctx->Computed = FNVcomputed + FNV512state;
        break;
    case FNVcomputed + FNV512state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV512size / 4; ++i)
        temp[i] = ctx->Hash[i]; // copy into temp
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV512prime * ( temp ^ ch ); */
        temp[FNV512size / 4 - 1] ^= ch;
        for (i = 0; i < 6; ++i)
            temp2[5 - i] = temp[FNV512size / 4 - 1 - i] << FNV512shift;
        for (i = 0; i < FNV512size / 4; ++i)
            temp[i] *= FNV512primeX;
        for (i = 0; i < 6; ++i)
            temp[i] += temp2[i];
        for (i = FNV512size / 4 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 32; // propagate carries
            temp[i] &= 0xFFFFFFFF;
        }
    }
    for (i = 0; i < FNV512size / 4; ++i)
        ctx->Hash[i] = (uint32_t)temp[i]; // store back into hash
    return fnvSuccess;
} /* end FNV512stringin */

/* return hash  (64-bit)
 ******************************************************************/
int FNV512result(FNV512context *const ctx, uint8_t out[FNV512size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV512state)
        return fnvStateError;
    for (int i = 0; i < FNV512size / 4; ++i) {
        out[4 * i]     = ctx->Hash[i] >> 24;
        out[4 * i + 1] = (uint8_t)(ctx->Hash[i] >> 16);
        out[4 * i + 2] = (uint8_t)(ctx->Hash[i] >> 8);
        out[4 * i + 3] = (uint8_t)ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV512state;
    return fnvSuccess;
} /* end FNV512result */

//*****************************************************************
//        END VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//*****************************************************************
#else /*  FNV_64bitIntegers */
//*****************************************************************
//      START VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//*****************************************************************

/* 512-bit FNV_prime = 2^344 + 2^8 + 0x57 =
   0x00000000 00000000 00000000 00000000
     00000000 01000000 00000000 00000000
     00000000 00000000 00000000 00000000
     00000000 00000000 00000000 00000157 */
#define FNV512primeX 0x0157
#define FNV512shift  8

//*****************************************************************
//         Set of init, input, and output functions below
//         to incrementally compute FNV512
//*****************************************************************

/* initialize context  (32-bit)
 ******************************************************************/
int FNV512init(FNV512context *const ctx) {
    uint16_t const FNV512basis[FNV512size / 2]
        = {0xB86D, 0xB0B1, 0x171F, 0x4416, 0xDCA1, 0xE50F, 0x3099, 0x90AC, 0xAC87, 0xD059, 0xC900,
           0x0000, 0x0000, 0x0000, 0x0000, 0x0D21, 0xE948, 0xF68A, 0x34C1, 0x92F6, 0x2EA7, 0x9BC9,
           0x42DB, 0xE7CE, 0x1820, 0x3641, 0x5F56, 0xE34B, 0xAC98, 0x2AAC, 0x4AFE, 0x9FD9};

    if (!ctx)
        return fnvNull;
    for (int i = 0; i < FNV512size / 2; ++i)
        ctx->Hash[i] = FNV512basis[i];
    ctx->Computed = FNVinited + FNV512state;
    return fnvSuccess;
} /* end FNV512init */

/* initialize context with a provided 64-byte vector basis  (32-bit)
 ******************************************************************/
int FNV512initBasis(FNV512context *const ctx, uint8_t const basis[FNV512size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV512size / 2; ++i) {
        uint32_t temp = *basis++;
        ctx->Hash[i]  = (temp << 8) + *basis++;
    }
    ctx->Computed = FNVinited + FNV512state;
    return fnvSuccess;
} /* end FNV512initBasis */

/* hash in a counted block  (32-bit)
 ******************************************************************/
int FNV512blockin(FNV512context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp[FNV512size / 2];
    uint32_t       temp2[11];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV512state:
        ctx->Computed = FNVcomputed + FNV512state;
        break;
    case FNVcomputed + FNV512state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV512size / 2; ++i)
        temp[i] = ctx->Hash[i]; // copy into temp
    for (; length > 0; length--) {
        /* temp = FNV512prime * ( temp ^ *in++ ); */
        temp[FNV512size / 2 - 1] ^= *in++;
        for (i = 0; i < 11; ++i)
            temp2[10 - i] = temp[FNV512size / 2 - 1 - i] << FNV512shift;
        for (i = 0; i < FNV512size / 2; ++i)
            temp[i] *= FNV512primeX;
        for (i = 0; i < 11; ++i)
            temp[i] += temp2[i];
        for (i = FNV512size / 2 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 16; // propagate carries
            temp[i] &= 0xFFFF;
        }
    } /* end for length */
    for (i = 0; i < FNV512size / 2; ++i)
        ctx->Hash[i] = (uint16_t)temp[i]; // store back into hash
    return fnvSuccess;
} /* end FNV512blockin */

/* hash in a zero-terminated string not including the zero  (32-bit)
 ******************************************************************/
int FNV512stringin(FNV512context *const ctx, char const *in) {
    uint32_t temp[FNV512size / 2];
    uint32_t temp2[11];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV512state:
        ctx->Computed = FNVcomputed + FNV512state;
        break;
    case FNVcomputed + FNV512state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV512size / 2; ++i)
        temp[i] = ctx->Hash[i]; // copy into temp
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV512prime * ( temp ^ *in++ ); */
        temp[FNV512size / 2 - 1] ^= ch;
        for (i = 0; i < 11; ++i)
            temp2[10 - i] = temp[FNV512size / 2 - 1 - i] << FNV512shift;
        for (i = 0; i < FNV512size / 2; ++i)
            temp[i] *= FNV512primeX;
        for (i = 0; i < 11; ++i)
            temp[i] += temp2[i];
        for (i = FNV512size / 2 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 16; // propagate carries
            temp[i] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV512size / 2; ++i)
        ctx->Hash[i] = temp[i]; // store back into hash
    return fnvSuccess;
} /* end FNV512stringin */

/* return hash  (32-bit)
 ******************************************************************/
int FNV512result(FNV512context *const ctx, uint8_t out[FNV512size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV512state)
        return fnvStateError;
    for (int i = 0; i < FNV512size / 2; ++i) {
        out[2 * i]     = ctx->Hash[i] >> 8;
        out[2 * i + 1] = ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV512state;
    return fnvSuccess;
} /* end FNV512result */

#endif /*  FNV_64bitIntegers */

//*****************************************************************
//        END VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//*****************************************************************
