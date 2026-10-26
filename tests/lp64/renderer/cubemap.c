#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DxGlobals dx;
void *imp_dx = &dx;

#define pow ObservedPow
#include "screenshot_source.h"
#undef pow

refimport_t ri;
vidConfig_t vidConfig;
int alwaysfails, cod2_native_trace_enabled;
static dvar_t overbright;
static dvar_t *overbrightPointer = &overbright;
void *imp_r_overbrightBits = &overbrightPointer;
static void *vtable[44];
static struct { void **vtable; } device = {vtable};
static int face = 1, lighting, failRead, reads, writes, clears, resets, gammaCalls;
static unsigned int pendingError;
static int allocationCount, freeCount, powCalls;
static struct { void *data; int size, live; } allocations[64];
static byte saved[6][4 * 4 * 4 + 18];

static byte Color(int channel, int x, int y)
{
    const byte constant[] = {191, 127, 63};
    return lighting ? constant[channel] : (byte)(face * 20 + channel * 7 + x + y * 3);
}
void glPushAttrib(GLbitfield mask) { (void)mask; }
void glPushClientAttrib(GLbitfield mask) { (void)mask; }
void glPopClientAttrib(void) {}
void glPopAttrib(void) {}
void glDisable(GLenum cap) { (void)cap; }
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{ (void)r; (void)g; (void)b; (void)a; }
void glDepthMask(GLboolean mask) { (void)mask; }
void glActiveTextureARB(GLenum unit) { (void)unit; }
void glReadBuffer(GLenum mode) { assert(mode == GL_BACK || mode == GL_FRONT); }
GLenum glGetError(void) { GLenum result = pendingError; pendingError = 0; return result; }
void glGetIntegerv(GLenum name, GLint *value)
{
    if (name == GL_VIEWPORT) {
        int viewport[] = {0, 0, 16, 16};
        memcpy(value, viewport, sizeof(viewport));
    } else if (name == GL_MAX_TEXTURE_UNITS) {
        *value = 0;
    } else {
        assert(name == GL_READ_BUFFER || name == GL_DRAW_BUFFER);
        *value = GL_BACK;
    }
}
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, GLvoid *data)
{
    /* Mac ignores the cubemap border and reads the requested square at x=0. */
    assert(x == 0 && y == 12 && width == 4 && height == 4);
    assert(format == GL_RGBA && type == GL_UNSIGNED_BYTE);
    ++reads;
    if (failRead) {
        pendingError = GL_INVALID_OPERATION;
        return;
    }
    byte *pixels = data;
    for (int row = 0; row < height; ++row)
        for (int col = 0; col < width; ++col) {
            for (int c = 0; c < 3; ++c)
                pixels[(row * width + col) * 4 + c] = Color(c, col, row);
            pixels[(row * width + col) * 4 + 3] = 217;
        }
}
int MacDisplay_GetCardType(void) { return 0; }
int MacOpenGLUtils_GetPCPixelShaderVersion(void) { return 0; }
uint16_t MacDisplay_GetCurrentDimensions(int *width, int *height)
{ *width = *height = 16; return 1; }
void RB_SetRenderTarget(int target) { assert(target == 0); ++resets; }
static void D3DVTCC Clear(void *self, int count, int rectangles, int flags,
                        int color, float depth, int stencil)
{
    assert(self == &device && !count && !rectangles && flags == 7);
    assert(color == (int)0xffff00ff && depth == 1 && stencil == 0);
    ++clears;
}
static void *Allocate(int size)
{
    assert(allocationCount < 64);
    void *data = malloc((size_t)size);
    memset(data, 0xa5, (size_t)size);
    allocations[allocationCount++] = (typeof(allocations[0])){data, size, 1};
    return data;
}
static void Release(void *data)
{
    for (int i = allocationCount - 1; i >= 0; --i)
        if (allocations[i].data == data && allocations[i].live) {
            allocations[i].live = 0;
            ++freeCount;
            free(data);
            return;
        }
    assert(!"unowned allocation or double free");
}
static void Print(int level, const char *format, ...) { (void)level; (void)format; }
static qboolean Write(const char *path, const void *data, int size)
{
    assert(!strcmp(path, "synthetic.tga") && size == sizeof(saved[0]));
    assert(face >= 1 && face <= 6);
    memcpy(saved[face - 1], data, (size_t)size);
    ++writes;
    return 1;
}
void R_GammaCorrect(byte *data, int size)
{
    assert(size == 4 * 4 * 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            for (int c = 0; c < 3; ++c)
                assert(data[(y * 4 + x) * 4 + (2 - c)] == Color(c, x, y));
    ++gammaCalls;
}
double ObservedPow(double value, double exponent)
{
    assert(lighting && isfinite(value) && value >= 0);
    if (powCalls < 6 * 16 * 3) {
        assert(exponent == (double)2.2f);
        assert(value == (double)((float)Color(powCalls % 3, 0, 0) * (1.0f / 255.0f)));
    } else {
        assert(exponent == (double)0.45454545f);
    }
    ++powCalls;
    return pow(value, exponent);
}
static void Capture(void)
{
    R_BeginCubemapShot(4, 1);
    assert(dx->cubemapShotRes == 4 && dx->cubemapShotPixelBorder == 1);
    R_EndCubemapShot((CubemapShot)face);
}
static void Direction(int index, float u, float v, float *direction)
{
    /* Face convention: up, down, front, back, right, left. */
    switch (index) {
    case 1: direction[0] = u; direction[1] = v; direction[2] = 2; break;
    case 2: direction[0] = -u; direction[1] = v; direction[2] = -2; break;
    case 3: direction[0] = -u; direction[1] = 2; direction[2] = v; break;
    case 4: direction[0] = u; direction[1] = -2; direction[2] = v; break;
    case 5: direction[0] = 2; direction[1] = u; direction[2] = v; break;
    case 6: direction[0] = -2; direction[1] = -u; direction[2] = v; break;
    default: abort();
    }
}
static void SaveAndCheck(int expectedLight)
{
    R_SaveCubemapShot("synthetic.tga", (CubemapShot)face, 1.0f, 1.333f);
    const byte header[] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 4, 0, 32, 0};
    const byte *data = saved[face - 1];
    assert(!memcmp(data, header, sizeof(header)));
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            int offset = 18 + (y * 4 + x) * 4;
            for (int c = 0; c < 3; ++c) {
                int expected = lighting ? expectedLight : Color(c, x, y);
                if (lighting && expectedLight < 0) {
                    /* Mac hemisphere averages of constant RGB(191,127,63),
                       at corner, edge, and interior pixel directions. */
                    const byte gray[3][3] = {{138, 92, 46}, {139, 92, 46}, {140, 93, 46}};
                    int interiorAxes = (x == 1 || x == 2) + (y == 1 || y == 2);
                    expected = gray[interiorAxes][c];
                }
                assert(data[offset + 2 - c] == expected);
            }
            float direction[3];
            Direction(face, x - 1.5f, y - 1.5f, direction);
            double cosIncident = fabs(direction[2]) / sqrt(direction[0] * direction[0]
                + direction[1] * direction[1] + direction[2] * direction[2]);
            double n1 = (double)1.333f;
            double cosTransmit = sqrt(1 - (1 - cosIncident * cosIncident) / (n1 * n1));
            double s = (cosIncident - n1 * cosTransmit) / (cosIncident + n1 * cosTransmit);
            double p = (n1 * cosIncident - cosTransmit) / (n1 * cosIncident + cosTransmit);
            int alpha = (int)floor((s * s + p * p) * 0.5 * 255 + 0.5);
            assert(data[offset + 3] == alpha);
        }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    ri.Z_MallocInternal = Allocate;
    ri.Z_FreeInternal = Release;
    ri.Printf = Print;
    ri.FS_WriteFile = Write;
    dx->device = (void *)&device;
    vtable[0xac / 4] = (void *)Clear;
    if (!strcmp(argv[1], "begin")) {
        R_BeginCubemapShot(4, 1);
        assert(clears == 1 && resets == 1 && !allocationCount);
    } else if (!strcmp(argv[1], "basis")) {
        for (int index = 1; index <= 6; ++index) {
            float expected[3];
            Direction(index, 0.5f, -1.5f, expected);
            for (int axis = 0; axis < 3; ++axis)
                assert(cubemapShotAxis[index][0][axis] * 2
                    + cubemapShotAxis[index][1][axis] * 0.5f
                    + cubemapShotAxis[index][2][axis] * -1.5f == expected[axis]);
        }
    } else if (!strcmp(argv[1], "invalid-index")) {
        R_EndCubemapShot((CubemapShot)0);
        R_EndCubemapShot((CubemapShot)7);
        R_SaveCubemapShot("synthetic.tga", (CubemapShot)0, 1, 1.333f);
        R_SaveCubemapShot("synthetic.tga", (CubemapShot)7, 1, 1.333f);
        assert(!allocationCount && !writes && !reads);
    } else if (!strcmp(argv[1], "read-failure")) {
        failRead = 1;
        Capture();
        assert(reads == 2 && freeCount == 1 && resets == 2);
        R_SaveCubemapShot("synthetic.tga", (CubemapShot)face, 1, 1.333f);
        assert(!writes && allocationCount == 1);
        failRead = 0;
        Capture();
        SaveAndCheck(0);
        assert(writes == 1);
    } else if (!strcmp(argv[1], "incomplete-lighting")) {
        Capture();
        face = 2;
        failRead = 1;
        Capture();
        assert(allocationCount == 2 && freeCount == 1);
        const vec3_t base = {1, 1, 1};
        R_LightingFromCubemapShots(base);
        assert(allocationCount == 2 && freeCount == 2 && !powCalls);
        for (face = 1; face <= 6; ++face)
            R_SaveCubemapShot("synthetic.tga", (CubemapShot)face, 1, 1.333f);
        assert(!writes);
    } else if (!strcmp(argv[1], "replace-face")) {
        Capture();
        Capture();
        assert(freeCount == 1);
        SaveAndCheck(0);
        R_SaveCubemapShot("synthetic.tga", (CubemapShot)face, 1, 1.333f);
        assert(writes == 1);
    } else {
        lighting = !strncmp(argv[1], "lighting-", 9);
        assert(lighting || !strcmp(argv[1], "all-faces") || !strcmp(argv[1], "gamma"));
        overbright.current.integer = !strcmp(argv[1], "gamma");
        vidConfig.deviceSupportsGamma = overbright.current.integer;
        for (face = 1; face <= 6; ++face)
            Capture();
        assert(allocationCount == 6 && freeCount == 0 && reads == 6);
        for (int i = 0; i < 6; ++i) {
            assert(allocations[i].size == 64 && allocations[i].live);
            for (int j = 0; j < i; ++j)
                assert(allocations[i].data != allocations[j].data);
        }
        int expectedLight = !strcmp(argv[1], "lighting-white") ? 255 : 0;
        if (!strcmp(argv[1], "lighting-gray"))
            expectedLight = -1;
        if (lighting) {
            const float color = expectedLight < 0 ? 1 : expectedLight ? 100 : 0;
            const vec3_t base = {color, color, color};
            R_LightingFromCubemapShots(base);
            assert(powCalls == 2 * 6 * 16 * 3 && freeCount == 1);
        }
        for (face = 1; face <= 6; ++face)
            SaveAndCheck(expectedLight);
        assert(writes == 6 && gammaCalls == (overbright.current.integer ? 6 : 0));
    }
    assert(allocationCount == freeCount);
    puts(argv[1]);
}
