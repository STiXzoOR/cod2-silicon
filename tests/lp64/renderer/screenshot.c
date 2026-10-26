#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenshot_source.h"

int cod2_native_trace_enabled;
refimport_t ri;
vidConfig_t vidConfig;
r_global_permanent_t rgp;
static byte written[128 * 128 * 3 + 18];
static int writtenSize, allocationSize, pattern, commandCount;
static const char *commandArgs[3];
static byte *expectedPixels;
static int expectedWidth, expectedHeight;
static int writes, jpegs, reads, frees;
static unsigned int pendingError;
static int failRead;
/* GL rows are bottom first. Distinct red and blue values expose channel swaps. */
static const byte rgba[] = { 11, 22, 33, 255, 44, 55, 66, 255,
                             77, 88, 99, 255, 111, 122, 133, 255 };
static const byte rgb[] = { 77, 88, 99, 111, 122, 133, 11, 22, 33, 44, 55, 66 };

static byte Pattern(int x, int y, int channel)
{
    return (byte)(20 + (x * (7 + channel) + y * (3 + channel) + channel * 31) % 191);
}

/* Independent rational overlap/center sampling oracle. Each axis rounds before
   the next axis, as the Mac SSE/floorf stores bytes between passes. */
static byte Sample(const byte *pixels, int oldSize, int newSize, int stride, int index)
{
    int sum = 0;
    if (oldSize >= newSize) {
        int begin = index * oldSize, end = (index + 1) * oldSize;
        for (int i = begin / newSize; i < (end + newSize - 1) / newSize; ++i) {
            int left = begin > i * newSize ? begin : i * newSize;
            int right = end < (i + 1) * newSize ? end : (i + 1) * newSize;
            sum += pixels[i * stride] * (right - left);
        }
        return (byte)floorf((float)sum * (1.0f / oldSize) + 0.5f);
    }
    int denominator = 2 * newSize;
    int position = (2 * index + 1) * oldSize - newSize;
    if (position <= 0)
        return pixels[0];
    if (position >= (oldSize - 1) * denominator)
        return pixels[(oldSize - 1) * stride];
    int left = position / denominator, fraction = position % denominator;
    sum = pixels[left * stride] * (denominator - fraction) + pixels[(left + 1) * stride] * fraction;
    return (byte)floorf((float)sum * (0.5f / newSize) + 0.5f);
}

static void PrepareExpected(int width, int height)
{
    int oldWidth = vidConfig.width, oldHeight = vidConfig.height;
    byte *source = malloc((size_t)oldWidth * oldHeight * 3);
    byte *horizontal = malloc((size_t)width * oldHeight * 3);
    expectedPixels = malloc((size_t)width * height * 3);
    expectedWidth = width;
    expectedHeight = height;
    for (int y = 0; y < oldHeight; ++y)
        for (int x = 0; x < oldWidth; ++x)
            for (int c = 0; c < 3; ++c)
                source[(y * oldWidth + x) * 3 + c] = Pattern(x, y, c);
    for (int y = 0; y < oldHeight; ++y)
        for (int x = 0; x < width; ++x)
            for (int c = 0; c < 3; ++c)
                horizontal[(y * width + x) * 3 + c] = Sample(source + y * oldWidth * 3 + c, oldWidth, width, 3, x);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            for (int c = 0; c < 3; ++c)
                expectedPixels[(y * width + x) * 3 + c] = Sample(horizontal + x * 3 + c, oldHeight, height, width * 3, y);
    free(horizontal);
    free(source);
}

static void CheckPixels(const byte *actual, int bgr)
{
    for (int i = 0; i < expectedWidth * expectedHeight; ++i)
        for (int c = 0; c < 3; ++c) {
            int actualChannel = bgr ? 2 - c : c;
            if (actual[i * 3 + actualChannel] != expectedPixels[i * 3 + c])
                fprintf(stderr, "pixel %d channel %d: got %u expected %u\n", i, c,
                        actual[i * 3 + actualChannel], expectedPixels[i * 3 + c]);
            assert(actual[i * 3 + actualChannel] == expectedPixels[i * 3 + c]);
        }
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
        int viewport[] = {0, 0, vidConfig.width, vidConfig.height};
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
    assert(!x && !y && width == vidConfig.width && height == vidConfig.height);
    assert(format == GL_RGBA && type == GL_UNSIGNED_BYTE);
    ++reads;
    if (failRead)
        pendingError = GL_INVALID_OPERATION;
    else if (pattern) {
        byte *pixels = data;
        for (int row = 0; row < height; ++row)
            for (int col = 0; col < width; ++col) {
                for (int c = 0; c < 3; ++c)
                    pixels[(row * width + col) * 4 + c] = Pattern(col, height - 1 - row, c);
                pixels[(row * width + col) * 4 + 3] = 255;
            }
    } else
        memcpy(data, rgba, sizeof(rgba));
}
int MacDisplay_GetCardType(void) { return 0; }
int MacOpenGLUtils_GetPCPixelShaderVersion(void) { return 0; }
uint16_t MacDisplay_GetCurrentDimensions(int *width, int *height)
{ *width = vidConfig.width; *height = vidConfig.height; return 1; }
static void *Allocate(int size)
{
    allocationSize = size;
    void *data = malloc((size_t)size);
    memset(data, 0xa5, (size_t)size);
    return data;
}
static void Release(void *data) { ++frees; free(data); }
static void Print(int level, const char *format, ...) { (void)level; (void)format; }
static qboolean Write(const char *path, const void *data, int size)
{
    assert(!strcmp(path, pattern ? "levelshots/screenshot.tga" : "synthetic.tga"));
    assert(size == (pattern ? sizeof(written) : 30));
    writtenSize = size;
    memcpy(written, data, (size_t)size);
    ++writes;
    return 1;
}
void R_SaveJpg(const char *path, int quality, int width, int height, byte *data)
{
    assert(!strcmp(path, "synthetic.jpg") && quality == 90);
    if (pattern) {
        assert(width == expectedWidth && height == expectedHeight);
        CheckPixels(data, 0);
    } else {
        assert(width == 2 && height == 2);
        assert(!memcmp(data, rgb, sizeof(rgb)));
    }
    ++jpegs;
}
static int Argc(void) { return commandCount; }
static const char *Argv(int index)
{ return index < commandCount ? commandArgs[index] : ""; }
int Com_sprintf(char *dest, int size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, (size_t)size, format, args);
    va_end(args);
    return result;
}
void I_strncpyz(char *dest, const char *src, int size)
{
    assert(size > 0);
    snprintf(dest, (size_t)size, "%s", src);
}

static void ResampleCase(const char *name)
{
    pattern = 1;
    int width = 128, height = 128, header = 18;
    vidConfig.width = 1280;
    vidConfig.height = 720;
    if (!strcmp(name, "savegame")) {
        width = height = 512;
        header = 0;
    } else if (!strcmp(name, "noninteger-mixed")) {
        vidConfig.width = 640;
        vidConfig.height = 480;
        width = height = 512;
    } else if (!strcmp(name, "single-pixel")) {
        vidConfig.width = vidConfig.height = 1;
        width = 3;
        height = 5;
    } else if (!strcmp(name, "upsample")) {
        vidConfig.width = vidConfig.height = 2;
        width = height = 4;
    } else if (!strcmp(name, "shrink-x-expand-y")) {
        vidConfig.width = 8;
        vidConfig.height = 2;
        width = height = 4;
    } else if (!strcmp(name, "expand-x-shrink-y")) {
        vidConfig.width = 2;
        vidConfig.height = 8;
        width = height = 4;
    } else if (!strcmp(name, "same-size")) {
        vidConfig.width = width = 3;
        vidConfig.height = height = 2;
    } else if (!strcmp(name, "resample-failure")) {
        failRead = 1;
    } else {
        assert(!strcmp(name, "levelshot"));
    }
    PrepareExpected(width, height);
    if (!strcmp(name, "levelshot") || !strcmp(name, "savegame")) {
        commandCount = header ? 2 : 3;
        commandArgs[0] = "screenshotJPEG";
        commandArgs[1] = header ? "levelshot" : "savegame";
        commandArgs[2] = "synthetic";
        R_ScreenshotCommand((GfxScreenshotType)0);
        if (header) {
            assert(writes == 1 && !jpegs && writtenSize == 128 * 128 * 3 + 18);
            const byte tgaHeader[] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 24, 0};
            assert(!memcmp(written, tgaHeader, sizeof(tgaHeader)));
            /* Keep the original levelshot descriptor: its payload remains top-first. */
            CheckPixels(written + 18, 1);
        } else {
            assert(jpegs == 1 && !writes);
        }
    } else {
        byte *result = R_TakeResampledScreenshot(width, height, 3, header);
        if (failRead) {
            assert(!result && reads == 2 && frees == 1 && !writes && !jpegs);
        } else {
            assert(result);
            for (int i = 0; i < header; ++i)
                assert(result[i] == 0xa5);
            CheckPixels(result + header, 0);
            ri.Z_FreeInternal(result);
        }
    }
    int maxWidth = width > vidConfig.width ? width : vidConfig.width;
    int maxHeight = height > vidConfig.height ? height : vidConfig.height;
    assert(allocationSize == maxWidth * maxHeight * 3 + header);
    assert(frees == 1);
    free(expectedPixels);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    ri.Z_MallocInternal = Allocate;
    ri.Z_FreeInternal = Release;
    ri.Printf = Print;
    ri.FS_WriteFile = Write;
    ri.Cmd_Argc = Argc;
    ri.Cmd_Argv = Argv;
    vidConfig.width = vidConfig.height = 2;
    if (!strcmp(argv[1], "tga")) {
        R_CaptureScreenshotNow((GfxScreenshotType)1, "synthetic.tga", 1);
        assert(writes == 1 && reads == 1 && frees == 1 && !jpegs);
        const byte header[] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 24, 32};
        assert(!memcmp(written, header, sizeof(header)));
        /* Decode TGA's BGR pixels independently; descriptor 32 means top row first. */
        for (int i = 0; i < 4; ++i) {
            assert(written[18 + i * 3 + 2] == rgb[i * 3]);
            assert(written[18 + i * 3 + 1] == rgb[i * 3 + 1]);
            assert(written[18 + i * 3] == rgb[i * 3 + 2]);
        }
    } else if (!strcmp(argv[1], "jpeg")) {
        R_CaptureScreenshotNow((GfxScreenshotType)0, "synthetic.jpg", 1);
        assert(jpegs == 1 && reads == 1 && frees == 1 && !writes);
    } else if (!strcmp(argv[1], "read-failure")) {
        failRead = 1;
        R_CaptureScreenshotNow((GfxScreenshotType)1, "synthetic.tga", 1);
        assert(reads == 2 && frees == 1 && !writes && !jpegs);
    } else if (!strcmp(argv[1], "upsample-endpoint")) {
        byte source[12] = {20, 40, 60, 100, 120, 140};
        byte destination[12] = {0};
        const byte expected[] = {20, 40, 60, 40, 60, 80, 80, 100, 120, 100, 120, 140};
        R_UpsamplePixelData(2, 4, 1, 3, source, destination);
        assert(!memcmp(destination, expected, sizeof(expected)));
    } else if (!strcmp(argv[1], "downsample-boundary")) {
        byte source[] = {20, 40, 60, 40, 60, 80, 80, 100, 120, 100, 120, 140};
        byte destination[6] = {0};
        const byte expected[] = {30, 50, 70, 90, 110, 130};
        R_DownsamplePixelData(4, 2, 1, 3, source, destination);
        assert(!memcmp(destination, expected, sizeof(expected)));
    } else {
        ResampleCase(argv[1]);
    }
    printf("screenshot %s: passed\n", argv[1]);
    return 0;
}
