#include "common_types.h"
#include "imports.h"

void CAStreamBasicDescription_SetCanonical(const CAStreamBasicDescription *_this, UInt32 nChannels, int interleaved);
void CAStreamBasicDescription_CAStreamBasicDescription(const CAStreamBasicDescription *_this);

void CAStreamBasicDescription_SetCanonical(const CAStreamBasicDescription *_this, UInt32 nChannels, int interleaved) {}

void CAStreamBasicDescription_CAStreamBasicDescription(const CAStreamBasicDescription *_this)
{
    memset((void *)_this, 0, 40);
}
