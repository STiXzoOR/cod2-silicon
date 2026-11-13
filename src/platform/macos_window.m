#import <AppKit/AppKit.h>
#include <SDL.h>
#include <SDL_syswm.h>
#if defined(COD2_X64)
#include <stdlib.h>
#include <dispatch/dispatch.h>
#include "macos_console.h"
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

/* Carbon Mac 1.3 lazily builds a scrolling TXN view and a separate edit
 * control. Keep this in the existing ObjC bridge so neither the engine nor
 * dedicated/headless builds need AppKit types. All control access is main-thread. */
static NSWindow *consoleWindow;
static NSTextView *consoleOutput;
static NSTextField *consoleInput;
static NSMutableData *consoleText;
static NSUInteger consoleTotalCharacters;
static void (*consoleSubmit)(const char *);
static void (*consoleClose)(void);
static void (*consoleVisibility)(int);
static BOOL consoleError;

@interface CoDSystemConsole : NSObject <NSWindowDelegate, NSTextFieldDelegate>
- (void)submit:(id)sender;
@end

@implementation CoDSystemConsole
- (void)submit:(id)sender
{
    (void)sender;
    NSString *text = [[consoleInput.stringValue copy] autorelease];
    if (!consoleError && consoleSubmit)
        consoleSubmit(text.UTF8String);
    consoleInput.stringValue = @"";
}
- (BOOL)windowShouldClose:(NSWindow *)sender
{
    (void)sender;
    if (consoleClose)
        consoleClose();
    return NO;
}
- (void)windowDidMiniaturize:(NSNotification *)notification
{
    (void)notification;
    if (consoleVisibility)
        consoleVisibility(2);
}
- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    (void)notification;
    if (consoleVisibility)
        consoleVisibility(1);
}
- (void)windowDidBecomeKey:(NSNotification *)notification
{
    (void)notification;
    if (!consoleError)
        [consoleWindow makeFirstResponder:consoleInput];
}
@end

static CoDSystemConsole *consoleDelegate;

void MacConsole_Destroy(void)
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ MacConsole_Destroy(); });
        return;
    }
    consoleWindow.delegate = nil;
    [consoleWindow orderOut:nil];
    [consoleWindow close];
    [consoleWindow release];
    consoleWindow = nil;
    consoleInput = nil;
    consoleOutput = nil;
    [consoleDelegate release];
    consoleDelegate = nil;
    [consoleText release];
    consoleText = nil;
    consoleTotalCharacters = 0;
    consoleSubmit = NULL;
    consoleClose = NULL;
    consoleVisibility = NULL;
    consoleError = NO;
}

void MacConsole_Create(void (*submit)(const char *), void (*close)(void),
                       void (*visibility)(int))
{
    MacConsole_Destroy();
    consoleText = [[NSMutableData alloc] init];
    consoleSubmit = submit;
    consoleClose = close;
    consoleVisibility = visibility;
}

static NSString *MacConsole_DecodeText(NSData *text, int language);
static int consoleLanguage;

static void MacConsole_CreateWindow(void)
{
    if (!NSApp) {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
    }
    consoleDelegate = [[CoDSystemConsole alloc] init];
    consoleWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 620, 450)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    consoleWindow.releasedWhenClosed = NO;
    consoleWindow.title = @"Call of Duty 2 Multiplayer Console";
    consoleWindow.delegate = consoleDelegate;
    consoleWindow.contentMinSize = NSMakeSize(360, 180);
    [consoleWindow center];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(8, 42, 604, 400)];
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezelBorder;
    consoleOutput = [[NSTextView alloc] initWithFrame:scroll.contentView.bounds];
    consoleOutput.editable = NO;
    consoleOutput.selectable = YES;
    consoleOutput.richText = NO;
    consoleOutput.font = [NSFont userFixedPitchFontOfSize:11];
    consoleOutput.verticallyResizable = YES;
    consoleOutput.horizontallyResizable = NO;
    consoleOutput.autoresizingMask = NSViewWidthSizable;
    consoleOutput.textContainer.widthTracksTextView = YES;
    consoleOutput.textContainer.containerSize = NSMakeSize(scroll.contentSize.width, CGFLOAT_MAX);
    consoleOutput.minSize = NSMakeSize(0, scroll.contentSize.height);
    consoleOutput.maxSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
    consoleOutput.string = MacConsole_DecodeText(consoleText, consoleLanguage);
    scroll.documentView = consoleOutput;
    [consoleWindow.contentView addSubview:scroll];
    [consoleOutput release];
    [scroll release];

    consoleInput = [[NSTextField alloc] initWithFrame:NSMakeRect(8, 10, 604, 24)];
    consoleInput.autoresizingMask = NSViewWidthSizable;
    consoleInput.font = [NSFont userFixedPitchFontOfSize:12];
    consoleInput.target = consoleDelegate;
    consoleInput.action = @selector(submit:);
    consoleInput.delegate = consoleDelegate;
    consoleInput.cell.usesSingleLineMode = YES;
    consoleInput.cell.scrollable = YES;
    consoleInput.hidden = consoleError;
    [consoleInput setAccessibilityLabel:@"Console command"];
    [consoleOutput setAccessibilityLabel:@"Console output"];
    [consoleWindow.contentView addSubview:consoleInput];
    [consoleInput release];
}

void MacConsole_Show(int level)
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ MacConsole_Show(level); });
        return;
    }
    @autoreleasepool {
        if (!level) {
            [consoleWindow orderOut:nil];
        } else {
            if (!consoleWindow)
                MacConsole_CreateWindow();
            if (level == 2) {
                [consoleWindow orderFront:nil];
                [consoleWindow miniaturize:nil];
            } else {
                [consoleWindow deminiaturize:nil];
                [consoleWindow makeKeyAndOrderFront:nil];
                if (!consoleError)
                    [consoleWindow makeFirstResponder:consoleInput];
                [consoleOutput scrollRangeToVisible:NSMakeRange(consoleOutput.string.length, 0)];
            }
        }
        if (consoleVisibility)
            consoleVisibility(consoleWindow ? level : 0);
    }
}

static NSString *MacConsole_DecodeText(NSData *text, int language)
{
    unsigned codepage = 1252;
    switch (language) {
    case 6: codepage = 1251; break;
    case 7: case 14: codepage = 1250; break;
    case 8: codepage = 949; break;
    case 9: codepage = 950; break;
    case 10: codepage = 932; break;
    case 11: codepage = 936; break;
    case 12: codepage = 874; break;
    }
    NSStringEncoding encoding = CFStringConvertEncodingToNSStringEncoding(
        CFStringConvertWindowsCodepageToEncoding(codepage));
    NSString *string = [[[NSString alloc] initWithData:text encoding:encoding] autorelease];
    return string ?: [[[NSString alloc] initWithData:text encoding:NSISOLatin1StringEncoding] autorelease];
}

#if defined(COD2_CODX) && COD2_CODX
#define MAC_CONSOLE_BUFFER_SIZE 32767
#else
#define MAC_CONSOLE_BUFFER_SIZE 16384
#endif

static void MacConsole_AppendData(NSData *text, int language)
{
    if (!consoleText || !text.length)
        return;
    /* Windows 0x468B10 clears the old buffer on overflow. CoD2x deliberately
     * doubles its capacity and removes only the older half (console.cpp). The
     * count includes CRLF bytes just like the original ANSI edit control. */
    consoleTotalCharacters += text.length;
    if (consoleTotalCharacters > MAC_CONSOLE_BUFFER_SIZE) {
#if defined(COD2_CODX) && COD2_CODX
        NSUInteger remove = consoleTotalCharacters / 2;
        [consoleText replaceBytesInRange:NSMakeRange(0, MIN(remove, consoleText.length))
                               withBytes:NULL length:0];
        consoleTotalCharacters -= remove;
#else
        [consoleText setLength:0];
        consoleTotalCharacters = text.length;
#endif
    }
    [consoleText appendData:text];
    consoleLanguage = language;
    if (consoleOutput) {
        consoleOutput.string = MacConsole_DecodeText(consoleText, language);
        [consoleOutput setSelectedRange:NSMakeRange(consoleOutput.string.length, 0)];
        [consoleOutput scrollRangeToVisible:NSMakeRange(consoleOutput.string.length, 0)];
    }
}

void MacConsole_AppendText(const char *text, int language)
{
    if (!text)
        return;
    @autoreleasepool {
        /* Keep Windows' visible newline/color handling, including its stock
         * digit-only colors. Preserve stdout separately in mac_syscon.c. */
        size_t length = strlen(text);
        if (length > MAC_CONSOLE_BUFFER_SIZE - 1)
            text += length - MAC_CONSOLE_BUFFER_SIZE + 1;
        char buffer[MAC_CONSOLE_BUFFER_SIZE * 2];
        size_t used = 0;
        for (; *text && used < sizeof(buffer) - 2; ++text) {
            if (*text == '\n' && text[1] == '\r') {
                buffer[used++] = '\r';
                buffer[used++] = '\n';
                ++text;
            } else if (*text == '\r' || *text == '\n') {
                buffer[used++] = '\r';
                buffer[used++] = '\n';
#if defined(COD2_CODX) && COD2_CODX
            } else if (*text == '^' && text[1] && text[1] != '^') {
#else
            } else if (*text == '^' && text[1] >= '0' && text[1] <= '9') {
#endif
                ++text;
            } else {
                buffer[used++] = *text;
            }
        }
        NSData *data = [NSData dataWithBytes:buffer length:used];
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                MacConsole_AppendData(data, language);
            });
        } else {
            MacConsole_AppendData(data, language);
        }
    }
}

void MacConsole_SetErrorText(void)
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ MacConsole_SetErrorText(); });
        return;
    }
    consoleError = YES;
    consoleInput.hidden = YES;
    [consoleWindow makeFirstResponder:consoleOutput];
}

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
