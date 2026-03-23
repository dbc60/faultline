/**
 * @file math.c
 * @author Douglas Cuthbertson
 * @brief
 * @version 0.1
 * @date 2022-02-12
 *
 * See LICENSE.txt for copyright and licensing information about this file.
 */
#include <faultline/fl_math.h>
#include <faultline/fl_abbreviated_types.h> // u16, u32, u64

u16 math_count_leading_zeros16(u16 val) {
    u16 m; // shift amount applied at each narrowing step
    u16 n = 0;
    u16 result;

    // Now we only have 16 bits to test. Regardless of the original value of val, the
    // least-significant 16 bits are the ones being counted here. If val is less than
    // 0x100, set m to 8, otherwise set m to zero.
    m = ((val - 0x100) >> 16) & 0x8;
    // Set n to the current number of leading zeros
    n += m;
    // If m is eight, then val was no more than 0xFF, so left-shift val eight bits so val
    // is at most 0xFF00. Otherwise val is unchanged
    val <<= m;

    // Now we only have eight bits to test. Regardless of the original value of val, the
    // least-significant 8 bits are the ones being tested here. If val is less than
    // 0x1000, set m to four, otherwise set m to zero. This indicates that val was
    // originally no more than 0x0F and was left-shifted to the value of 0xF00 by the
    // line of code above.
    m = ((val - 0x1000) >> 16) & 0x4;
    // Set n to the current number of leading zeros
    n += m;
    // If m is four, then val now has a value no more than 0xF00, so left-shift val four
    // bits so val is at most 0xF000. Otherwise val is unchanged.
    val <<= m;

    // Now we only have four bits to test (bits 12 through 15). If val is less than
    // 0x4000, the highest bit is in position 12 or 13, so set m to 2. Otherwise set m to
    // zero.
    m = ((val - 0x4000) >> 16) & 0x2;
    // Set n to the current number of leading zeros
    n += m;
    // If m is two, left-shift val two bits to move the highest bit into position 14
    // or 15. Otherwise val is unchanged.
    val <<= m;

    // The highest bit is now in position 14 or 15. m = 1 if bit 14 is the highest set
    // bit, m = 2 if bit 15 is set. result = n + 2 - m gives the total leading zero
    // count.
    m = (val >> 14) & ~(val >> 15);

    result = n + 2 - m;

    return result;
}

u16 math_count_leading_zeros32(u32 val) {
    u16 m; // 16 if the upper 16 bits are zero, else 0
    u16 n;
    u16 result;

    // If val is less than 0x10000 (i.e., the left-most 16-bits are zero), then set n
    // to 16. Otherwise set n to zero. Right-shift val 16-bits if it is at least 0x10000,
    // moving the upper bits into the lower 16, otherwise leave it unchanged.
    m = (u16)(((~(val >> 16) + 1) >> 16) & 0x10);
    n = 16 - m;
    val >>= m;

    u16 low16 = (u16)(val & 0xFFFF);
    result    = n + math_count_leading_zeros16(low16);

    return result;
}

u16 math_count_leading_zeros64(u64 val) {
    u16 m; // 32 if the upper 32 bits are zero, else 0
    u16 n;
    u16 result;

    // If val is less than 0x100000000 (i.e., the left-most 32-bits are zero), then set n
    // to 32. Otherwise set n to zero. Right-shift val 32-bits if it is at least
    // 0x100000000, moving the upper bits into the lower 32, otherwise leave it
    // unchanged.
    m = (u16)(((~(val >> 32) + 1) >> 32) & 0x20);
    n = 32 - m;
    val >>= m;

    u32 low32       = (u32)(val & 0xFFFFFFFF);
    u16 count_low32 = math_count_leading_zeros32(low32);
    result          = n + count_low32;

    return result;
}

u16 math_log2_bit16(u16 val) {
    u16 zeros = math_count_leading_zeros16(val);
    return 15 - zeros;
}

u16 math_log2_bit32(u32 val) {
    u16 zeros = math_count_leading_zeros32(val);
    return 31 - zeros;
}

u16 math_log2_bit64(u64 val) {
    u16 zeros = math_count_leading_zeros64(val);
    return 63 - zeros;
}

// Parallel bit-summation popcount. See Warren Jr, H. S. (2002).
// Hacker's Delight (ch. 5). Addison-Wesley.
u16 math_pop_count32(u32 val) {
    val -= ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    val = (val + (val >> 4)) & 0x0F0F0F0F;
    val += val >> 8;
    val += val >> 16;

    return (u16)(val & 0x0000003F);
}

u16 math_pop_count64(u64 val) {
    val -= ((val >> 1) & 0x5555555555555555);
    val = (val & 0x3333333333333333) + ((val >> 2) & 0x3333333333333333);
    val = (val + (val >> 4)) & 0x0F0F0F0F0F0F0F0F;
    val += val >> 8;
    val += val >> 16;
    val += val >> 32;

    return (u16)(val & 0x0000007F);
}
