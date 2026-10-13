#import <AppKit/AppKit.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <assert.h>
#include <stdio.h>
extern void MacWindow_SetBorderless(int enabled);
int main(void)
{
    @autoreleasepool {
        assert(SDL_Init(SDL_INIT_VIDEO) == 0);
        SDL_Window *window = SDL_CreateWindow("CoD2 application hide probe", 0, 0, 320, 240, SDL_WINDOW_SHOWN);
        assert(window);
        SDL_SysWMinfo wm;
        SDL_VERSION(&wm.version);
        assert(SDL_GetWindowWMInfo(window, &wm));
        assert(wm.info.cocoa.window);
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        [NSApp unhide:nil];
        [NSApp activateIgnoringOtherApps:YES];
        SDL_Delay(200); SDL_PumpEvents();
        MacWindow_SetBorderless(1);
        [NSNotificationCenter.defaultCenter postNotificationName:NSWindowDidResignKeyNotification object:wm.info.cocoa.window];
        assert(!NSApp.hidden);
        [NSNotificationCenter.defaultCenter postNotificationName:NSApplicationDidResignActiveNotification object:NSApp];
        SDL_Delay(100); SDL_PumpEvents();
        assert(NSApp.hidden);
        MacWindow_SetBorderless(0);
        [NSApp unhideWithoutActivation];
        [NSRunningApplication.currentApplication unhide];
        [NSApp activateIgnoringOtherApps:YES];
        SDL_RaiseWindow(window);
        SDL_Delay(200); SDL_PumpEvents();
        assert(!NSApp.hidden);
        MacWindow_SetBorderless(0);
        [NSNotificationCenter.defaultCenter postNotificationName:NSApplicationDidResignActiveNotification object:NSApp];
        assert(!NSApp.hidden);
        SDL_DestroyWindow(window);
        SDL_Quit();
        puts("PASS borderless app resignation hides; key-window loss and exclusive/windowed do not");
    }
    return 0;
}
