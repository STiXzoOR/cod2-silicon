// Test-only invocation of the launcher's actual Launch menu action, without
// synthesizing OS input or requiring Accessibility.
#import <AppKit/AppKit.h>
#include <dlfcn.h>

static void traceCursor(void)
{
    const char *path = getenv("COD2_QA_CURSOR_PATH");
    if (!path) return;
    int (*showCursor)(int) = dlsym(RTLD_DEFAULT, "SDL_ShowCursor");
    int (*relative)(void) = dlsym(RTLD_DEFAULT, "SDL_GetRelativeMouseMode");
    int (*visible)(void) = dlsym(RTLD_DEFAULT, "MacDisplay_WindowVisible");
    if (!showCursor || !relative || !visible) return;
    __block unsigned attempts = 0;
    NSTimer *timer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *timer) {
        if (visible() && showCursor(-1) == 0 && relative() == 1) {
            FILE *file = fopen(path, "w");
            if (file) { fputs("{\"system_cursor_hidden\":true,\"relative_mouse\":true}\n", file); fclose(file); }
            [timer invalidate];
        } else if (++attempts == 400) [timer invalidate];
    }];
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

static BOOL launch(NSMenu *menu)
{
    for (NSInteger i = 0; i < menu.numberOfItems; ++i) {
        NSMenuItem *item = [menu itemAtIndex:i];
        if ([item.title isEqualToString:@"Launch"]) {
            [menu performActionForItemAtIndex:i];
            fprintf(stderr, "[launch-probe] invoked Launch menu action\n");
            return YES;
        }
        if (item.submenu && launch(item.submenu)) return YES;
    }
    return NO;
}

static void traceLauncher(void)
{
    const char *path = getenv("COD2_QA_WINDOW_TRACE");
    if (!path) return;
    __block unsigned attempts = 0;
    NSTimer *timer = [NSTimer timerWithTimeInterval:0.25 repeats:YES block:^(NSTimer *timer) {
        NSMutableArray *windows = [NSMutableArray array];
        for (NSWindow *window in NSApp.windows)
            [windows addObject:@{ @"title":window.title, @"class":NSStringFromClass(window.class),
                @"visible":@(window.visible), @"key":@(window.canBecomeKeyWindow),
                @"number":@(window.windowNumber) }];
        NSData *data = [NSJSONSerialization dataWithJSONObject:@{ @"active":@(NSApp.active),
            @"policy":@(NSApp.activationPolicy), @"windows":windows,
            @"bundle":[NSBundle mainBundle].bundleIdentifier ?: @"" } options:0 error:nil];
        FILE *file = fopen(path, "a");
        if (file) { fwrite(data.bytes, 1, data.length, file); fputc('\n', file); fclose(file); }
        if (++attempts == 240) [timer invalidate];
    }];
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

__attribute__((constructor)) static void install(void)
{
    if ([[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleExecutable"] isEqual:@"cod2_macos"]) {
        dispatch_async(dispatch_get_main_queue(), ^{ traceCursor(); });
        return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{ traceLauncher(); });
    if (!getenv("COD2_QA_LAUNCH")) return;
    NSDate *installed = [NSDate date];
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *ready = [NSHomeDirectory() stringByAppendingPathComponent:
            @"Library/Application Support/CoD2 Unbound/.launcher-setup-complete"];
        __block unsigned attempts = 0;
        NSTimer *timer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *timer) {
            NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:ready error:nil];
            NSDate *modified = attributes[NSFileModificationDate];
            // Ignore a marker from an earlier launch. Setup writes a fresh one
            // on the main actor immediately before leaving onboarding.
            if (modified && [modified compare:installed] != NSOrderedAscending && launch(NSApp.mainMenu)) {
                [timer invalidate];
            } else if (++attempts == 400) {
                fprintf(stderr, "[launch-probe] setup timed out\n");
                [timer invalidate];
            }
        }];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
    });
}
