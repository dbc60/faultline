#ifndef _FNV_PRIVATE_H_
#define _FNV_PRIVATE_H_

/* Copyright (c) 2016, 2023, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * *  Neither the name of Internet Society, IETF or IETF Trust, nor
 *    the names of specific contributors, may be used to endorse or
 *    promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *      Six FNV-1a hashes are defined with these sizes:
 *              FNV32          32 bits, 4 bytes
 *              FNV64          64 bits, 8 bytes
 *              FNV128         128 bits, 16 bytes
 *              FNV256         256 bits, 32 bytes
 *              FNV512         512 bits, 64 bytes
 *              FNV1024        1024 bits, 128 bytes
 */

/* Private stuff used by this implementation of the FNV
 * (Fowler, Noll, Vo) non-cryptographic hash function FNV-1a.
 * External callers don't need to know any of this.  */

enum {  /* State value bases for context->Computed */
    FNVinited   = 22,
    FNVcomputed = 76,
    FNVemptied  = 220,
    FNVclobber  = 122 /* known bad value for testing */
};

/* Deltas to assure distinct state values for different lengths */
enum {
    FNV32state   = 1,
    FNV64state   = 3,
    FNV128state  = 5,
    FNV256state  = 7,
    FNV512state  = 11,
    FNV1024state = 13
};

#endif /* _FNV_PRIVATE_H_ */
