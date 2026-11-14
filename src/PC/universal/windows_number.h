#ifndef COD2_WINDOWS_NUMBER_H
#define COD2_WINDOWS_NUMBER_H
#include <stdint.h>
#include <string.h>
#include <limits.h>

static uint64_t Q_WindowsRoundShiftEven(uint64_t value, unsigned shift)
{
    if (!shift) return value;
    /* The supported exact products have fewer than 64 bits. */
    if (shift >= 64) return 0;
    uint64_t whole = value >> shift;
    uint64_t remainder = value & ((UINT64_C(1) << shift) - 1);
    uint64_t half = UINT64_C(1) << (shift - 1);
    return whole + (remainder > half || (remainder == half && (whole & 1)));
}

/* Windows PC24 FMUL of a parsed binary64 by 255 or 1000, followed by
 * the original float store and nearest-even signed 32-bit FISTP.
 * scale is an exact positive integer no larger than 1000.
 * No floating-point expression may replace the exact significand product.
 */
static int Q_WindowsScaleDoubleToInt(double value, unsigned scale)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    unsigned encodedExponent = (unsigned)((bits >> 52) & 0x7ffu);
    unsigned negative = (unsigned)(bits >> 63);
    if (encodedExponent == 0x7ffu) return INT32_MIN;
    uint64_t significand = bits & UINT64_C(0x000fffffffffffff);
    int exponent;
    if (encodedExponent) {
        significand |= UINT64_C(0x0010000000000000);
        exponent = (int)encodedExponent - 1075;
    } else {
        exponent = -1074;
    }
    uint64_t product = significand * scale;
    if (!product) return 0;
    unsigned width = 64u - (unsigned)__builtin_clzll(product);
    if (width > 24) {
        unsigned shift = width - 24;
        product = Q_WindowsRoundShiftEven(product, shift);
        exponent += (int)shift;
    }
    /* The float store's subnormal/overflow rounding cannot change this int:
     * subnormal results round to zero, and float overflow is already outside
     * the signed 32-bit range. At |x| >= 0.5, the original +2^-30 is too small
     * to change the PC24 result; at smaller magnitudes its integer is zero.
     */
    uint64_t integer;
    if (exponent < 0) {
        integer = Q_WindowsRoundShiftEven(product, (unsigned)-exponent);
    } else {
        if (exponent > 31 || product > (UINT64_C(0x80000000) >> exponent))
            return INT32_MIN;
        integer = product << exponent;
    }
    uint64_t limit = negative ? UINT64_C(0x80000000) : INT32_MAX;
    if (integer > limit || integer == UINT64_C(0x80000000)) return INT32_MIN;
    return negative ? -(int)integer : (int)integer;
}
#endif
