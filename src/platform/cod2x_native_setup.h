/* Included only by the native CoD2x Cocoa launcher. No licensed assets here. */
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "cod2x_native_shaders.h"

static NSString *Cod2xAppHome(void)
{
    return [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/CoD2 Silicon"];
}

static NSString *Cod2xSetupOverride(const char *name)
{
    const char *value = getenv(name);
    return value && *value ? [NSString stringWithUTF8String:value] : nil;
}

static BOOL Cod2xNoninteractive(void)
{
    return [Cod2xSetupOverride("COD2_SETUP_NONINTERACTIVE") isEqualToString:@"1"];
}

static void Cod2xSetupAlert(NSString *message, NSString *detail)
{
    if (Cod2xNoninteractive()) { fprintf(stderr, "CoD2 Silicon: %s\n", message.UTF8String); return; }
    [NSApplication sharedApplication];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = message;
    alert.informativeText = detail;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

static void Cod2xMigrate(void)
{
    NSFileManager *files = NSFileManager.defaultManager;
    NSString *home = Cod2xAppHome();
    NSString *marker = [home stringByAppendingPathComponent:@".migrated-native"];
    if ([files fileExistsAtPath:marker]) return;
    NSString *old = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/CoD2x Native"];
    BOOL complete = YES;
    for (NSString *name in @[@"main", @"shaders", @"data-path.txt"]) {
        NSString *source = [old stringByAppendingPathComponent:name];
        NSString *target = [home stringByAppendingPathComponent:name];
        if ([files fileExistsAtPath:source] && ![files fileExistsAtPath:target])
            complete &= [files copyItemAtPath:source toPath:target error:nil];
    }
    if (complete) [@"1\n" writeToFile:marker atomically:YES encoding:NSUTF8StringEncoding error:nil];
}

static BOOL Cod2xGameDirectoryValid(NSString *path)
{
    if (!path.length) return NO;
    NSFileManager *files = NSFileManager.defaultManager;
    for (int index = 0; index < 16; ++index) {
        NSString *file = [path stringByAppendingPathComponent:[NSString stringWithFormat:@"main/iw_%02d.iwd", index]];
        BOOL directory = NO;
        if (![files fileExistsAtPath:file isDirectory:&directory] || directory || ![files isReadableFileAtPath:file]) return NO;
    }
    return YES;
}

static NSArray<NSString *> *Cod2xSteamFolders(void)
{
    NSString *steam = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/Steam"];
    NSMutableArray *folders = [NSMutableArray arrayWithObject:[steam stringByAppendingPathComponent:@"steamapps/common/Call of Duty 2"]];
    NSString *vdf = [NSString stringWithContentsOfFile:[steam stringByAppendingPathComponent:@"steamapps/libraryfolders.vdf"] encoding:NSUTF8StringEncoding error:nil];
    NSRegularExpression *pattern = [NSRegularExpression regularExpressionWithPattern:@"\"path\"\\s*\"([^\"]+)\"" options:0 error:nil];
    for (NSTextCheckingResult *match in [pattern matchesInString:vdf ?: @"" options:0 range:NSMakeRange(0, vdf.length)]) {
        NSString *path = [[vdf substringWithRange:[match rangeAtIndex:1]] stringByReplacingOccurrencesOfString:@"\\\\" withString:@"\\"];
        [folders addObject:[path stringByAppendingPathComponent:@"steamapps/common/Call of Duty 2"]];
    }
    return folders;
}

static NSArray<NSString *> *Cod2xInstallFolders(void)
{
    NSMutableArray *folders = Cod2xSteamFolders().mutableCopy;
    for (NSString *name in [NSFileManager.defaultManager contentsOfDirectoryAtPath:@"/Applications" error:nil])
        if ([name hasPrefix:@"Call of Duty 2"]) [folders addObject:[@"/Applications" stringByAppendingPathComponent:name]];
    return folders;
}

static void Cod2xGameDataAlert(void)
{
    NSString *detail = @"CoD2 Silicon needs your own licensed Call of Duty 2 game data: main/iw_00.iwd through iw_15.iwd. Select the containing folder, or a Call of Duty 2 app that contains it. Game files are not included in this download.";
    if (Cod2xNoninteractive()) { Cod2xSetupAlert(@"Game data folder is missing or incomplete.", detail); return; }
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Choose your Call of Duty 2 game data";
    alert.informativeText = detail;
    [alert addButtonWithTitle:@"Choose Again"]; [alert addButtonWithTitle:@"Game Data Instructions"];
    if ([alert runModal] == NSAlertSecondButtonReturn)
        [NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:@"https://github.com/STiXzoOR/cod2-silicon#game-data"]];
}

static NSString *Cod2xDataInFolder(NSString *folder)
{
    for (NSString *relative in @[@"", @"Contents/Resources", @"Contents", @"Call of Duty 2.app/Contents/Resources"] ) {
        NSString *path = relative.length ? [folder stringByAppendingPathComponent:relative] : folder;
        if (Cod2xGameDirectoryValid(path)) return path;
    }
    return nil;
}

static NSString *Cod2xGameDirectory(NSBundle *bundle)
{
    NSString *remembered = [Cod2xAppHome() stringByAppendingPathComponent:@"data-path.txt"];
    NSString *override = Cod2xSetupOverride("COD2_SETUP_GAME_DIR");
    NSMutableArray *candidates = [NSMutableArray array];
    if (override) [candidates addObject:override];
    else {
        NSString *saved = [NSString stringWithContentsOfFile:remembered encoding:NSUTF8StringEncoding error:nil];
        if (saved.length) [candidates addObject:[saved stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]];
        id configured = [bundle objectForInfoDictionaryKey:@"CoD2GameDirectory"];
        if ([configured isKindOfClass:NSString.class]) [candidates addObject:configured];
        [candidates addObject:[NSHomeDirectory() stringByAppendingPathComponent:@"Games/CoD2"]];
        [candidates addObjectsFromArray:Cod2xInstallFolders()];
    }
    NSString *path = nil;
    for (NSString *candidate in candidates) { path = Cod2xDataInFolder(candidate); if (path) break; }
    if (!path) {
        if (Cod2xNoninteractive()) { Cod2xGameDataAlert(); return nil; }
        [NSApplication sharedApplication];
        NSOpenPanel *panel = NSOpenPanel.openPanel;
        panel.canChooseDirectories = YES; panel.canChooseFiles = NO;
        panel.treatsFilePackagesAsDirectories = YES; panel.allowsMultipleSelection = NO;
        panel.message = @"Choose your licensed Call of Duty 2 folder containing main/iw_00.iwd through iw_15.iwd.";
        panel.prompt = @"Use Game Folder";
        for (;;) {
            if ([panel runModal] != NSModalResponseOK) return nil;
            path = Cod2xDataInFolder(panel.URL.path);
            if (path) break;
            Cod2xGameDataAlert();
        }
    }
    [path writeToFile:remembered atomically:YES encoding:NSUTF8StringEncoding error:nil];
    return path;
}

static BOOL Cod2xKeyValid(NSString *key)
{
    if (!key || [key rangeOfString:@"\\A[A-Za-z0-9]{16}[A-Fa-f0-9]{4}\\z" options:NSRegularExpressionSearch].location == NSNotFound)
        return NO;
    const unsigned char *bytes = (const unsigned char *)key.UTF8String;
    unsigned crc = 0;
    /* Same CRC-16 check as CL_CDKeyValidate; never expose the supplied key. */
    for (unsigned i = 0; i < 16; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
    }
    NSString *checksum = [NSString stringWithFormat:@"%04x", crc];
    return [[key substringFromIndex:16] caseInsensitiveCompare:checksum] == NSOrderedSame;
}

static BOOL Cod2xSetupKey(void)
{
    NSString *directory = [NSHomeDirectory() stringByAppendingPathComponent:@".cod2"];
    NSString *preferences = [directory stringByAppendingPathComponent:@"preferences"];
    NSString *previous = [NSString stringWithContentsOfFile:preferences encoding:NSUTF8StringEncoding error:nil] ?: @"";
    for (NSString *line in [previous componentsSeparatedByString:@"\n"])
        if ([line hasPrefix:@"codkey="] && Cod2xKeyValid([line substringFromIndex:7])) {
            chmod(preferences.fileSystemRepresentation, 0600); return YES;
        }
    NSString *key = Cod2xSetupOverride("COD2_SETUP_CD_KEY");
    for (;;) {
        if (!key) {
            if (Cod2xNoninteractive()) { Cod2xSetupAlert(@"A CD key is required for first launch.", @""); return NO; }
            NSAlert *alert = [[NSAlert alloc] init];
            alert.messageText = @"Enter your Call of Duty 2 CD key";
            alert.informativeText = @"Enter the 20 characters from your licensed copy. You can change the key later in Multiplayer Options. It is stored privately on this Mac.";
            NSSecureTextField *input = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 320, 24)];
            alert.accessoryView = input;
            [alert addButtonWithTitle:@"Save and Play"]; [alert addButtonWithTitle:@"Cancel"];
            [alert.window setInitialFirstResponder:input];
            if ([alert runModal] != NSAlertFirstButtonReturn) return NO;
            key = input.stringValue;
        }
        key = [[key stringByReplacingOccurrencesOfString:@"-" withString:@""] stringByReplacingOccurrencesOfString:@" " withString:@""].uppercaseString;
        if (Cod2xKeyValid(key)) break;
        Cod2xSetupAlert(@"The CD key is incomplete or its checksum is incorrect.", @"Enter all 20 characters from your licensed copy, including the last four checksum digits.");
        if (Cod2xNoninteractive()) return NO;
        key = nil;
    }
    if (![NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions: @0700} error:nil]) return NO;
    NSMutableString *contents = [NSMutableString string];
    for (NSString *line in [previous componentsSeparatedByString:@"\n"])
        if (line.length && ![line hasPrefix:@"codkey="]) [contents appendFormat:@"%@\n", line];
    [contents appendFormat:@"codkey=%@\n", key];
    NSString *temporary = [preferences stringByAppendingFormat:@".%@", NSUUID.UUID.UUIDString];
    int fd = open(temporary.fileSystemRepresentation, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) return NO;
    NSData *bytes = [contents dataUsingEncoding:NSUTF8StringEncoding];
    const unsigned char *cursor = bytes.bytes;
    NSUInteger remaining = bytes.length;
    BOOL saved = YES;
    while (remaining) {
        ssize_t count = write(fd, cursor, remaining);
        if (count <= 0) { saved = NO; break; }
        remaining -= count; cursor += count;
    }
    if (close(fd) != 0) saved = NO;
    if (saved) saved = rename(temporary.fileSystemRepresentation, preferences.fileSystemRepresentation) == 0;
    if (!saved) unlink(temporary.fileSystemRepresentation);
    return saved;
}

static NSArray<NSString *> *Cod2xShaderBinaries(NSString *data)
{
    NSMutableArray *binaries = [NSMutableArray array];
    NSString *override = Cod2xSetupOverride("COD2_SETUP_MAC_BINARY");
    if (override) [binaries addObject:override];
    NSMutableArray *folders = [NSMutableArray arrayWithObject:data];
    [folders addObjectsFromArray:Cod2xInstallFolders()];
    [folders addObject:[NSHomeDirectory() stringByAppendingPathComponent:@"Games/CoD2-mac-bin"]];
    for (NSString *folder in folders) {
        if ([NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:@"Call of Duty 2 Multiplayer"]])
            [binaries addObject:[folder stringByAppendingPathComponent:@"Call of Duty 2 Multiplayer"]];
        NSDirectoryEnumerator *entries = [NSFileManager.defaultManager enumeratorAtURL:[NSURL fileURLWithPath:folder]
            includingPropertiesForKeys:nil options:NSDirectoryEnumerationSkipsHiddenFiles errorHandler:nil];
        for (NSURL *entry in entries) {
            if (entries.level > 8) { [entries skipDescendants]; continue; }
            if ([entry.lastPathComponent isEqualToString:@"Call of Duty 2 Multiplayer"])
                [binaries addObject:entry.path];
        }
    }
    return binaries;
}

static void Cod2xSetupShaders(NSString *data)
{
    NSString *cache = Cod2xSetupOverride("COD2_MAC_SHADER_CACHE") ?: [Cod2xAppHome() stringByAppendingPathComponent:@"shaders"];
    BOOL verified = Cod2xShadersSetup(Cod2xShaderBinaries(data), cache);
    if (!verified && !Cod2xNoninteractive()) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"Original Mac shaders were not found";
        alert.informativeText = @"For the original rendering, choose a folder containing your licensed Mac Call of Duty 2 app. You can also play now using approximate shaders; lighting and sky may differ.";
        [alert addButtonWithTitle:@"Choose Mac Game Folder"]; [alert addButtonWithTitle:@"Use Approximate Shaders"];
        if ([alert runModal] == NSAlertFirstButtonReturn) {
            NSOpenPanel *panel = NSOpenPanel.openPanel;
            panel.canChooseDirectories = YES; panel.canChooseFiles = NO; panel.treatsFilePackagesAsDirectories = YES;
            if ([panel runModal] == NSModalResponseOK) verified = Cod2xShadersSetup(Cod2xShaderBinaries(panel.URL.path), cache);
        }
    }
    if (verified) setenv("COD2_MAC_SHADER_CACHE", cache.fileSystemRepresentation, 1);
    else {
        unsetenv("COD2_MAC_SHADER_CACHE");
        fprintf(stderr, "CoD2 Silicon: no verified licensed Mac shaders; using approximate shaders (lighting and sky may differ).\n");
    }
}

static int Cod2xSetupAppArguments(NSBundle *bundle, NSString *arguments, char *buffer, int capacity)
{
    /* Foundation honors CFFIXED_USER_HOME; keep the engine's C preferences in the same home. */
    setenv("HOME", NSHomeDirectory().fileSystemRepresentation, 1);
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];
    if (!Cod2xNoninteractive()) [NSApp activateIgnoringOtherApps:YES];
    [NSFileManager.defaultManager createDirectoryAtPath:Cod2xAppHome() withIntermediateDirectories:YES attributes:nil error:nil];
    Cod2xMigrate();
    NSString *data = Cod2xGameDirectory(bundle);
    if (!data) return -1;
    NSCharacterSet *unsafe = [NSCharacterSet characterSetWithCharactersInString:@"\";+\r\n"];
    if ([data rangeOfCharacterFromSet:unsafe].location != NSNotFound ||
        [Cod2xAppHome() rangeOfCharacterFromSet:unsafe].location != NSNotFound) {
        Cod2xSetupAlert(@"The game folder path contains unsupported characters.", @"Move it to a path without quotes, semicolons, plus signs or newlines.");
        return -1;
    }
    if (!Cod2xSetupKey()) return -1;
    Cod2xSetupShaders(data);
    int length = snprintf(buffer, capacity, "%s +set fs_basepath \"%s\" +set fs_homepath \"%s\"",
        arguments.UTF8String, data.fileSystemRepresentation, Cod2xAppHome().fileSystemRepresentation);
    return length >= 0 && length < capacity ? length : -1;
}
