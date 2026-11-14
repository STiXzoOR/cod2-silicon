#ifndef COD2_SND_ROUNDING_H
#define COD2_SND_ROUNDING_H
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

/* The Windows mixer uses x87 PC24 arithmetic but FILD keeps its integer
 * operand exact, and intermediate exponents remain wider than binary32.
 * A binary64 result plus its fused residual locates the exact value on
 * either side of a PC24 midpoint without double rounding. All finite
 * operands used below fit the normal binary64 exponent range. */
static inline double SND_WindowsRoundPC24(double value, double residual)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    if ((bits & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000))
        return value;
    const uint64_t mask = (UINT64_C(1) << 29) - 1;
    const uint64_t half = UINT64_C(1) << 28;
    uint64_t remainder = bits & mask;
    int up;
    if (remainder == half && residual != 0.0)
        up = (bits >> 63) ? residual < 0.0 : residual > 0.0;
    else
        up = remainder > half || (remainder == half && ((bits >> 29) & 1));
    bits = (bits & ~mask) + (up ? UINT64_C(1) << 29 : 0);
    memcpy(&value, &bits, sizeof value);
    return value;
}

static inline double SND_WindowsMul(double left, double right)
{
    double value = left * right;
    return SND_WindowsRoundPC24(value, fma(left, right, -value));
}

static inline double SND_WindowsDiv(double left, double right)
{
    double value = left / right;
    double residual = fma(-value, right, left);
    return SND_WindowsRoundPC24(value, signbit(right) ? -residual : residual);
}

/* FILD signed + FADD 2^32 for unsigned values with bit 31 set. */
static inline double SND_WindowsUnsigned(unsigned int value)
{
    return value & 0x80000000u ? (double)(float)value : (double)value;
}

/* FSTP binary32 then nearest-even FISTP; invalid converts to 0x80000000.
 * The intervening original +2^-30 cannot change this integer result. */
static inline int SND_WindowsInt(double value)
{
    float rounded = __builtin_rintf((float)value);
    if (!(rounded >= -2147483648.0f && rounded < 2147483648.0f))
        return INT32_MIN;
    return (int)rounded;
}

static inline int SND_WindowsRate(int rate, float first, float second)
{
    return SND_WindowsInt(SND_WindowsMul(SND_WindowsMul((double)rate, first), second));
}

static inline int SND_WindowsUnsignedRate(unsigned int rate, float first, float second)
{
    return SND_WindowsInt(SND_WindowsMul(SND_WindowsMul(SND_WindowsUnsigned(rate), first), second));
}

static inline int SND_WindowsScale(int value, float scale)
{
    return SND_WindowsInt(SND_WindowsMul((double)value, scale));
}

static inline int SND_WindowsUnscale(int value, float scale)
{
    return SND_WindowsInt(SND_WindowsDiv((double)value, scale));
}

static inline int SND_WindowsDuration(unsigned int samples, float timescale, int rate)
{
    unsigned int sampleMillis = samples * 1000u;
    double scaled = SND_WindowsMul(SND_WindowsUnsigned(sampleMillis), timescale);
    return SND_WindowsInt(SND_WindowsDiv(scaled, (double)rate));
}

static inline int SND_WindowsOffset(unsigned int bytes, float fraction)
{
    return SND_WindowsInt(SND_WindowsMul(SND_WindowsUnsigned(bytes), fraction));
}
#endif
