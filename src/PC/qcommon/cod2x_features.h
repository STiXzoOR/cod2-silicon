#ifndef COD2X_FEATURES_H
#define COD2X_FEATURES_H

#include <limits.h>
#include <math.h>

void Cod2x_FeaturesInit(void);
void Cod2x_FeaturesFrame(int active, int demo);
int Cod2x_ThirdPersonMode(void);
int Cod2x_DebugBullets(void);
int Cod2x_PrintDoubleColors(void);
int Cod2x_DrawSpectatedName(void);
int Cod2x_DrawCompass(void);
void Cod2x_CompassOffset(float *x, float *y);
int Cod2x_FeaturesDemo(void);
void CG_Cod2xRadarInit(void);
void CG_Cod2xRadarDraw(void);
void CG_Cod2xRadarFire(int entityNum);

static inline int Cod2x_StepInt(int value, int direction)
{
    if ((direction > 0 && value == INT_MAX) || (direction < 0 && value == INT_MIN))
        return value;
    return value + (direction > 0 ? 1 : -1);
}

static inline float Cod2x_StepFloat(float value, int direction)
{
    return value + (direction > 0 ? 1.0f : -1.0f);
}

static inline int Cod2x_NextConsoleChar(const char **text, int fixedColor, int doubleColors, int *color)
{
    const char *cursor = *text;
    while (*cursor) {
        if (*cursor == '^') {
            if (!doubleColors) {
                int carets = 0;
                do {
                    ++carets;
                    ++cursor;
                } while (*cursor == '^');
                while (carets-- > 0 && *cursor >= '0' && *cursor <= '9')
                    ++cursor;
                continue;
            }
            if (cursor[1] >= '0' && cursor[1] <= '9') {
                if (fixedColor < 0)
                    *color = cursor[1] - '0';
                cursor += 2;
                continue;
            }
        }
        {
            unsigned char ch = (unsigned char)*cursor++;
            if (ch == '\r' || ch == '\n')
                continue;
            *text = cursor;
            return ch;
        }
    }
    *text = cursor;
    return -1;
}

static inline void Cod2x_OrbitView(const float origin[3], float angles[3], float range, float orbit, float view[3])
{
    const float radians = 0.017453292519943295f;
    float pitch, yaw;
    angles[0] *= 0.5f;
    angles[1] -= orbit;
    pitch = angles[0] * radians;
    yaw = angles[1] * radians;
    view[0] = origin[0] - range * cosf(pitch) * cosf(yaw);
    view[1] = origin[1] - range * cosf(pitch) * sinf(yaw);
    view[2] = origin[2] + 8.0f + range * sinf(pitch);
    angles[0] *= 1.2f;
}

static inline int Cod2x_RadarCalibrate(const float world1[2], const float world2[2],
                                     const float image1[2], const float image2[2],
                                     float *scale, float *offsetX, float *offsetY)
{
    float dx = world2[0] - world1[0], dy = world2[1] - world1[1];
    float ix = image2[0] - image1[0], iy = image2[1] - image1[1];
    float distance = sqrtf(dx * dx + dy * dy);
    if (distance <= 0 || !isfinite(distance))
        return 0;
    *scale = sqrtf(ix * ix + iy * iy) / (4.0f * distance);
    *offsetX = image1[0] * 0.25f - world1[0] * *scale;
    *offsetY = image1[1] * 0.25f + world1[1] * *scale - 100.0f;
    return isfinite(*scale) && isfinite(*offsetX) && isfinite(*offsetY) && *scale > 0;
}

static inline void Cod2x_RadarPoint(float worldX, float worldY, float entityScale,
                                 float entityOffsetX, float entityOffsetY, float scale,
                                 float radarX, float radarY, float imageOffsetX, float imageOffsetY,
                                 int rotation, float *x, float *y)
{
    float factor = scale * 2.0f;
    float centerX = radarX + 50.0f * factor, centerY = radarY + 50.0f * factor;
    float px = radarX + (worldX * entityScale + entityOffsetX) * factor - centerX;
    float py = radarY + (-worldY * entityScale + entityOffsetY + 100.0f) * factor - centerY;
    switch (rotation) {
    case 90: *x = -py; *y = px; break;
    case -90: *x = py; *y = -px; break;
    case 180: case -180: *x = -px; *y = -py; break;
    default: *x = px; *y = py; break;
    }
    *x += centerX + imageOffsetX;
    *y += centerY + imageOffsetY;
}

#endif
