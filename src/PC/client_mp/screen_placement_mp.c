#include "common_types.h"
#include "imports.h"

extern unsigned char spGlob[];
#define SP ((struct ScreenPlacementGlob *)spGlob)

extern byte *cls_ptr_195ecac;

float GetRealWidthFromVirtualWidth(float width);
float GetRealHeightFromVirtualHeight(float height);
float GetVirtualWidthFromRealWidth(float width);
float GetVirtualHeightFromRealHeight(float height);
void CalcScreenX(float *x, int horzAlign);
void CalcScreenY(float *y, int vertAlign);
void CalcSplitScreenTextOffset(FontHandle font, float *y);
#if defined(COD2_X64)
float ScrPlace_RealViewportHeight(void)
{
    return SP->realViewportSize[1];
}
#endif

void SetScreenScaling(float safeAreaRatioHorizontal, float safeAreaRatioVertical, int viewportX, int viewportY, int viewportWidth, int viewportHeight);
void CalcScreenPlacement(float *x, float *y, float *w, float *h, int horzAlign, int vertAlign);

float GetRealWidthFromVirtualWidth(float width)
{
    return width * SP->scaleVirtualToReal[0];
}

float GetRealHeightFromVirtualHeight(float height)
{
    return height * SP->scaleVirtualToReal[1];
}

float GetVirtualWidthFromRealWidth(float width)
{
    return width * SP->scaleRealToVirtual[0];
}

float GetVirtualHeightFromRealHeight(float height)
{
    return height * SP->scaleRealToVirtual[1];
}

void CalcScreenX(float *x, int horzAlign)
{
    switch (horzAlign) {
    default:
#if COD2_APPLE_SDK
        *x = *x * SP->scaleVirtualToReal[0] + SP->virtualScreenOffsetX + SP->subScreenLeft;
#else
        *x = *x * SP->scaleVirtualToReal[0] + SP->subScreenLeft + SP->virtualScreenOffsetX;
#endif
        break;
    case 1:
        *x = *x * SP->scaleVirtualToReal[0] + SP->realViewableMin[0];
        break;
    case 2:
        *x = *x * SP->scaleVirtualToReal[0] + SP->realViewportSize[0] * 0.5f;
        break;
    case 3:
        *x = *x * SP->scaleVirtualToReal[0] + SP->realViewableMax[0];
        break;
    case 4:
        *x *= SP->scaleVirtualToFull[0];
        break;
    case 5:
        break;
    case 6:
        *x = *x * SP->scaleRealToVirtual[0] + SP->virtualScreenOffsetX;
        break;
    case 7:
        *x = *x * SP->scaleVirtualToReal[0] + (SP->realViewableMin[0] + SP->realViewableMax[0]) * 0.5f;
        break;
    }
}

void CalcScreenY(float *y, int vertAlign)
{
    switch (vertAlign) {
    default:
        *y *= SP->scaleVirtualToReal[1];
        break;
    case 1:
        *y = *y * SP->scaleVirtualToReal[1] + SP->realViewableMin[1];
        break;
    case 2:
        *y = *y * SP->scaleVirtualToReal[1] + SP->realViewportSize[1] * 0.5f;
        break;
    case 3:
        *y = *y * SP->scaleVirtualToReal[1] + SP->realViewableMax[1];
        break;
    case 4:
        *y *= SP->scaleVirtualToFull[1];
        break;
    case 5:
        break;
    case 6:
        *y *= SP->scaleRealToVirtual[1];
        break;
    case 7:
        *y = *y * SP->scaleVirtualToReal[1] + (SP->realViewableMin[1] + SP->realViewableMax[1]) * 0.5f;
        break;
    }
}

void CalcSplitScreenTextOffset(FontHandle font, float *y)
{

}

void SetScreenScaling(float safeAreaRatioHorizontal, float safeAreaRatioVertical,
                      int viewportX, int viewportY, int viewportWidth, int viewportHeight)
{
    float fViewportHeight = (float)viewportHeight;
    float fViewportWidth = (float)viewportWidth;
    float fViewportY = (float)viewportY;
    float fViewportX = (float)viewportX;

    SP->realViewportSize[0] = fViewportWidth;
    SP->realViewportSize[1] = fViewportHeight;

    byte *cls = cls_ptr_195ecac;
#if COD2_APPLE_SDK
    /* Windows 1.3 (0x44bb70): every intermediate uses single precision.
     * Reordering these operations can move text across a pixel boundary. */
    float adjustedRealWidth = (fViewportHeight / ((clientStatic_t *)cls)->vidConfig.aspectRatioPixel) * (4.0f / 3.0f);
    float inverseAdjustedRealWidth = 1.0f / adjustedRealWidth;
#else
    float adjustedRealWidth = fViewportHeight * (4.0f / 3.0f) / (((clientStatic_t *)(cls))->vidConfig.aspectRatioPixel);
#endif
    float horzAspectPixelDiff = fViewportWidth - adjustedRealWidth;
#if COD2_APPLE_SDK
    float horzAspectScale = inverseAdjustedRealWidth * fViewportWidth;
#else
    float horzAspectScale = fViewportWidth / adjustedRealWidth;
#endif

    float displayWidth = (float)(((clientStatic_t *)(cls))->vidConfig.width);
    float displayHeight = (float)(((clientStatic_t *)(cls))->vidConfig.height);

    float unsafeHorz = (1.0f - safeAreaRatioHorizontal) * 0.5f * displayWidth;
    float unsafeVert = (1.0f - safeAreaRatioVertical) * 0.5f * displayHeight;

    float safeRight = displayWidth - unsafeHorz;
    float safeBottom = displayHeight - unsafeVert;

#if defined(COD2_X64)
    float safeLeft = !(fViewportX - unsafeHorz < 0.0f) ? fViewportX : unsafeHorz;
    float safeTop = !(fViewportY - unsafeVert < 0.0f) ? fViewportY : unsafeVert;
    float clampedRight = !(safeRight - (fViewportX + fViewportWidth) < 0.0f) ? (fViewportX + fViewportWidth) : safeRight;
    float clampedBottom = !(safeBottom - (fViewportY + fViewportHeight) < 0.0f) ? (fViewportY + fViewportHeight) : safeBottom;
#else
    float safeLeft = (fViewportX - unsafeHorz >= 0) ? fViewportX : unsafeHorz;
    float safeTop = (fViewportY - unsafeVert >= 0) ? fViewportY : unsafeVert;
    float clampedRight = (safeRight - (fViewportX + fViewportWidth) >= 0) ? (fViewportX + fViewportWidth) : safeRight;
    float clampedBottom = (safeBottom - (fViewportY + fViewportHeight) >= 0) ? (fViewportY + fViewportHeight) : safeBottom;
#endif

    SP->realViewableMin[0] = safeLeft - fViewportX;
    SP->realViewableMin[1] = safeTop - fViewportY;
    SP->realViewableMax[0] = clampedRight - fViewportX;
    SP->realViewableMax[1] = clampedBottom - fViewportY;

#if COD2_APPLE_SDK
    SP->virtualViewableMin[0] = (640.0f / fViewportWidth) * SP->realViewableMin[0] * horzAspectScale;
#else
    SP->virtualViewableMin[0] = horzAspectScale * SP->realViewableMin[0] * (640.0f / fViewportWidth);
#endif
    SP->virtualViewableMin[1] = SP->realViewableMin[1] * (480.0f / fViewportHeight);
#if COD2_APPLE_SDK
    SP->virtualViewableMax[0] = (640.0f / fViewportWidth) * SP->realViewableMax[0] * horzAspectScale;
#else
    SP->virtualViewableMax[0] = horzAspectScale * SP->realViewableMax[0] * (640.0f / fViewportWidth);
#endif
    SP->virtualViewableMax[1] = SP->realViewableMax[1] * (480.0f / fViewportHeight);

#if COD2_APPLE_SDK
    float scaleY = fViewportHeight * (1.0f / 480.0f);
#else
    float scaleY = fViewportHeight / 480.0f;
#endif

#if COD2_APPLE_SDK
    SP->scaleVirtualToReal[0] = adjustedRealWidth * (1.0f / 640.0f);
#else
    SP->scaleVirtualToReal[0] = adjustedRealWidth / 640.0f;
#endif
    SP->scaleVirtualToReal[1] = scaleY;

#if COD2_APPLE_SDK
    SP->scaleVirtualToFull[0] = fViewportWidth * (1.0f / 640.0f);
#else
    SP->scaleVirtualToFull[0] = fViewportWidth / 640.0f;
#endif
    SP->scaleVirtualToFull[1] = scaleY;

#if COD2_APPLE_SDK
    SP->scaleRealToVirtual[0] = inverseAdjustedRealWidth * 640.0f;
#else
    SP->scaleRealToVirtual[0] = 640.0f / adjustedRealWidth;
#endif
    SP->scaleRealToVirtual[1] = 480.0f / fViewportHeight;

    SP->subScreenLeft = 0.5f * horzAspectPixelDiff;
    SP->virtualScreenOffsetX = 640.0f - adjustedRealWidth * SP->scaleRealToVirtual[0];
}

void CalcScreenPlacement(float *x, float *y, float *w, float *h, int horzAlign, int vertAlign)
{
    switch (horzAlign) {
    default:
#if COD2_APPLE_SDK
        *x = *x * SP->scaleVirtualToReal[0] + SP->virtualScreenOffsetX + SP->subScreenLeft;
#else
        *x = *x * SP->scaleVirtualToReal[0] + SP->subScreenLeft + SP->virtualScreenOffsetX;
#endif
        *w *= SP->scaleVirtualToReal[0];
        break;
    case 1:
        *x = *x * SP->scaleVirtualToReal[0] + SP->realViewableMin[0];
        *w *= SP->scaleVirtualToReal[0];
        break;
    case 2:
        *x = *x * SP->scaleVirtualToReal[0] + SP->realViewportSize[0] * 0.5f;
        *w *= SP->scaleVirtualToReal[0];
        break;
    case 3:
        *x = *x * SP->scaleVirtualToReal[0] + SP->realViewableMax[0];
        *w *= SP->scaleVirtualToReal[0];
        break;
    case 4:
        *x *= SP->scaleVirtualToFull[0];
        *w *= SP->scaleVirtualToFull[0];
        break;
    case 5:
        break;
    case 6:
        *x = *x * SP->scaleRealToVirtual[0] + SP->virtualScreenOffsetX;
        *w *= SP->scaleRealToVirtual[0];
        break;
    case 7:
        *x = *x * SP->scaleVirtualToReal[0] + (SP->realViewableMin[0] + SP->realViewableMax[0]) * 0.5f;
        *w *= SP->scaleVirtualToReal[0];
        break;
    }

    switch (vertAlign) {
    default:
        *y *= SP->scaleVirtualToReal[1];
        *h *= SP->scaleVirtualToReal[1];
        break;
    case 1:
        *y = *y * SP->scaleVirtualToReal[1] + SP->realViewableMin[1];
        *h *= SP->scaleVirtualToReal[1];
        break;
    case 2:
        *y = *y * SP->scaleVirtualToReal[1] + SP->realViewportSize[1] * 0.5f;
        *h *= SP->scaleVirtualToReal[1];
        break;
    case 3:
        *y = *y * SP->scaleVirtualToReal[1] + SP->realViewableMax[1];
        *h *= SP->scaleVirtualToReal[1];
        break;
    case 4:
        *y *= SP->scaleVirtualToFull[1];
        *h *= SP->scaleVirtualToFull[1];
        break;
    case 5:
        break;
    case 6:
        *y *= SP->scaleRealToVirtual[1];
        *h *= SP->scaleRealToVirtual[1];
        break;
    case 7:
        *y = *y * SP->scaleVirtualToReal[1] + (SP->realViewableMin[1] + SP->realViewableMax[1]) * 0.5f;
        *h *= SP->scaleVirtualToReal[1];
        break;
    }
}
