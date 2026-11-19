/* The game window's handle, for the Windows APIs that bind to one. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <windows.h>

extern SDL_Window *sdl_gl_window;

void *Win_GameWindow(void)
{
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (sdl_gl_window && SDL_GetWindowWMInfo(sdl_gl_window, &info))
        return info.info.win.window;
    return GetDesktopWindow();
}
