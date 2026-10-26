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
static byte written[30];
static int writes, jpegs, reads, frees;
static unsigned int pendingError;
static int failRead;
/* GL rows are bottom first. Distinct red and blue values expose channel swaps. */
static const byte rgba[] = { 11, 22, 33, 255, 44, 55, 66, 255,
                             77, 88, 99, 255, 111, 122, 133, 255 };
static const byte rgb[] = { 77, 88, 99, 111, 122, 133, 11, 22, 33, 44, 55, 66 };

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
        int viewport[] = {0, 0, 2, 2};
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
    assert(!x && !y && width == 2 && height == 2);
    assert(format == GL_RGBA && type == GL_UNSIGNED_BYTE);
    ++reads;
    if (failRead)
        pendingError = GL_INVALID_OPERATION;
    else
        memcpy(data, rgba, sizeof(rgba));
}
int MacDisplay_GetCardType(void) { return 0; }
int MacOpenGLUtils_GetPCPixelShaderVersion(void) { return 0; }
uint16_t MacDisplay_GetCurrentDimensions(int *width, int *height)
{ *width = 2; *height = 2; return 1; }
static void *Allocate(int size) { return malloc((size_t)size); }
static void Release(void *data) { ++frees; free(data); }
static void Print(int level, const char *format, ...) { (void)level; (void)format; }
static qboolean Write(const char *path, const void *data, int size)
{
    assert(!strcmp(path, "synthetic.tga") && size == sizeof(written));
    memcpy(written, data, sizeof(written));
    ++writes;
    return 1;
}
void R_SaveJpg(const char *path, int quality, int width, int height, byte *data)
{
    assert(!strcmp(path, "synthetic.jpg") && quality == 90 && width == 2 && height == 2);
    assert(!memcmp(data, rgb, sizeof(rgb)));
    ++jpegs;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    ri.Z_MallocInternal = Allocate;
    ri.Z_FreeInternal = Release;
    ri.Printf = Print;
    ri.FS_WriteFile = Write;
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
    } else {
        assert(!strcmp(argv[1], "read-failure"));
        failRead = 1;
        R_CaptureScreenshotNow((GfxScreenshotType)1, "synthetic.tga", 1);
        assert(reads == 2 && frees == 1 && !writes && !jpegs);
    }
    printf("screenshot %s: passed\n", argv[1]);
    return 0;
}
