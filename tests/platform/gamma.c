#define SDL_GL_SwapWindow GammaTestSwap
#define SDL_GL_CreateContext GammaTestCreateContext
#define glClear GammaTestClear
#define glBlitFramebufferEXT GammaTestBlit
#include "../../src/platform/macos_display.c"
#undef SDL_GL_SwapWindow
#undef SDL_GL_CreateContext
#undef glClear
#undef glBlitFramebufferEXT
#include <assert.h>
#include <math.h>

void SDL_GL_SwapWindow(SDL_Window *window);
SDL_GLContext SDL_GL_CreateContext(SDL_Window *window);
void glClear(GLbitfield mask);
void glBlitFramebufferEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                          GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                          GLbitfield mask, GLenum filter);
static unsigned char presented[4];
static int forceSoftware;
static int traceIdentityPresentation;

static void TracePresentationState(const char *operation)
{
    GLint readFramebuffer, drawFramebuffer, readBuffer, drawBuffer, viewport[4];
    GLint backingSize[2] = { 0 }, fixedBacking = 0;
    int windowWidth, windowHeight, drawableWidth, drawableHeight;
    SDL_GetWindowSize(sdl_gl_window, &windowWidth, &windowHeight);
    SDL_GL_GetDrawableSize(sdl_gl_window, &drawableWidth, &drawableHeight);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &readFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &drawFramebuffer);
    glGetIntegerv(GL_READ_BUFFER, &readBuffer);
    glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    CGLError enabledResult = CGLIsEnabled(CGLGetCurrentContext(), kCGLCESurfaceBackingSize, &fixedBacking);
    CGLError sizeResult = CGLGetParameter(CGLGetCurrentContext(), kCGLCPSurfaceBackingSize, backingSize);
    fprintf(stderr, "gamma final identity %s: render=%dx%d window=%dx%d drawable=%dx%d "
            "read_fbo=%d draw_fbo=%d read_buffer=0x%x draw_buffer=0x%x viewport=%d,%d,%d,%d "
            "fixed_backing=%d backing=%dx%d cgl_results=%d,%d\n",
            operation, screenContext->width, screenContext->height, windowWidth, windowHeight,
            drawableWidth, drawableHeight, readFramebuffer, drawFramebuffer, readBuffer, drawBuffer,
            viewport[0], viewport[1], viewport[2], viewport[3], fixedBacking, backingSize[0], backingSize[1],
            enabledResult, sizeResult);
    fflush(stderr);
}

void GammaTestClear(GLbitfield mask)
{
    if (traceIdentityPresentation)
        TracePresentationState("clear begin");
    glClear(mask);
    if (traceIdentityPresentation) {
        fprintf(stderr, "gamma final identity clear returned: mask=0x%x\n", mask);
        fflush(stderr);
    }
}

void GammaTestBlit(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                   GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                   GLbitfield mask, GLenum filter)
{
    if (traceIdentityPresentation) {
        TracePresentationState("blit begin");
        fprintf(stderr, "gamma final identity blit arguments: src=%d,%d,%d,%d dst=%d,%d,%d,%d "
                "mask=0x%x filter=0x%x\n", srcX0, srcY0, srcX1, srcY1,
                dstX0, dstY0, dstX1, dstY1, mask, filter);
        fflush(stderr);
    }
    glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    if (traceIdentityPresentation) {
        fputs("gamma final identity blit returned\n", stderr);
        fflush(stderr);
    }
}

SDL_GLContext GammaTestCreateContext(SDL_Window *window)
{
    if (forceSoftware)
        assert(SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 0) == 0);
    return SDL_GL_CreateContext(window);
}

void GammaTestSwap(SDL_Window *window)
{
    GLint framebuffer, readBuffer;
    int width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &framebuffer);
    glGetIntegerv(GL_READ_BUFFER, &readBuffer);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
    glReadBuffer(GL_BACK);
    if (traceIdentityPresentation) {
        fputs("gamma final identity readback begin\n", stderr);
        fflush(stderr);
    }
    glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, presented);
    if (traceIdentityPresentation) {
        fputs("gamma final identity readback returned\n", stderr);
        fflush(stderr);
    }
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, framebuffer);
    glReadBuffer(readBuffer);
    if (traceIdentityPresentation) {
        fputs("gamma final identity swap begin\n", stderr);
        fflush(stderr);
    }
    SDL_GL_SwapWindow(window);
    if (traceIdentityPresentation) {
        fputs("gamma final identity swap returned\n", stderr);
        fflush(stderr);
    }
}

static void CheckPixel(int red, int green, int blue)
{
    printf("presented RGB=%u,%u,%u expected=%d,%d,%d\n", presented[0], presented[1], presented[2], red, green, blue);
    fflush(stdout);
    assert(abs((int)presented[0] - red) <= 1);
    assert(abs((int)presented[1] - green) <= 1);
    assert(abs((int)presented[2] - blue) <= 1);
    assert(glGetError() == GL_NO_ERROR);
}

static void CheckState(void)
{
    GLint viewport[4], activeTexture, program, binding;
    GLboolean mask[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    assert(viewport[0] == 5 && viewport[1] == 7 && viewport[2] == 11 && viewport[3] == 13);
    assert(activeTexture == GL_TEXTURE3 && binding != 0 && program != 0);
    assert(!mask[0] && mask[1] && !mask[2] && mask[3]);
    assert(glIsEnabled(GL_VERTEX_PROGRAM_ARB) && glIsEnabled(GL_FRAGMENT_PROGRAM_ARB));
    assert(glIsEnabled(GL_SCISSOR_TEST) && glIsEnabled(GL_DEPTH_TEST));
    assert(glIsEnabled(GL_STENCIL_TEST) && glIsEnabled(GL_CULL_FACE));
    assert(glIsEnabled(GL_BLEND) && glIsEnabled(GL_ALPHA_TEST));
    assert(glIsEnabled(GL_COLOR_LOGIC_OP) && glIsEnabled(GL_CLIP_PLANE0));
}

int main(int argc, char **argv)
{
    unsigned short ramp[768];
    forceSoftware = argc == 2 && strcmp(argv[1], "--software") == 0;
    MacPlatform_ConfigureWindow(320, 240, MAC_WINDOWED, 60);
    void *context = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
    assert(context);
    SDL_version runtimeVersion;
    SDL_GetVersion(&runtimeVersion);
    printf("SDL_VERSION=%u.%u.%u GL_VENDOR=%s GL_RENDERER=%s GL_VERSION=%s\n",
           runtimeVersion.major, runtimeVersion.minor, runtimeVersion.patch,
           glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
    fflush(stdout);
    if (forceSoftware)
        assert(strstr((const char *)glGetString(GL_RENDERER), "Software"));
    glClearColor(32.0f / 255, 128.0f / 255, 224.0f / 255, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindowDirect();
    CheckPixel(32, 128, 224);

    for (int i = 0; i < 256; ++i) {
        ramp[i] = 65535;
        ramp[i + 256] = 0;
        ramp[i + 512] = 32768;
    }
    assert(MacDisplay_SetGammaRamp(ramp) == 0);
    GLuint vertex = GammaCompile(GL_VERTEX_SHADER, "#version 120\nvoid main() { gl_Position = gl_Vertex; }");
    GLuint fragment = GammaCompile(GL_FRAGMENT_SHADER, "#version 120\nvoid main() { gl_FragColor = vec4(0, 1, 0, 1); }");
    GLuint program = glCreateProgram(), texture;
    assert(vertex && fragment);
    glAttachShader(program, vertex); glAttachShader(program, fragment);
    glLinkProgram(program); glUseProgram(program);
    glDeleteShader(vertex); glDeleteShader(fragment);
    glEnable(GL_VERTEX_PROGRAM_ARB); glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glEnable(GL_SCISSOR_TEST); glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST); glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND); glEnable(GL_ALPHA_TEST);
    glEnable(GL_COLOR_LOGIC_OP); glEnable(GL_CLIP_PLANE0);
    glScissor(0, 0, 1, 1);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glViewport(5, 7, 11, 13);
    glActiveTexture(GL_TEXTURE3);
    glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    assert(glGetError() == GL_NO_ERROR);
    SDL_GL_SwapWindowDirect();
    CheckPixel(255, 0, 128);
    CheckState();
    glUseProgram(0); glDeleteProgram(program);
    glDeleteTextures(1, &texture); glActiveTexture(GL_TEXTURE0);
    glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST);
    glDisable(GL_COLOR_LOGIC_OP); glDisable(GL_CLIP_PLANE0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, 320, 240);

    for (int i = 0; i < 256; ++i) {
        unsigned short value = (unsigned short)floor(pow(i / 255.0, 1 / 1.4) * 65535 + 0.5);
        ramp[i] = ramp[i + 256] = ramp[i + 512] = value;
    }
    assert(MacDisplay_SetGammaRamp(ramp) == 0);
    SDL_GL_SwapWindowDirect();
    CheckPixel((int)round(ramp[32] / 257.0), (int)round(ramp[128] / 257.0), (int)round(ramp[224] / 257.0));
    unsigned char scene[4];
    glReadPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, scene);
    assert(scene[0] == 32 && scene[1] == 128 && scene[2] == 224);
    SDL_GLContext originalContext = SDL_GL_GetCurrentContext();
    const unsigned char texel[4] = { 17, 53, 91, 255 };
    GLuint retainedTexture;
    glGenTextures(1, &retainedTexture);
    glBindTexture(GL_TEXTURE_2D, retainedTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    const int sizes[][2] = { { 400, 300 }, { 400, 300 }, { 320, 240 }, { 320, 240 } };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; ++i) {
        assert(MacDisplay_SetMode(sizes[i][0], sizes[i][1], 32, 60) == 0);
        assert(SDL_GL_GetCurrentContext() == originalContext);
        assert(glIsTexture(retainedTexture));
        glBindTexture(GL_TEXTURE_2D, retainedTexture);
        unsigned char retained[4];
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, retained);
        assert(memcmp(retained, texel, sizeof texel) == 0);
        glClearColor(32.0f / 255, 128.0f / 255, 224.0f / 255, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindowDirect();
        CheckPixel((int)round(ramp[32] / 257.0), (int)round(ramp[128] / 257.0), (int)round(ramp[224] / 257.0));
    }
    glDeleteTextures(1, &retainedTexture);

    for (int i = 0; i < 256; ++i)
        ramp[i] = ramp[i + 256] = ramp[i + 512] = (unsigned short)(i * 257);
    assert(MacDisplay_SetGammaRamp(ramp) == 0);
    traceIdentityPresentation = 1;
    fputs("gamma final identity presentation begin\n", stderr);
    fflush(stderr);
    SDL_GL_SwapWindowDirect();
    fputs("gamma final identity presentation returned\n", stderr);
    fflush(stderr);
    traceIdentityPresentation = 0;
    CheckPixel(32, 128, 224);
    MacDisplay_ReleaseContext(&context);
    MacDisplay_ReleaseDisplay();
    puts("presentation gamma: passed");
}
