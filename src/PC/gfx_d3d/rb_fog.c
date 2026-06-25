#include "common_types.h"
extern GfxBackEndData *backEndData;
extern dvar_t *r_rendererInUse;
#include "imports.h"
extern int alwaysfails;
extern DxGlobals dx;

extern r_globals_t rg;
extern r_backEndGlobals_t backEnd;
extern struct DxState dxState;

extern void **g_viewInfo;

GfxFogOffset RB_FogOffset(void)
{
    const dvar_t *fogDvar = *(const dvar_t **)imp_r_fog;
    if (fogDvar->current.enabled == 0) {
        return 0;
    }

    GfxFog *fog = &backEndData->fogSettings;
    return fog->techniqueOffset;
}

void RB_UpdateFogColor(FogColorSrcEnum fogColorSrc)
{
    unsigned int fogColor;

    if (rg.fogIndex == 0)
        return;

    if (fogColorSrc != 0)
        goto src_nonzero;

    if ((dxState.refStateBits[0] & 0xf0) == 0x20) {
        fogColor = 0;
        goto or_mask;
    }

use_backend:
    fogColor = backEnd.fogColor.packed;

or_mask:
    fogColor |= 0xff000000;

    if (dxState.fog.color.packed == fogColor)
        return;

    do {
        void *device = dx.device;
        void **vtable = *(void ***)device;
        typedef int(D3DVTCC * SetRenderStateFn)(void *, int, unsigned int);
        ((SetRenderStateFn)vtable[0xe4 / 4])(device, 0x22, fogColor);
    } while (*(volatile int *)&alwaysfails != 0);

    dxState.fog.color.packed = fogColor;
    return;

src_nonzero:
    if (fogColorSrc == 2) {
        fogColor = 0;
        goto or_mask;
    }
    goto use_backend;
}

void RB_SetIteratorFog(void)
{
    materialCommands_t *tess = (materialCommands_t *)imp_tess;
    if (tess->techType == 3)
        return;

    GfxFog *fog = &backEndData->fogSettings;

    if (fog->techniqueOffset == 0)
        return;

    if (!fog->registered)
        return;

    r_backEndGlobals_t *be = &backEnd;
    unsigned int fogColorPacked = fog->color.packed;
    be->fogColor.packed = fogColorPacked;

    if (rg.fogIndex != 0) {
        unsigned int fogColor;
        if ((dxState.refStateBits[0] & 0xf0) == 0x20) {
            fogColor = 0;
        } else {
            fogColor = fogColorPacked;
        }
        fogColor |= 0xff000000;

        if (dxState.fog.color.packed != fogColor) {
            do {
                void *device = dx.device;
                void **vtable = *(void ***)device;
                typedef int(D3DVTCC * SetRenderStateFn)(void *, int, unsigned int);
                ((SetRenderStateFn)vtable[0xe4 / 4])(device, 0x22, fogColor);
            } while (*(volatile int *)&alwaysfails != 0);

            dxState.fog.color.packed = fogColor;
        }
    }

    float inv255 = 0.003921568859368563f;
    float fr = (float)fog->color.array[1] * inv255;
    float fg = (float)fog->color.array[0] * inv255;
    float fa = (float)fog->color.array[3] * inv255;
    float fb = (float)fog->color.array[2] * inv255;

    be->codeConsts[29][0] = fb;
    be->codeConsts[29][1] = fr;
    be->codeConsts[29][2] = fg;
    be->codeConsts[29][3] = fa;

    float *viewInfo = *(float **)g_viewInfo;
    float fogEnd = viewInfo[2];
    if (fogEnd == 0.0f) {
        fogEnd = fog->fogEnd;
    }
    float invRange = 1.0f / (fogEnd - fog->fogStart);

    be->codeConsts[28][0] = -invRange;
    be->codeConsts[28][1] = invRange * fogEnd;
    be->codeConsts[28][2] = -fog->density;
    be->codeConsts[28][3] = 0.0f;

    int *dxCaps = (int *)r_rendererInUse;
    if (dxCaps[2] != 2)
        return;

    {

        if (fog->techniqueOffset == 1)
            goto exp_fog;

        if (dxState.fog.mode != 1) {
            do {
                void *device = dx.device;
                void **vtable = *(void ***)device;
                typedef int(D3DVTCC * SetRenderStateFn)(void *, int, int);
                ((SetRenderStateFn)vtable[0xe4 / 4])(device, 0x8c, 1);
            } while (*(volatile int *)&alwaysfails != 0);
            dxState.fog.mode = 1;
        }

        if (dxState.fog.density != fog->density) {
            do {
                void *device = dx.device;
                void **vtable = *(void ***)device;
                typedef int(D3DVTCC * SetRenderStateFn)(void *, int, int);
                ((SetRenderStateFn)vtable[0xe4 / 4])(device, 0x26, *(int *)&fog->density);
            } while (*(volatile int *)&alwaysfails != 0);
            dxState.fog.density = fog->density;
        }

        return;

    exp_fog:

        if (dxState.fog.mode != 3) {
            do {
                void *device = dx.device;
                void **vtable = *(void ***)device;
                typedef int(D3DVTCC * SetRenderStateFn)(void *, int, int);
                ((SetRenderStateFn)vtable[0xe4 / 4])(device, 0x8c, 3);
            } while (*(volatile int *)&alwaysfails != 0);
            dxState.fog.mode = 3;
        }

        if (dxState.fog.start != fog->fogStart) {
            do {
                void *device = dx.device;
                void **vtable = *(void ***)device;
                typedef int(D3DVTCC * SetRenderStateFn)(void *, int, int);
                ((SetRenderStateFn)vtable[0xe4 / 4])(device, 0x24, *(int *)&fog->fogStart);
            } while (*(volatile int *)&alwaysfails != 0);
            dxState.fog.start = fog->fogStart;
        }

        if (dxState.fog.end != fogEnd) {
            do {
                void *device = dx.device;
                void **vtable = *(void ***)device;
                typedef int(D3DVTCC * SetRenderStateFn)(void *, int, int);
                ((SetRenderStateFn)vtable[0xe4 / 4])(device, 0x25, *(int *)&fogEnd);
            } while (*(volatile int *)&alwaysfails != 0);
            dxState.fog.end = fogEnd;
        }
    }

    return;
}
