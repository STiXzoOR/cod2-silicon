#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdlib.h>
#include "Mac/DirectX_9/lp64_shader_state.h"

static void Triangle(int clockwise)
{
    glBegin(GL_TRIANGLES);
    glVertex2f(-1, -1);
    if (clockwise) { glVertex2f(0, 1); glVertex2f(1, -1); }
    else { glVertex2f(1, -1); glVertex2f(0, 1); }
    glEnd();
}

static void Cull(DWORD mode, int clockwise, int visible)
{
    byte pixel[4];
    MacShader_ApplyRasterEquations(mode, 1, 1);
    glDisable(GL_BLEND);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(1, 0, 0, 1);
    Triangle(clockwise);
    glReadPixels(16, 12, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == (visible ? 255 : 0));
    assert(pixel[1] == 0 && pixel[2] == 0);
}

static void Blend(DWORD rgb, DWORD alpha, const float *expected)
{
    byte pixel[4];
    MacShader_ApplyRasterEquations(1, rgb, alpha);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
    glClearColor(.2f, .3f, .4f, .5f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(.6f, .4f, .2f, .75f);
    Triangle(0);
    glReadPixels(16, 12, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    for (int i = 0; i < 4; ++i)
        assert(abs((int)pixel[i] - (int)(expected[i] * 255 + .5f)) <= 2);
}

int main(void)
{
    CGLPixelFormatAttribute attrs[] = {kCGLPFAAccelerated, kCGLPFAColorSize, 24, 0};
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    glViewport(0, 0, 32, 32);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    for (DWORD mode = 1; mode <= 3; ++mode) {
        Cull(mode, 0, mode != 3);
        Cull(mode, 1, mode != 2);
    }
    const float expected[5][4] = {{.8f,.7f,.6f,1}, {.4f,.1f,0,.25f},
        {0,0,.2f,0}, {.2f,.3f,.2f,.5f}, {.6f,.4f,.4f,.75f}};
    for (DWORD operation = 1; operation <= 5; ++operation)
        Blend(operation, operation, expected[operation - 1]);
    const float separate[] = {.8f,.7f,.6f,0};
    Blend(1, 3, separate);
    assert(glGetError() == GL_NO_ERROR);
    glDeleteFramebuffersEXT(1, &framebuffer);
    glDeleteTextures(1, &target);
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(context);
    puts("native raster state: both windings, five equations and separate alpha passed");
}
