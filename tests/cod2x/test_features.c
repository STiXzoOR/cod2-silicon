#include "cod2x_features.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void console_text(const char *text, int fixed, int doubleColors, char *out, int *colors)
{
    int ch, count = 0, color = fixed < 0 ? 7 : fixed;
    while ((ch = Cod2x_NextConsoleChar(&text, fixed, doubleColors, &color)) >= 0) {
        out[count] = (char)ch;
        colors[count++] = color;
    }
    out[count] = '\0';
}

int main(void)
{
    char text[80];
    int colors[80];
    float origin[3] = {10, 20, 30}, angles[3] = {0, 0, 0}, view[3];
    float world1[2] = {100, 200}, world2[2] = {300, 200};
    float image1[2] = {80, 240}, image2[2] = {240, 240};
    float mapScale, offsetX, offsetY, x, y;

    assert(Cod2x_StepInt(12, 1) == 13);
    assert(Cod2x_StepInt(12, -1) == 11);
    assert(Cod2x_StepInt(INT_MAX, 1) == INT_MAX);
    assert(Cod2x_StepInt(INT_MIN, -1) == INT_MIN);
    assert(Cod2x_StepFloat(1.25f, 1) == 2.25f);
    assert(Cod2x_StepFloat(1.25f, -1) == 0.25f);

    console_text("^^12Bob\r\n", 4, 0, text, colors);
    assert(!strcmp(text, "Bob"));
    assert(colors[0] == 4);
    console_text("^^12Bob", -1, 1, text, colors);
    assert(!strcmp(text, "^2Bob"));
    assert(colors[0] == 7 && colors[1] == 1);
    console_text("^2A^7B", -1, 1, text, colors);
    assert(!strcmp(text, "AB"));
    assert(colors[0] == 2 && colors[1] == 7);
    console_text("^2A^7B", 4, 1, text, colors);
    assert(!strcmp(text, "AB"));
    assert(colors[0] == 4 && colors[1] == 4);
    console_text("^^carets", 7, 0, text, colors);
    assert(!strcmp(text, "carets"));

    Cod2x_OrbitView(origin, angles, 120, 0, view);
    assert(view[0] == -110 && view[1] == 20 && view[2] == 38);
    angles[0] = 60;
    angles[1] = 90;
    Cod2x_OrbitView(origin, angles, 0, 45, view);
    assert(fabsf(angles[0] - 36) < 0.001f && angles[1] == 45);
    assert(Cod2x_RadarCalibrate(world1, world2, image1, image2, &mapScale, &offsetX, &offsetY));
    assert(fabsf(mapScale - .2f) < .00001f);
    assert(fabsf(offsetX) < .00001f && fabsf(offsetY) < .00001f);
    assert(!Cod2x_RadarCalibrate(world1, world1, image1, image2, &mapScale, &offsetX, &offsetY));
    Cod2x_RadarPoint(100, 200, .2f, 0, 0, 1, 5, 20, 0, 0, 0, &x, &y);
    assert(fabsf(x - 45) < .00001f && fabsf(y - 140) < .00001f);
    Cod2x_RadarPoint(100, 200, .2f, 0, 0, 1, 5, 20, 0, 0, 90, &x, &y);
    assert(fabsf(x - 85) < .00001f && fabsf(y - 60) < .00001f);
    puts("cod2x client feature logic: passed");
    return 0;
}
