#import <AppKit/AppKit.h>
#include <SDL.h>
#include <SDL_syswm.h>

/* SDL2 compatibility drops SDL3's occluded event. Query Cocoa after the event
 * pump instead: Mission Control can cover a key window without taking focus. */
int MacWindow_IsVisible(SDL_Window *window)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_COCOA)
        return 0;
    NSWindow *native = info.info.cocoa.window;
    return native.visible && !native.miniaturized &&
           (native.occlusionState & NSWindowOcclusionStateVisible) != 0;
}
