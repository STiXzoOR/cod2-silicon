/* Synthetic target coverage using production filter quads and projection. */
#define main ImageFilterMathMain
#include "imagefilter.c"
#undef main
#include "Mac/DirectX_9/MacOpenGLUtils.c"

int MacDisplay_GetCardType(void) { return 0; }

static void CheckCoverage(int width, int height, int glow)
{
    Init();
    for (int i = 0; i < R_RENDERTARGET_COUNT; ++i) {
        TARGETS[i].width = width;
        TARGETS[i].height = height;
    }
    VIDEO.width = width;
    VIDEO.height = height;
    view.viewport.Height = height;
    if (glow) {
        float radii[] = { 2, 0 };
        RB_GlowFilterImage(radii);
    } else {
        RB_GaussianFilterImage(2, R_RENDERTARGET_BLURRED_SCREEN);
    }
    assert(drawCount > 0);
    for (int i = 0; i < drawCount; ++i) {
        int w = draws[i].width, h = draws[i].height;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glViewport(0, 0, w, h);
        float matrix[16] = { 0 };
        matrix[0] = 2.0f / w;
        matrix[5] = -2.0f / h;
        matrix[10] = matrix[15] = 1;
        matrix[12] = -1;
        matrix[13] = 1;
        MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(matrix, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(matrix);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glColor4f(1, 0, 0, 1);
        float x = draws[i].x, y = draws[i].y;
        glBegin(GL_TRIANGLES);
        glVertex2f(x, y + h); glVertex2f(x, y); glVertex2f(x + w, y + h);
        glVertex2f(x + w, y + h); glVertex2f(x, y); glVertex2f(x + w, y);
        glEnd();
        const int points[][2] = { {0, 0}, {w - 1, 0}, {0, h - 1}, {w - 1, h - 1}, {w / 2, h - 1} };
        for (int p = 0; p < 5; ++p) {
            byte pixel[4];
            glReadPixels(points[p][0], points[p][1], 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            if (pixel[0] != 255)
                fprintf(stderr, "%s %dx%d pass %d leaves pixel (%d,%d) uncovered\n",
                        glow ? "glow" : "blur", w, h, i, points[p][0], points[p][1]);
            assert(pixel[0] == 255);
        }
    }
}

int main(void)
{
    CGLPixelFormatAttribute attrs[] = { kCGLPFAAccelerated, kCGLPFAColorSize, 24, 0 };
    CGLPixelFormatObj format;
    CGLContextObj context;
    GLint count;
    GLuint target, framebuffer;
    assert(!CGLChoosePixelFormat(attrs, &format, &count));
    assert(!CGLCreateContext(format, NULL, &context));
    CGLDestroyPixelFormat(format);
    assert(!CGLSetCurrentContext(context));
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const int sizes[][2] = { {320, 180}, {321, 181}, {640, 360}, {1280, 720} };
    for (int i = 0; i < 4; ++i) {
        CheckCoverage(sizes[i][0], sizes[i][1], 0);
        CheckCoverage(sizes[i][0], sizes[i][1], 1);
    }
    assert(glGetError() == GL_NO_ERROR);
    glDeleteFramebuffersEXT(1, &framebuffer);
    glDeleteTextures(1, &target);
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(context);
    puts("native filter raster: blur/glow cover target edges at four even/odd extents");
    return 0;
}
