#ifndef COD2X_MATH_H
#define COD2X_MATH_H

#include <math.h>
#include <stdint.h>

/* CoD2x's replacement routines use its C++ helper arithmetic. The stock
   engine's helpers have different angular quantization and rounding. */
static inline float Cod2x_AngleNormalize360(float angle)
{
    double steps = (double)angle * (65536.0 / 360.0);
    if (!(steps >= -2147483648.0 && steps < 2147483648.0))
        return 0.0f;
    uint32_t wrapped = (uint32_t)(int32_t)steps & 65535u;
    return (float)((double)wrapped * (360.0 / 65536.0));
}

static inline float Cod2x_AngleNormalize180(float angle)
{
    float wrapped = Cod2x_AngleNormalize360(angle);
    return wrapped > 180.0f ? wrapped - 360.0f : wrapped;
}

static inline float Cod2x_AngleSubtract(float left, float right)
{
    return Cod2x_AngleNormalize180(left - right);
}

static inline void Cod2x_AnglesSubtract(const float *left, const float *right, float *out)
{
    for (int axis = 0; axis < 3; ++axis)
        out[axis] = Cod2x_AngleSubtract(left[axis], right[axis]);
}

static inline float Cod2x_VectorNormalizeTo(const float *value, float *out)
{
    float length = sqrtf(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    double reciprocal = length == 0.0f ? 0.0 : 1.0 / (double)length;
    for (int axis = 0; axis < 3; ++axis)
        out[axis] = length == 0.0f ? 0.0f : (float)((double)value[axis] * reciprocal);
    return length;
}

static inline void Cod2x_VectorToAngles(const float *value, float *out)
{
    float pitch, yaw;
    if (value[0] == 0.0f && value[1] == 0.0f) {
        yaw = 0.0f;
        pitch = value[2] > 0.0f ? 270.0f : 90.0f;
    } else {
        /* math.h's C++ float atan2 overload rounds before the degree product. */
        float yawDegrees = atan2f(value[1], value[0]) * 180.0f;
        yaw = (float)((double)yawDegrees / 3.14159265358979323846);
        if (yaw < 0.0f)
            yaw += 360.0f;
        float horizontal = sqrtf(value[0] * value[0] + value[1] * value[1]);
        float pitchDegrees = atan2f(value[2], horizontal) * -180.0f;
        pitch = (float)((double)pitchDegrees / 3.14159265358979323846);
        if (pitch < 0.0f)
            pitch += 360.0f;
    }
    out[0] = pitch;
    out[1] = yaw;
    out[2] = 0.0f;
}

static inline float Cod2x_DegreesToRadians(float angle)
{
    return (float)((double)angle * (3.14159265358979323846 / 180.0));
}

static inline float Cod2x_Clamp(float value, float low, float high)
{
    return fmaxf(low, fminf(value, high));
}

#endif
