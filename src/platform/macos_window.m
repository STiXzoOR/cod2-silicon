#import <AppKit/AppKit.h>
#include <SDL.h>
#include <SDL_syswm.h>
#if defined(COD2_X64)
#include <stdlib.h>
#include <dispatch/dispatch.h>
#endif

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
static NSWindow *splashWindow;
static int splashExitRegistered;

void MacWindow_HideSplash(void)
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ MacWindow_HideSplash(); });
        return;
    }
    [splashWindow orderOut:nil];
}

void MacWindow_DestroySplash(void)
{
    /* Fatal recursion and exit handlers can arrive from a worker. Never wait
     * for a main thread that may itself be waiting on that worker. */
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ MacWindow_DestroySplash(); });
        return;
    }
    [splashWindow orderOut:nil];
    [splashWindow close];
    [splashWindow release];
    splashWindow = nil;
}

static void MacWindow_DisplaySplash(void)
{
    [splashWindow display];
    /* Startup immediately continues with blocking work, without a Cocoa loop
     * to perform AppKit's usual deferred buffered-window flush. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [splashWindow flushWindow];
#pragma clang diagnostic pop
}

void MacWindow_DrawSplash(const char *directory)
{
    if (![NSThread isMainThread])
        return;
    @autoreleasepool {
        if (splashWindow) {
            [splashWindow orderFront:nil];
            MacWindow_DisplaySplash();
            return;
        }
        if (!directory)
            return;
        NSString *folder = [NSString stringWithUTF8String:directory];
        if (!folder)
            return;
        NSData *data = [NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:@"cod.bmp"]];
        const unsigned char *bytes = data.bytes;
        if (data.length < 2 || bytes[0] != 'B' || bytes[1] != 'M')
            return;
        NSImageRep *representation = [NSBitmapImageRep imageRepWithData:data];
        if (![representation isKindOfClass:NSBitmapImageRep.class])
            return;
        NSBitmapImageRep *bitmap = (NSBitmapImageRep *)representation;
        if (bitmap.pixelsWide <= 0 || bitmap.pixelsHigh <= 0)
            return;
        /* Match the existing native first-launch AppKit bootstrap. SDL video
         * must remain deferred so MacDisplay can set its initialization hints. */
        if (!NSApp) {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            [NSApp finishLaunching];
        }
        if (!splashExitRegistered) {
            if (atexit(MacWindow_DestroySplash) != 0) {
                MacWindow_DestroySplash();
                return;
            }
            splashExitRegistered = 1;
        }
        NSSize size = NSMakeSize(bitmap.pixelsWide, bitmap.pixelsHigh);
        NSImage *image = [[NSImage alloc] initWithSize:size];
        NSImageView *view = [[NSImageView alloc] initWithFrame:NSMakeRect(1, 1, size.width, size.height)];
        if (!image || !view) {
            [image release];
            [view release];
            MacWindow_DestroySplash();
            return;
        }
        [image addRepresentation:bitmap];
        view.image = image;
        view.imageScaling = NSImageScaleNone;
        splashWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, size.width + 2, size.height + 2)
            styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
        splashWindow.releasedWhenClosed = NO;
        splashWindow.title = @"CoD Splash Screen";
        splashWindow.backgroundColor = NSColor.blackColor;
        [splashWindow.contentView addSubview:view];
        [view release];
        [image release];
        if (!splashWindow) {
            MacWindow_DestroySplash();
            return;
        }
        /* Windows 0x4681e0 centers the bitmap-sized popup on the desktop. */
        NSScreen *screen = NSScreen.screens.firstObject;
        if (!screen) {
            MacWindow_DestroySplash();
            return;
        }
        NSRect frame = splashWindow.frame;
        [splashWindow setFrameOrigin:NSMakePoint(NSMidX(screen.frame) - frame.size.width / 2,
                                                NSMidY(screen.frame) - frame.size.height / 2)];
        [NSApp activateIgnoringOtherApps:YES];
        [splashWindow orderFront:nil];
        MacWindow_DisplaySplash();
    }
}

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
