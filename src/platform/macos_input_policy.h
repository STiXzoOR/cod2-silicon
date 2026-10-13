#ifndef COD2_MACOS_INPUT_POLICY_H
#define COD2_MACOS_INPUT_POLICY_H

static inline int MacInput_UseRaw(int active, int keyCatchers, int relative, int rawMode, int enabled, int available)
{
    return active && keyCatchers == 0 && relative && rawMode > 0 && enabled && available;
}

static inline int MacInput_WheelSteps(float *remainder, float delta)
{
    *remainder += delta;
    int steps = (int)*remainder;
    *remainder -= steps;
    return steps;
}

/* The menu cursor is drawn with full-screen alignment in 640x480 virtual
 * coordinates. The presentation transform has already produced render pixels. */
static inline void MacInput_RenderToUI(int x, int y, int width, int height, int *uiX, int *uiY)
{
    *uiX = width > 0 ? (int)((double)x * 640 / width) : 0;
    *uiY = height > 0 ? (int)((double)y * 480 / height) : 0;
}

#endif
