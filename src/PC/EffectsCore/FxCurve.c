#include "common_types.h"
#include "imports.h"

void FxCurveIterator_Create(FxCurveIterator *createe, const FxCurve *master)
{
    createe->master = master;
    createe->currentKeyIndex = 0;
}

float FxCurve_Integrate(const FxCurve *curve, float normDuration)
{
    int keyStride = curve->dimensionCount + 1;
    const float *keys = (const float *)((const char *)curve + 8);
    float result = 0.0f;

    const float *iKey = keys;
    const float *nextKey = keys + keyStride;
    while (normDuration > nextKey[0]) {
        float avgVal = (nextKey[1] + iKey[1]) * 0.5f;
        float dt = nextKey[0] - iKey[0];
        result += avgVal * dt;
        iKey = nextKey;
        nextKey += keyStride;
    }

    float t0 = iKey[0];
    float v0 = iKey[1];
    float t1 = iKey[2];
    float v1 = iKey[3];
    float dt = normDuration - t0;
    float interpVal = (v1 - v0) * dt / (t1 - t0) + v0;
    float avgVal = (interpVal + v0) * 0.5f;
    result += avgVal * dt;

    return result;
}
