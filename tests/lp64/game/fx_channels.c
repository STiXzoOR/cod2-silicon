/* Synthetic FX channel text exercises the production template parser. */
#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern Bool PrimitiveTemplate_ParseChannel(const PrimitiveTemplate *, BackCompatibleParameters *, GPGroup *, FxChannelId,
    const PrimitiveTemplate *, const char *, const PrimitiveTemplate *, const char *, const PrimitiveTemplate *, const char *, const PrimitiveTemplate *, const char *);

void *Hunk_AllocateTempMemoryInternal(int size)
{
    return malloc(size);
}

void Hunk_FreeTempMemory(void *p)
{
    free(p);
}

void *Hunk_AllocAlignInternal(int size, int align)
{
    assert(align <= 16);
    return malloc(size);
}

int stricmp(const char *a, const char *b)
{
    return strcasecmp(a, b);
}

const char *GPValue_GetTopValue(const GPValue *p)
{
    return p->valueList ? p->valueList->name : p->name;
}

void FX_Print(const char *fmt, ...)
{
    abort();
}

int main(void)
{
    PrimitiveTemplate prim = {0};
    BackCompatibleParameters compat = {0};
    GPValue row2 = {.name = "1 .6 .7 .8"};
    GPValue row1 = {.name = "0 .2 .3 .4", .next = (GPObject *)&row2};
    GPValue scaleValue = {.name = "64 96"};
    GPValue scale = {.name = "scale", .valueList = &scaleValue};
    GPValue curve = {.name = "curve", .valueList = &row1, .next = (GPObject *)&scale};
    GPGroup group = {.pairList = &curve};
    for (int color = FXCHAN_COLOR; color <= FXCHAN_COLOR_RAND; ++color) {
        assert(PrimitiveTemplate_ParseChannel(&prim, &compat, &group, color, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
        const FxCurve *master = prim.mFxChannels[color].curve;
        assert(master && master->dimensionCount == 3 && master->keyCount == 2);
        assert(master->keys[2] == .3f && master->keys[7] == .8f);
        assert(prim.mFxChannels[color].scaleRange.mMin == 64 && prim.mFxChannels[color].scaleRange.mMax == 96);
        assert(!compat.fxChannels[color].containsData);
        free((void *)master);
    }
    row1.name = "0 .25";
    row2.name = "1 .75";
    scaleValue.name = "128";
    assert(PrimitiveTemplate_ParseChannel(&prim, &compat, &group, FXCHAN_SIZE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    const FxCurve *master = prim.mFxChannels[FXCHAN_SIZE].curve;
    assert(master->dimensionCount == 1 && master->keyCount == 2);
    assert(master->keys[1] == .25f && master->keys[3] == .75f);
    assert(prim.mFxChannels[FXCHAN_SIZE].scaleRange.mMin == 128 && prim.mFxChannels[FXCHAN_SIZE].scaleRange.mMax == 128);
    free((void *)master);
    puts("FX channels: production RGB dimensions, scalar curves and single/range scales pass");
}
