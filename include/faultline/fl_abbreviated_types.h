#ifndef FL_ABBREVIATED_TYPES_H_
#define FL_ABBREVIATED_TYPES_H_

/**
 * @file fl_abbreviated_types.h
 * @author Douglas Cuthbertson
 * @brief platform-wide type definitions and macros.
 * @version 0.1.0
 * @date 2022-01-24
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <stdint.h> // int8_t, int16_t, etc.
#include <limits.h> // CHAR_BIT

#if defined(__cplusplus)
extern "C" {
#endif

// Brief type-names
typedef int8_t  i08;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u08;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef wchar_t wch;
typedef float   r32;
typedef double  r64;

// Types for bit flags
typedef u32 flag32;
typedef u64 flag64;

// Complement the definition of CHAR_BIT
#define U08_BIT CHAR_BIT
#define U16_BIT (sizeof(u16) * U08_BIT)
#define U32_BIT (sizeof(u32) * U08_BIT)
#define U64_BIT (sizeof(u64) * U08_BIT)

#define I08_MASK      ((i08)(~0))
#define I16_MASK      ((i16)(~0))
#define I32_MASK      ((i32)(~0))
#define I64_MASK      ((i64)(~0))

#define U08_MASK      ((u08)(~0))
#define U16_MASK      ((u16)(~0))
#define U16_MASK_HIGH 0xFF00
#define U16_MASK_LOW  0x00FF
#define U32_MASK      ((u32)(~0))
#define U32_MASK_HIGH 0xFFFF0000
#define U32_MASK_LOW  0x0000FFFF
#define U64_MASK      ((u64)(~0))
#define U64_MASK_HIGH 0xFFFFFFFF00000000
#define U64_MASK_LOW  0x00000000FFFFFFFF

/// COMPARE returns ~0 if x==y, and zero otherwise.
#define FL_COMPARE(x, y) (((x) == (y)) * (~0))

/// NEQ returns 0 if x==y and a non-zero value otherwise (it's just "x xor y").
#define FL_NEQ(x, y) ((x) ^ (y))

#if defined(__cplusplus)
}
#endif

#endif // FL_ABBREVIATED_TYPES_H_
