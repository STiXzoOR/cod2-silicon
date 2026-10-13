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
#if defined(COD2_X64)
    if (NSApp.hidden)
        return 0;
#endif
    return native.visible && !native.miniaturized &&
           (native.occlusionState & NSWindowOcclusionStateVisible) != 0;
}
#if defined(__APPLE__) && defined(COD2_X64)

static int hideBorderless;
static id resignObserver;

void MacWindow_SetBorderless(int enabled)
{
    hideBorderless = enabled;
    if (enabled && !resignObserver) {
        resignObserver = [NSNotificationCenter.defaultCenter
            addObserverForName:NSApplicationDidResignActiveNotification object:NSApp
            queue:nil usingBlock:^(NSNotification *note) {
                (void)note;
                if (hideBorderless) {
                    /* App switching resigns application activation. Spotlight
                     * only takes the key window and must not hide the game. */
                    [NSApp hide:nil];
                }
            }];
    } else if (!enabled && resignObserver) {
        [NSNotificationCenter.defaultCenter removeObserver:resignObserver];
        resignObserver = nil;
    }
}
#endif
