#ifndef PC_EFFECTSCORE_FXUTIL_H_H
#define PC_EFFECTSCORE_FXUTIL_H_H

#include "PC/EffectsCore/FxPrimitives.h"

struct EffectPrimitive;

struct EffectPrimitive {
    const EffectTemplate *fx;
    const PrimitiveTemplate *primTemp;
    FxBoltFramePtr boltFrame;
};

#endif
