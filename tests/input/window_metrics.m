/* Read the test game's own Cocoa geometry; never focus, resize, or warp it. */
#import <AppKit/AppKit.h>
#include <SDL.h>
#include <SDL_syswm.h>

int TestInput_WindowMetrics(SDL_Window *window, float *metrics)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!window || !SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_COCOA) return 0;
    NSWindow *native = info.info.cocoa.window;
    NSSize content = native.contentView.bounds.size;
    NSSize screen = native.screen.frame.size;
    metrics[0] = content.width; metrics[1] = content.height;
    metrics[2] = screen.width; metrics[3] = screen.height;
    metrics[4] = native.backingScaleFactor;
    return 1;
}
