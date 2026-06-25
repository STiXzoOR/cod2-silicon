#include "common_types.h"
#include "imports.h"

extern void CL_DrawStretchPic(float x, float y, float w, float h, int horzAlign, int vertAlign, float s0, float t0, float s1, float t1, const vec_t *color, MaterialHandle material);
extern void CL_DrawStretchPicPhysical(float x, float y, float w, float h, float s1, float t1, float s2, float t2, const vec_t *color, MaterialHandle material);
extern int Dvar_GetInt(const char *dvarName);
extern int Hunk_Used(void);

extern sharedUiInfo_t sharedUiInfo;
extern void *imp_sharedUiInfo;

void UI_DrawHandlePic(float x, float y, float w, float h, int horzAlign, int vertAlign, const vec_t *color, MaterialHandle hMaterial);
void UI_DrawLoadBar(float x, float y, float w, float h, int horzAlign, int vertAlign, const vec_t *color, MaterialHandle material);
void UI_FillRectPhysical(float x, float y, float width, float height, const vec_t *color);
void UI_FillRect(float x, float y, float width, float height, int horzAlign, int vertAlign, const vec_t *color);

void UI_DrawHandlePic(float x, float y, float w, float h, int horzAlign, int vertAlign, const vec_t *color, MaterialHandle hMaterial)
{
    float s0, t0, s1, t1;

    if (w < 0.0f) {

        w = -w;
        s0 = 1.0f;
        s1 = 0.0f;
    } else {
        s0 = 0.0f;
        s1 = 1.0f;
    }

    if (h < 0.0f) {

        h = -h;
        t0 = 1.0f;
        t1 = 0.0f;
    } else {
        t0 = 0.0f;
        t1 = 1.0f;
    }

    CL_DrawStretchPic(x, y, w, h, horzAlign, vertAlign, s0, t0, s1, t1, color, hMaterial);
}

void UI_DrawLoadBar(float x, float y, float w, float h, int horzAlign, int vertAlign, const vec_t *color, MaterialHandle material)
{
    int expectedHunk;
    float frac;

    expectedHunk = Dvar_GetInt("com_expectedHunkUsage");

    frac = 0.0f;

    if (expectedHunk > 0) {

        frac = (float)Hunk_Used() / (float)expectedHunk;

        if (frac > 1.0f)
            frac = 1.0f;
    }

    CL_DrawStretchPic(x, y, frac * w, h, horzAlign, vertAlign, 0.0f, 0.0f, frac, 1.0f, color, material);
}

void UI_FillRectPhysical(float x, float y, float width, float height, const vec_t *color)
{
    MaterialHandle whiteMaterial;

    whiteMaterial = sharedUiInfo.assets.whiteMaterial;

    if (whiteMaterial) {

        CL_DrawStretchPicPhysical(x, y, width, height, 0.0f, 0.0f, 0.0f, 0.0f, color, whiteMaterial);
    }
}

void UI_FillRect(float x, float y, float width, float height, int horzAlign, int vertAlign, const vec_t *color)
{
    MaterialHandle whiteMaterial;

    whiteMaterial = sharedUiInfo.assets.whiteMaterial;

    if (whiteMaterial) {

        CL_DrawStretchPic(x, y, width, height, horzAlign, vertAlign, 0.0f, 0.0f, 0.0f, 0.0f, color, whiteMaterial);
    }
}
