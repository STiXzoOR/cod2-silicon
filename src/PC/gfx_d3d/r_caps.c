#include "common_types.h"
#include "imports.h"
#include <stdio.h>

extern void MacDisplay_GetVideoMemoryInfo(int *videoMemory, int *textureMemory);
extern void R_Error(int level, const char *fmt, ...);
extern refimport_t ri;

static const DxCapsCheckBits s_capsCheckBits[36] = {
    { 0x0c, 0x0, 0x20000000, DX_CAPS_RESPONSE_QUIT, "doesn't support dynamic textures" },
    { 0x0c, 0x0, 0x20000, DX_CAPS_RESPONSE_WARN, "doesn't support fullscreen gamma" },
    { 0x10, 0x0, 0x20, DX_CAPS_RESPONSE_QUIT, "doesn't support alpha blending" },
    { 0x10, 0x0, 0x100, DX_CAPS_RESPONSE_WARN, "doesn't accelerate dynamic textures" },
    { 0x14, 0x0, 0x80000000, DX_CAPS_RESPONSE_WARN, "doesn't support immediate frame buffer swapping" },
    { 0x14, 0x0, 0x1, DX_CAPS_RESPONSE_WARN, "doesn't support vertical sync" },
    { 0x1c, 0x0, 0x8000, DX_CAPS_RESPONSE_QUIT, "is not at least DirectX 7 compliant" },
    { 0x1c, 0x0, 0x10400, DX_CAPS_RESPONSE_WARN, "doesn't accelerate transform and lighting" },
    { 0x1c, 0x0, 0x80000, DX_CAPS_RESPONSE_WARN, "doesn't accelerate rasterization" },
    { 0x20, 0x0, 0x2, DX_CAPS_RESPONSE_QUIT, "can't disable depth buffer writes" },
    { 0x20, 0x0, 0x80, DX_CAPS_RESPONSE_QUIT, "can't disable individual color channel writes" },
    { 0x20, 0x0, 0x800, DX_CAPS_RESPONSE_INFO, "doesn't support frame buffer blending ops beside add" },
    { 0x20, 0x0, 0x20000, DX_CAPS_RESPONSE_INFO, "doesn't support separate alpha blend, glow will be disabled" },
    { 0x20, 0x0, 0x70, DX_CAPS_RESPONSE_QUIT, "doesn't support all face culling modes" },
    { 0x24, 0x0, 0x2000000, DX_CAPS_RESPONSE_INFO, "doesn't support high-quality polygon offset" },
    { 0x24, 0x0, 0x80, DX_CAPS_RESPONSE_FORBID_DX7, "doesn't support fog" },
    { 0x28, 0x0, 0x8d, DX_CAPS_RESPONSE_QUIT, "doesn't support the required depth comparison modes" },
    { 0x2c, 0x0, 0x3ff, DX_CAPS_RESPONSE_QUIT, "doesn't support the required frame buffer source blend modes" },
    { 0x30, 0x0, 0x3ff, DX_CAPS_RESPONSE_QUIT, "doesn't support the required frame buffer destination blend modes" },
    { 0x34, 0x0, 0xd2, DX_CAPS_RESPONSE_QUIT, "doesn't support the required alpha comparison modes" },
    { 0x3c, 0x0, 0x4, DX_CAPS_RESPONSE_QUIT, "doesn't support alpha in textures" },
    { 0x3c, 0x0, 0x800, DX_CAPS_RESPONSE_QUIT, "doesn't support cubemap textures" },
    { 0x3c, 0x0, 0x4000, DX_CAPS_RESPONSE_QUIT, "doesn't support mipmapped textures" },
    { 0x3c, 0x2, 0x100, DX_CAPS_RESPONSE_QUIT, "doesn't support restricted use of non-power-of-2 textures" },
    { 0x3c, 0x0, 0x1, DX_CAPS_RESPONSE_WARN, "doesn't support perspective correct texturing" },
    { 0x3c, 0x20, 0x0, DX_CAPS_RESPONSE_QUIT, "doesn't support non-square textures" },
    { 0x40, 0x0, 0x3030300, DX_CAPS_RESPONSE_QUIT, "doesn't support the required texture filtering modes" },
    { 0x44, 0x0, 0x3000300, DX_CAPS_RESPONSE_QUIT, "doesn't support the required cubemap texture filtering modes" },
    { 0x4c, 0x0, 0x4, DX_CAPS_RESPONSE_QUIT, "doesn't support texture clamping" },
    { 0x4c, 0x0, 0x1, DX_CAPS_RESPONSE_QUIT, "doesn't support texture wrapping" },
    { 0x88, 0x0, 0xff, DX_CAPS_RESPONSE_FORBID_DX9, "doesn't support the required stencil operations" },
    { 0x90, 0x0, 0x39effff, DX_CAPS_RESPONSE_FORBID_DX7, "doesn't support the required fixed-function texture blend operations" },
    { 0x9c, 0x0, 0x8, DX_CAPS_RESPONSE_FORBID_DX7, "doesn't support direction lights in the fixed-function pipeline" },
    { 0x9c, 0x0, 0x1, DX_CAPS_RESPONSE_FORBID_DX7, "doesn't support texture coordinate generation in the fixed-function pipeline" },
    { 0xd4, 0x0, 0x1, DX_CAPS_RESPONSE_QUIT, "doesn't support vertex stream offsets" },
    { 0xf4, 0x0, 0x200, DX_CAPS_RESPONSE_WARN, "doesn't support linear filtering when copying and shrinking the frame buffer" },
};

static const DxCapsCheckInteger s_capsCheckInt[8] = {
    { 0x94, 4, -1, DX_CAPS_RESPONSE_QUIT, "doesn't support enough texture stages / coordinates" },
    { 0x94, 8, -1, DX_CAPS_RESPONSE_FORBID_DX9, "doesn't support enough texture coordinates for the DirectX 9 code path" },
    { 0x98, 2, -1, DX_CAPS_RESPONSE_QUIT, "doesn't support multitexture" },
    { 0x98, 8, -1, DX_CAPS_RESPONSE_FORBID_DX9, "doesn't support enough textures for the DirectX 9 code path" },
    { 0xa0, 3, -1, DX_CAPS_RESPONSE_FORBID_DX7, "doesn't support enough hardware lights for the DirectX 7 code path" },
    { 0xbc, 1, -1, DX_CAPS_RESPONSE_QUIT, "is not a DirectX 9 driver" },
    { 0xc4, 512, -1, DX_CAPS_RESPONSE_FORBID_DX9, "doesn't support vertex shader 2.0 or better" },
    { 0xcc, 512, -1, DX_CAPS_RESPONSE_FORBID_DX9, "doesn't support pixel shader 2.0 or better" },
};

static inline __attribute__((always_inline)) void
R_HandleCapsResponse(int response, const char *msg, int *allowedPaths)
{
    int printLevel;

    if (response == DX_CAPS_RESPONSE_WARN || response == DX_CAPS_RESPONSE_FORBID_DX7) {
        printLevel = 2;
    } else {
        printLevel = 0;
    }

    typedef void (*PrintfFn)(int, const char *, ...);
    ((PrintfFn)ri.Printf)(printLevel, "Video card or driver %s.\n", msg);

    switch (response) {
    case DX_CAPS_RESPONSE_FORBID_DX7:
        *allowedPaths &= ~4;
        ((PrintfFn)ri.Printf)(printLevel, "  DirectX 7 rendering path will not be available.\n");
        break;
    case DX_CAPS_RESPONSE_FORBID_DX9:
        *allowedPaths &= ~2;
        ((PrintfFn)ri.Printf)(printLevel, "  DirectX 9 rendering path will not be available.\n");
        break;
    case DX_CAPS_RESPONSE_QUIT:
        R_Error(0, "Video card or driver %s.\n", msg);
        break;
    }
}

int R_CheckDxCaps(const D3DCAPS9 *caps)
{
    int videoMemory;
    int textureMemory;
    int allowedPaths;
    const DxCapsCheckBits *bit;
    const DxCapsCheckInteger *intCheck;

    MacDisplay_GetVideoMemoryInfo(&videoMemory, &textureMemory);

    allowedPaths = 4;
    if (videoMemory >= 0x4000001)
        allowedPaths += 2;

    for (bit = s_capsCheckBits; bit != s_capsCheckBits + 36; bit++) {
        DWORD value = *(const DWORD *)((const char *)caps + bit->offset);

        if (bit->clearBits && (~value & bit->clearBits) == 0)
            continue;
        if (bit->setBits && (bit->setBits & value) == 0)
            continue;

        R_HandleCapsResponse(bit->response, bit->msg, &allowedPaths);
    }

    for (intCheck = s_capsCheckInt; intCheck != s_capsCheckInt + 8; intCheck++) {
        DWORD value = *(const DWORD *)((const char *)caps + intCheck->offset);

        if (value >= (DWORD)intCheck->min && value <= (DWORD)intCheck->max)
            continue;

        R_HandleCapsResponse(intCheck->response, intCheck->msg, &allowedPaths);
    }

    return allowedPaths;
}
