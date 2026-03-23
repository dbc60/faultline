#ifndef _FNV_ErrCodes_
#define _FNV_ErrCodes_

/* Copyright (c) 2016, 2023, 2024, 2025 IETF Trust and the persons
 * identified as authors of the code.  All rights reserved.
 * See fnv-private.h for terms of use and redistribution.
 */

//****************************************************************//
//
//  All FNV functions, except the FNVxxxfile functions,
//    return an integer as follows:
//       0 -> success
//      >0 -> error as listed below
//
enum { /* success and errors */
    fnvSuccess = 0,
    fnvNull,       // 1 Null pointer parameter
    fnvStateError, // 2 called Input after Result or before Init
    fnvBadParam    // 3 passed a bad parameter
};
#endif /* _FNV_ErrCodes_ */
