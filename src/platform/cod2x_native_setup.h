/* Included only by the native CoD2x Cocoa launcher. No licensed assets here. */
#include <stdlib.h>

static NSString *Cod2xAppHome(void)
{
    return [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/CoD2x Native"];
}

static BOOL Cod2xGameDirectoryValid(NSString *path)
{
    if (!path.length) return NO;
    NSArray *files = [NSFileManager.defaultManager contentsOfDirectoryAtPath:
        [path stringByAppendingPathComponent:@"main"] error:nil];
    for (int index = 0; index < 16; ++index)
        if (![files containsObject:[NSString stringWithFormat:@"iw_%02d.iwd", index]]) return NO;
    return YES;
}

static NSString *Cod2xGameDirectory(NSBundle *bundle)
{
    NSString *remembered = [Cod2xAppHome() stringByAppendingPathComponent:@"data-path.txt"];
    NSString *path = [NSString stringWithContentsOfFile:remembered encoding:NSUTF8StringEncoding error:nil];
    if (Cod2xGameDirectoryValid(path)) return path;
    id configured = [bundle objectForInfoDictionaryKey:@"CoD2GameDirectory"];
    if ([configured isKindOfClass:NSString.class] && Cod2xGameDirectoryValid(configured)) return configured;
    path = [NSHomeDirectory() stringByAppendingPathComponent:@"Games/CoD2"];
    if (Cod2xGameDirectoryValid(path)) return path;
    [NSApplication sharedApplication];
    NSOpenPanel *panel = NSOpenPanel.openPanel;
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = NO;
    panel.allowsMultipleSelection = NO;
    panel.message = @"Choose your Call of Duty 2 folder containing main/iw_00.iwd through iw_15.iwd.";
    panel.prompt = @"Play";
    for (;;) {
        if ([panel runModal] != NSModalResponseOK) return nil;
        path = panel.URL.path;
        if (Cod2xGameDirectoryValid(path)) break;
        panel.message = @"That folder does not contain the CoD2 game data. Choose the folder containing main/.";
    }
    [path writeToFile:remembered atomically:YES encoding:NSUTF8StringEncoding error:nil];
    return path;
}

static void Cod2xSetupShaders(NSBundle *bundle)
{
    const char *override = getenv("COD2_MAC_SHADER_CACHE");
    NSString *cache = override && *override ? [NSString stringWithUTF8String:override] : [Cod2xAppHome() stringByAppendingPathComponent:@"shaders"];
    NSString *script = [bundle pathForResource:@"extract_shaders" ofType:@"py" inDirectory:@"tools/macos-port"];
    NSString *python = nil;
    for (NSString *candidate in @[@"/opt/homebrew/bin/python3", @"/usr/local/bin/python3", @"/usr/bin/python3"])
        if ([NSFileManager.defaultManager isExecutableFileAtPath:candidate]) { python = candidate; break; }
    BOOL verified = NO;
    NSWindow *window = nil;
    if (script && python) {
        if (![NSFileManager.defaultManager fileExistsAtPath:[cache stringByAppendingPathComponent:@"manifest.json"]]) {
            [NSApplication sharedApplication];
            window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 440, 110)
                styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
            window.title = @"CoD2x Native";
            NSTextField *label = [NSTextField labelWithString:@"Preparing original Mac shaders for first launch…"];
            label.frame = NSMakeRect(20, 65, 410, 24);
            [window.contentView addSubview:label];
            NSProgressIndicator *progress = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, 30, 400, 18)];
            progress.indeterminate = YES;
            progress.style = NSProgressIndicatorStyleBar;
            [progress startAnimation:nil];
            [window.contentView addSubview:progress];
            [window center]; [window makeKeyAndOrderFront:nil];
        }
        NSTask *task = [[NSTask alloc] init];
        task.executableURL = [NSURL fileURLWithPath:python];
        /* A client observer must not run in the extraction subprocess or
         * overwrite the client's constructor-written ownership PID. */
        NSMutableDictionary *environment = NSProcessInfo.processInfo.environment.mutableCopy;
        for (NSString *key in @[@"DYLD_INSERT_LIBRARIES", @"COD2_FRAME_PID", @"COD2_FRAME_CSV", @"COD2_CPU_PROFILE"])
            [environment removeObjectForKey:key];
        task.environment = environment;
        task.arguments = @[script,
            [NSHomeDirectory() stringByAppendingPathComponent:@"Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer"],
            cache, @"--setup", @"--fallback",
            [NSHomeDirectory() stringByAppendingPathComponent:@"Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"]];
        task.standardOutput = NSFileHandle.fileHandleWithStandardError;
        task.standardError = NSFileHandle.fileHandleWithStandardError;
        NSError *error = nil;
        if ([task launchAndReturnError:&error]) {
            while (task.running)
                [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.05]];
            [task waitUntilExit];
            verified = task.terminationStatus == 0;
        } else fprintf(stderr, "CoD2x: shader setup could not start: %s\n", error.localizedDescription.UTF8String);
    }
    [window orderOut:nil];
    if (verified) setenv("COD2_MAC_SHADER_CACHE", cache.fileSystemRepresentation, 1);
    else {
        unsetenv("COD2_MAC_SHADER_CACHE");
        fprintf(stderr, "CoD2x: no verified Mac shader cache (licensed binary or Python 3 unavailable); using approximation rendering.\n");
    }
}

static int Cod2xSetupAppArguments(NSBundle *bundle, NSString *arguments, char *buffer, int capacity)
{
    [NSFileManager.defaultManager createDirectoryAtPath:Cod2xAppHome() withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *data = Cod2xGameDirectory(bundle);
    if (!data) return -1;
    NSCharacterSet *unsafe = [NSCharacterSet characterSetWithCharactersInString:@"\";+\r\n"];
    if ([data rangeOfCharacterFromSet:unsafe].location != NSNotFound ||
        [Cod2xAppHome() rangeOfCharacterFromSet:unsafe].location != NSNotFound) {
        fprintf(stderr, "CoD2x: game paths cannot contain quotes, semicolons, plus signs or newlines.\n");
        return -1;
    }
    Cod2xSetupShaders(bundle);
    int length = snprintf(buffer, capacity, "%s +set fs_basepath \"%s\" +set fs_homepath \"%s\"",
        arguments.UTF8String, data.fileSystemRepresentation, Cod2xAppHome().fileSystemRepresentation);
    return length >= 0 && length < capacity ? length : -1;
}
