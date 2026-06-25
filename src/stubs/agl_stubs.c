#define _GNU_SOURCE

#include "agl_stubs.h"
#include <SDL2/SDL.h>

extern SDL_Window *sdl_gl_window;

int g_dip_is_tri = 0;
int g_dip_drawflag_zero = 0;
int g_dip_numelems_zero = 0;
int g_dip_gl_draw = 0;

int g_fp_enable_count = 0;
int g_fp_bind_count = 0;

AGLPixelFormat aglChoosePixelFormat(void *gdevs, int ndev, const int *attribs)
{
    return (AGLPixelFormat)0;
}

AGLContext aglCreateContext(AGLPixelFormat pix, AGLContext share)
{
    return (AGLContext)0;
}

int aglDestroyContext(AGLContext ctx)
{
    return 1;
}

int aglSetCurrentContext(AGLContext ctx)
{
    if (sdl_gl_window && ctx) {
        SDL_GL_MakeCurrent(sdl_gl_window, (SDL_GLContext)ctx);
    }
    return 1;
}

int aglSetDrawable(AGLContext ctx, AGLDrawable draw)
{
    return 1;
}

int aglSetFullScreen(AGLContext ctx, int width, int height, int freq, int device)
{
    return 1;
}

void aglDestroyPixelFormat(AGLPixelFormat pix)
{
}

int aglUpdateContext(AGLContext ctx)
{
    return 1;
}

void aglSwapBuffers(AGLContext ctx)
{
    (void)ctx;
    if (!sdl_gl_window)
        return;
    SDL_PumpEvents();
    SDL_GL_SwapWindow(sdl_gl_window);
}

AGLError aglGetError(void)
{
    return 0;
}

const char *aglErrorString(AGLError code)
{
    return "no error";
}

int aglSetInteger(AGLContext ctx, GLenum pname, const GLint *params)
{
    return 1;
}

int aglGetInteger(AGLContext ctx, GLenum pname, GLint *params)
{
    if (params)
        *params = 0;
    return 1;
}
