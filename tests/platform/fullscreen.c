#include "macos_display.h"
#include <SDL2/SDL.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern SDL_Window *sdl_gl_window;

static void CheckBacking(int width, int height)
{
    int w, h;
    MacPlatform_GetDrawableSize(&w, &h);
    printf("drawable=%dx%d expected=%dx%d\n", w, h, width, height);
    assert(w == width && h == height);
    GLint size[2];
    assert(CGLGetParameter(CGLGetCurrentContext(), kCGLCPSurfaceBackingSize, size) == kCGLNoError);
    assert(size[0] == width && size[1] == height);
    GLint enabled;
    assert(CGLIsEnabled(CGLGetCurrentContext(), kCGLCESurfaceBackingSize, &enabled) == kCGLNoError);
    assert(enabled);
    /* Prove the default buffer really spans the selected size, not just a
     * cached size query: read the last pixel after a full-surface clear. */
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
    glClearColor(0.25f, 0.5f, 0.75f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    unsigned char pixel[4] = {0};
    glReadPixels(width - 1, height - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] >= 63 && pixel[0] <= 65 && pixel[1] >= 127 && pixel[1] <= 129);
    assert(glGetError() == GL_NO_ERROR);
}

int main(int argc, char **argv)
{
    const char *required[] = { "1920x1080", "2560x1440", "3008x1692",
        "3840x2160", "5120x2880", "6016x3384" };
    const char **names = MacPlatform_ModeNames();
    for (size_t i = 0; i < sizeof(required) / sizeof(*required); ++i) {
        int found = 0;
        for (int j = 0; names[j]; ++j)
            found |= strcmp(required[i], names[j]) == 0;
        assert(found);
    }
    if (argc > 1 && !strcmp(argv[1], "--modes"))
        return 0;
    int mode = argc > 1 && !strcmp(argv[1], "--exclusive") ? MAC_FULLSCREEN : MAC_BORDERLESS;
    MacPlatform_ConfigureWindow(1920, 1080, mode, 60);
    void *context = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
    assert(context);
    SDL_Delay(1000); SDL_PumpEvents();
    CheckBacking(1920, 1080);
    assert(MacDisplay_SetMode(2560, 1440, 32, 60) == 0);
    SDL_Delay(1000); SDL_PumpEvents();
    CheckBacking(2560, 1440);
    MacDisplay_ReleaseContext(&context);
    assert(!(SDL_GetWindowFlags(sdl_gl_window) & SDL_WINDOW_FULLSCREEN));
    MacDisplay_ReleaseDisplay();
    puts("fixed fullscreen backing and display release: passed");
    return 0;
}
