#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static const void *imp_colorWhite = white;
static sbpicinfo_t draws[6];
static int drawCount;
static float expectedAlpha;

MaterialHandle CL_RegisterMaterialNoMip(const char *name, int imageTrack)
{
    assert(!strcmp(name, "white") && imageTrack == 7);
    return (MaterialHandle)(uintptr_t)1;
}

void UI_DrawHandlePic(float x, float y, float w, float h, int horzAlign, int vertAlign,
                      const vec_t *color, MaterialHandle material)
{
    assert(drawCount < 6 && !horzAlign && !vertAlign);
    assert(material == (MaterialHandle)(uintptr_t)1);
    assert(color[0] == 1.0f && color[1] == 1.0f && color[2] == 1.0f);
    assert(color[3] == expectedAlpha);
    assert(x >= 120.0f && x + w <= 520.0f);
    assert(y >= 22.0f && y + h <= 452.0f);
    assert(w <= 2.0f || h <= 2.0f);
    draws[drawCount++] = (sbpicinfo_t){x, y, w, h};
}

#include "scoreboard_source.h"

int main(void)
{
    const float alphas[] = {0.1f, 0.05f, 0.0f};
    for (unsigned int frame = 0; frame < sizeof(alphas) / sizeof(alphas[0]); ++frame) {
        drawCount = 0;
        expectedAlpha = alphas[frame];
        draw_borders(expectedAlpha);
        assert(drawCount == 6);
        int horizontal = 0, vertical = 0;
        for (int i = 0; i < drawCount; ++i) {
            const sbpicinfo_t *rect = &draws[i];
            assert(isfinite(rect->x) && isfinite(rect->y));
            assert(rect->w > 0.0f && rect->h > 0.0f);
            /* Borders stay inside the existing 400x430 panel and are at most
             * two pixels thick. These geometric bounds do not reuse the
             * producer's rectangle coordinates as expected values. */
            assert(rect->x >= 120.0f && rect->x + rect->w <= 520.0f);
            assert(rect->y >= 22.0f && rect->y + rect->h <= 452.0f);
            assert(rect->w <= 2.0f || rect->h <= 2.0f);
            horizontal += rect->h <= 2.0f;
            vertical += rect->w <= 2.0f;
        }
        assert(horizontal == 4 && vertical == 2);
    }
    puts("PASS six scoreboard borders remain thin, inside the panel, and retain fade alpha");
    return 0;
}
