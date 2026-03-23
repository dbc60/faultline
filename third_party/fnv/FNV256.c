/* Copyright (c) 2016, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

/* This file implements the FNV (Fowler, Noll, Vo) non-cryptographic
 * hash function FNV-1a for 256-bit hashes.
 */

#include <stdio.h>

#include "fnv-private.h"
#include "FNV256.h"

//*****************************************************************
//  COMMON CODE FOR 64- AND 32-BIT INTEGER MODES
//*****************************************************************

/* FNV256 hash a zero-terminated string not including the zero
 ******************************************************************/
int FNV256string(char const *in, uint8_t out[FNV256size]) {
    FNV256context ctx;
    int           error;

    if ((error = FNV256init(&ctx)))
        return error;
    if ((error = FNV256stringin(&ctx, in)))
        return error;
    return FNV256result(&ctx, out);
} /* end FNV256string */

/* FNV256 hash a zero-terminated string not including the zero
 * with a non-standard basis
 ******************************************************************/
int FNV256stringBasis(char const *in, uint8_t out[FNV256size], uint8_t const basis[FNV256size]) {
    FNV256context ctx;
    int           error;

    if ((error = FNV256initBasis(&ctx, basis)))
        return error;
    if ((error = FNV256stringin(&ctx, in)))
        return error;
    return FNV256result(&ctx, out);
} /* end FNV256stringBasis */

/* FNV256 hash a counted block  (64/32-bit)
 ******************************************************************/
int FNV256block(void const *vin, long int length, uint8_t out[FNV256size]) {
    FNV256context ctx;
    int           error;

    if ((error = FNV256init(&ctx)))
        return error;
    if ((error = FNV256blockin(&ctx, vin, length)))
        return error;
    return FNV256result(&ctx, out);
} /* end FNV256block */

/* FNV256 hash a counted block  (64/32-bit)
 * with a non-standard basis
 ******************************************************************/
int FNV256blockBasis(void const *vin, long int length, uint8_t out[FNV256size], uint8_t const basis[FNV256size]) {
    FNV256context ctx;
    int           error;

    if ((error = FNV256initBasis(&ctx, basis)))
        return error;
    if ((error = FNV256blockin(&ctx, vin, length)))
        return error;
    return FNV256result(&ctx, out);
} /* end FNV256blockBasis */

/* hash the contents of a file
 ******************************************************************/
int FNV256file(char const *fname, uint8_t out[FNV256size]) {
    FNV256context e256Context;
    int           error;

    if (!out)
        return fnvNull;
    if ((error = FNV256init(&e256Context)))
        return error;
    if ((error = FNV256filein(&e256Context, fname)))
        return error;
    return FNV256result(&e256Context, out);
} /* end FNV256file */

/* hash the contents of a file with a non-standard basis
 ******************************************************************/
int FNV256fileBasis(char const *fname, uint8_t out[FNV256size], uint8_t const basis[FNV256size]) {
    FNV256context e256Context;
    int           error;

    if (!out)
        return fnvNull;
    if ((error = FNV256initBasis(&e256Context, basis)))
        return error;
    if ((error = FNV256filein(&e256Context, fname)))
        return error;
    return FNV256result(&e256Context, out);
} /* end FNV256fileBasis */

/* hash in the contents of a file
 ******************************************************************/
int FNV256filein(FNV256context *const e256Context, char const *fname) {
    FILE    *fp;
    long int i;
    char     buf[1024];
    int      error;

    if (!e256Context || !fname)
        return fnvNull;
    switch (e256Context->Computed) {
    case FNVinited + FNV256state:
        e256Context->Computed = FNVcomputed + FNV256state;
        break;
    case FNVcomputed + FNV256state:
        break;
    default:
        return fnvStateError;
    }
    if (fopen_s(&fp, fname, "rb") != 0)
        return fnvBadParam;
    if ((error = FNV256blockin(e256Context, "", 0))) {
        fclose(fp);
        return error;
    }
    while ((i = (long)fread(buf, 1, sizeof(buf), fp)) > 0)
        if ((error = FNV256blockin(e256Context, buf, i))) {
            fclose(fp);
            return error;
        }
    error = ferror(fp);
    fclose(fp);
    if (error)
        return fnvBadParam;
    return fnvSuccess;
} /* end FNV256filein */

//*****************************************************************
//        START VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//*****************************************************************
#ifdef FNV_64bitIntegers

/* 256-bit FNV_prime = 2^168 + 2^8 + 0x63 */
/* 0x0000000000000000 0000010000000000
     0000000000000000 0000000000000163 */
#define FNV256primeX 0x0163
#define FNV256shift  8

//*****************************************************************
//         Set of init, input, and output functions below
//         to incrementally compute FNV256
//*****************************************************************

/* initialize context  (64-bit)
 ******************************************************************/
int FNV256init(FNV256context *const ctx) {
    uint32_t const FNV256basis[FNV256size / 4]
        = {0xDD268DBC, 0xAAC55036, 0x2D98C384, 0xC4E576CC, 0xC8B15368, 0x47B6BBB3, 0x1023B4C8, 0xCAEE0535};

    if (!ctx)
        return fnvNull;
    for (int i = 0; i < FNV256size / 4; ++i)
        ctx->Hash[i] = FNV256basis[i];
    ctx->Computed = FNVinited + FNV256state;
    return fnvSuccess;
} /* end FNV256init */

/* initialize context with a provided 32-byte vector basis  (64-bit)
 * with a non-standard basis
 ******************************************************************/
int FNV256initBasis(FNV256context *const ctx, uint8_t const basis[FNV256size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV256size / 4; ++i) {
        uint32_t temp = *basis++ << 24;
        temp += *basis++ << 16;
        temp += *basis++ << 8;
        ctx->Hash[i] = temp + *basis++;
    }
    ctx->Computed = FNVinited + FNV256state;
    return fnvSuccess;
} /* end FNV256initBasis */

/* hash in a counted block  (64-bit)
 ******************************************************************/
int FNV256blockin(FNV256context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint64_t       temp[FNV256size / 4];
    uint64_t       temp2[3];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV256state:
        ctx->Computed = FNVcomputed + FNV256state;
        break;
    case FNVcomputed + FNV256state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV256size / 4; ++i)
        temp[i] = ctx->Hash[i];
    for (; length > 0; length--) {
        /* temp = FNV256prime * ( temp ^ *in++ ); */
        temp[FNV256size / 4 - 1] ^= *in++;
        for (i = 2; i >= 0; --i)
            temp2[i] = temp[i + 5] << FNV256shift;
        for (i = 0; i < FNV256size / 4; ++i)
            temp[i] *= FNV256primeX;
        for (i = 2; i >= 0; --i)
            temp[i] += temp2[i];
        for (i = FNV256size / 4 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 32;
            temp[i] &= 0xFFFFFFFF;
        }
    }
    for (i = 0; i < FNV256size / 4; ++i)
        ctx->Hash[i] = (uint32_t)temp[i];
    return fnvSuccess;
} /* end FNV256blockin */

/* hash in a zero-terminated string not including the zero  (64-bit)
 ******************************************************************/
int FNV256stringin(FNV256context *const ctx, char const *in) {
    uint64_t temp[FNV256size / 4];
    uint64_t temp2[3];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV256state:
        ctx->Computed = FNVcomputed + FNV256state;
        break;
    case FNVcomputed + FNV256state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV256size / 4; ++i)
        temp[i] = ctx->Hash[i];
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV256prime * ( temp ^ ch ); */
        temp[FNV256size / 4 - 1] ^= ch;
        for (i = 2; i >= 0; --i)
            temp2[i] = temp[i + 5] << FNV256shift;
        for (i = 0; i < FNV256size / 4; ++i)
            temp[i] *= FNV256primeX;
        for (i = 2; i >= 0; --i)
            temp[i] += temp2[i];
        for (i = FNV256size / 4 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 32;
            temp[i] &= 0xFFFFFFFF;
        }
    }
    for (i = 0; i < FNV256size / 4; ++i)
        ctx->Hash[i] = (uint32_t)temp[i];
    return fnvSuccess;
} /* end FNV256stringin */

/* return hash as 8-byte vector  (64-bit)
 ******************************************************************/
int FNV256result(FNV256context *const ctx, uint8_t out[FNV256size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV256state)
        return fnvStateError;
    for (int i = 0; i < FNV256size / 4; ++i) {
        out[4 * i]     = ctx->Hash[i] >> 24;
        out[4 * i + 1] = (uint8_t)(ctx->Hash[i] >> 16);
        out[4 * i + 2] = (uint8_t)(ctx->Hash[i] >> 8);
        out[4 * i + 3] = (uint8_t)ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV256state;
    return fnvSuccess;
} /* end FNV256result */

//****************************************************************
//       END VERSION FOR WHEN YOU HAVE 64-BIT ARITHMETIC
//****************************************************************
#else /*  FNV_64bitIntegers */
//****************************************************************
//       START VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//****************************************************************

/* version for when you only have 32-bit arithmetic
 *****************************************************************/

/* 256-bit FNV_prime = 2^168 + 2^8 + 0x63 */
/* 0x00000000 00000000 00000100 00000000
     00000000 00000000 00000000 00000163 */
#define FNV256primeX 0x0163
#define FNV256shift  8

//****************************************************************
//       Set of init, input, and output functions below
//       to incrementally compute FNV256
//****************************************************************

/* initialize context  (32-bit)
 *****************************************************************/
int FNV256init(FNV256context *const ctx) {
    uint16_t const FNV256basis[FNV256size / 2] = {0xDD26, 0x8DBC, 0xAAC5, 0x5036, 0x2D98, 0xC384, 0xC4E5, 0x76CC,
                                                  0xC8B1, 0x5368, 0x47B6, 0xBBB3, 0x1023, 0xB4C8, 0xCAEE, 0x0535};

    if (!ctx)
        return fnvNull;
    for (int i = 0; i < FNV256size / 2; ++i)
        ctx->Hash[i] = FNV256basis[i];
    ctx->Computed = FNVinited + FNV256state;
    return fnvSuccess;
} /* end FNV256init */

/* initialize context with a provided 32-byte vector basis  (32-bit)
 *****************************************************************/
int FNV256initBasis(FNV256context *const ctx, uint8_t const basis[FNV256size]) {
    if (!ctx || !basis)
        return fnvNull;
    for (int i = 0; i < FNV256size / 2; ++i) {
        uint32_t temp = *basis++;
        ctx->Hash[i]  = (temp << 8) + (*basis++);
    }
    ctx->Computed = FNVinited + FNV256state;
    return fnvSuccess;
} /* end FNV256initBasis */

/* hash in a counted block  (32-bit)
 *****************************************************************/
int FNV256blockin(FNV256context *const ctx, void const *vin, long int length) {
    uint8_t const *in = (uint8_t const *)vin;
    uint32_t       temp[FNV256size / 2];
    uint32_t       temp2[6];
    int            i;

    if (!ctx || !in)
        return fnvNull;
    if (length < 0)
        return fnvBadParam;
    switch (ctx->Computed) {
    case FNVinited + FNV256state:
        ctx->Computed = FNVcomputed + FNV256state;
        break;
    case FNVcomputed + FNV256state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV256size / 2; ++i)
        temp[i] = ctx->Hash[i];
    for (; length > 0; length--) {
        /* temp = FNV256prime * ( temp ^ *in++ ); */
        temp[FNV256size / 2 - 1] ^= *in++;
        for (i = 0; i < 6; ++i)
            temp2[5 - i] = temp[FNV256size / 2 - 1 - i] << FNV256shift;
        for (i = 0; i < FNV256size / 2; ++i)
            temp[i] *= FNV256primeX;
        for (i = 0; i < 6; ++i)
            temp[i] += temp2[i];
        for (i = FNV256size / 2 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 16;
            temp[i] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV256size / 2; ++i)
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
} /* end FNV256blockin */

/* hash in a zero-terminated string not including the zero  (32-bit)
 *****************************************************************/
int FNV256stringin(FNV256context *const ctx, char const *in) {
    uint32_t temp[FNV256size / 2];
    uint32_t temp2[6];
    int      i;
    uint8_t  ch;

    if (!ctx || !in)
        return fnvNull;
    switch (ctx->Computed) {
    case FNVinited + FNV256state:
        ctx->Computed = FNVcomputed + FNV256state;
        break;
    case FNVcomputed + FNV256state:
        break;
    default:
        return fnvStateError;
    }
    for (i = 0; i < FNV256size / 2; ++i)
        temp[i] = ctx->Hash[i];
    while ((ch = (uint8_t)*in++)) {
        /* temp = FNV256prime * ( temp ^ *in++ ); */
        temp[FNV256size / 2 - 1] ^= ch;
        for (i = 0; i < 6; ++i)
            temp2[5 - i] = temp[FNV256size / 2 - 1 - i] << FNV256shift;
        for (i = 0; i < FNV256size / 2; ++i)
            temp[i] *= FNV256primeX;
        for (i = 0; i < 6; ++i)
            temp[i] += temp2[i];
        for (i = FNV256size / 2 - 1; i > 0; --i) {
            temp[i - 1] += temp[i] >> 16;
            temp[i] &= 0xFFFF;
        }
    }
    for (i = 0; i < FNV256size / 2; ++i)
        ctx->Hash[i] = temp[i];
    return fnvSuccess;
} /* end FNV256stringin */

/* return hash  (32-bit)
 *****************************************************************/
int FNV256result(FNV256context *const ctx, uint8_t out[FNV256size]) {
    if (!ctx || !out)
        return fnvNull;
    if (ctx->Computed != FNVcomputed + FNV256state)
        return fnvStateError;
    for (int i = 0; i < FNV256size / 2; ++i) {
        out[2 * i]     = ctx->Hash[i] >> 8;
        out[2 * i + 1] = ctx->Hash[i];
        ctx->Hash[i]   = 0;
    }
    ctx->Computed = FNVemptied + FNV256state;
    return fnvSuccess;
} /* end FNV256result */

#endif /*  FNV_64bitIntegers */
//****************************************************************
//        END VERSION FOR WHEN YOU ONLY HAVE 32-BIT ARITHMETIC
//****************************************************************
