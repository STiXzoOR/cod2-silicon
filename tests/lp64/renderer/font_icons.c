#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "font_source.h"

refimport_t ri;
static Material material;
static int selected = -1, draws;
static float drawnWidth, height, sLeft, sRight;
static vec4_t drawnColor;
static MaterialHandle drawnMaterial;
int ColorIndex(int c) { return c - '0'; }
const char *CL_GetHudMsgIconMaterialName(int index)
{ selected = index; return "synthetic_icon"; }
MaterialHandle Material_RegisterHandle(const char *name, int lightmapIndex, int imageTrack)
{ assert(!strcmp(name, "synthetic_icon") && lightmapIndex == 0 && imageTrack == 7); return &material; }
void R_AddCmdDrawTextWithCursor(const char *text, int maxChars, FontHandle font,
    float x, float y, float xScale, float yScale, const vec_t *color, int style,
    int cursorPos, int cursor)
{ assert(!"icon-only input must not submit text"); }
void R_AddCmdDrawStretchPic(float x, float y, float w, float h, float s0,
    float t0, float s1, float t1, const vec_t *color, MaterialHandle mat)
{
    assert(x == 10 && t0 == 0 && t1 == 1);
    ++draws; drawnWidth = w; height = h; sLeft = s0; sRight = s1;
    memcpy(drawnColor, color, sizeof(drawnColor)); drawnMaterial = mat;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    Font font = {0}; font.pixelHeight = 16;
    const vec4_t color = {1,1,1,0.5f};
    /* High byte is command; low byte is its independent operand. */
    const short commands[] = {0x1040,0x1130,0x0d80,0x0e40,0x0fc0,0x1320,0x1205};
    if (!strcmp(argv[1], "measure")) {
        assert(R_ConsoleTextWidth(commands, 7, &font) == 2);
        assert(selected == 5);
    } else {
        R_DrawConsoleText(commands, 7, &font, 10, 40, 1, 1, color, 0);
        assert(draws == 1);
        if (!strcmp(argv[1], "material")) assert(selected == 5 && drawnMaterial == &material);
        if (!strcmp(argv[1], "width")) assert(drawnWidth == 32);
        if (!strcmp(argv[1], "height")) assert(height == 24);
        if (!strcmp(argv[1], "flip")) assert(sLeft == 1 && sRight == 0);
        if (!strcmp(argv[1], "color")) {
            assert(drawnColor[0] == 128 * 0.003921568859368563f);
            assert(drawnColor[1] == 64 * 0.003921568859368563f);
            assert(drawnColor[2] == 192 * 0.003921568859368563f);
            assert(drawnColor[3] == 0.5f);
        }
    }
    puts("console icon command and operand passed");
    return 0;
}
