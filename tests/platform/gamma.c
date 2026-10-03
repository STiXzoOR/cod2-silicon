#define SDL_GL_SwapWindow GammaTestSwap
#include "../../src/platform/macos_display.c"
#undef SDL_GL_SwapWindow
#include <assert.h>
#include <math.h>

void SDL_GL_SwapWindow(SDL_Window *window);
static unsigned char presented[4];

void GammaTestSwap(SDL_Window *window)
{
    GLint framebuffer, readBuffer;
    int width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &framebuffer);
    glGetIntegerv(GL_READ_BUFFER, &readBuffer);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
    glReadBuffer(GL_BACK);
    glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, presented);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, framebuffer);
    glReadBuffer(readBuffer);
    SDL_GL_SwapWindow(window);
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

int main(void)
{
    unsigned short ramp[768];
    MacPlatform_ConfigureWindow(320, 240, MAC_WINDOWED, 60);
    void *context = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
    assert(context);
    printf("GL_VERSION=%s\n", glGetString(GL_VERSION));
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
    assert(MacDisplay_SetMode(400, 300, 32, 60) == 0);
    glClearColor(32.0f / 255, 128.0f / 255, 224.0f / 255, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindowDirect();
    CheckPixel((int)round(ramp[32] / 257.0), (int)round(ramp[128] / 257.0), (int)round(ramp[224] / 257.0));

    for (int i = 0; i < 256; ++i)
        ramp[i] = ramp[i + 256] = ramp[i + 512] = (unsigned short)(i * 257);
    assert(MacDisplay_SetGammaRamp(ramp) == 0);
    SDL_GL_SwapWindowDirect();
    CheckPixel(32, 128, 224);
    MacDisplay_ReleaseContext(&context);
    MacDisplay_ReleaseDisplay();
    puts("presentation gamma: passed");
}
