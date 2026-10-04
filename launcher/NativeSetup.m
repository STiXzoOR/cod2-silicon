#import <AppKit/AppKit.h>
#import "NativeSetup.h"
#include <stdio.h>
#include "../src/platform/cod2x_native_setup.h"
#include "../src/PC/qcommon/cod2x_url.h"

NSString *LauncherHome(void) { return Cod2xAppHome(); }
void LauncherMigrate(void)
{
    [NSFileManager.defaultManager createDirectoryAtPath:Cod2xAppHome() withIntermediateDirectories:YES attributes:nil error:nil];
    Cod2xMigrate();
}
NSString *LauncherFindData(NSString *selected)
{
    if (selected) return Cod2xDataInFolder(selected);
    NSString *override = Cod2xSetupOverride("COD2_SETUP_GAME_DIR");
    if (override) return Cod2xDataInFolder(override);
    NSString *saved = [NSString stringWithContentsOfFile:[Cod2xAppHome() stringByAppendingPathComponent:@"data-path.txt"] encoding:NSUTF8StringEncoding error:nil];
    NSMutableArray *candidates = [NSMutableArray array];
    if (saved) [candidates addObject:[saved stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]];
    id configured = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CoD2GameDirectory"];
    if ([configured isKindOfClass:NSString.class]) [candidates addObject:configured];
    [candidates addObject:[NSHomeDirectory() stringByAppendingPathComponent:@"Games/CoD2"]];
    [candidates addObjectsFromArray:Cod2xInstallFolders()];
    for (NSString *candidate in candidates) {
        NSString *data = Cod2xDataInFolder(candidate);
        if (data) return data;
    }
    return nil;
}
BOOL LauncherKeyValid(NSString *key) { return Cod2xKeyValid(key); }
BOOL LauncherPrepareShaders(NSString *data, NSString *selected)
{
    BOOL directory = NO;
    NSMutableArray *binaries = Cod2xShaderBinaries(selected ?: data).mutableCopy;
    if (selected && [NSFileManager.defaultManager fileExistsAtPath:selected isDirectory:&directory] && !directory)
        [binaries insertObject:selected atIndex:0];
    return Cod2xShadersSetup(binaries, [Cod2xAppHome() stringByAppendingPathComponent:@"shaders"]);
}
BOOL LauncherVerifyShaders(void)
{
    return Cod2xShadersVerify([Cod2xAppHome() stringByAppendingPathComponent:@"shaders"]);
}
NSDictionary *LauncherParseLink(NSString *link)
{
    Cod2xURL parsed;
    if (!Cod2x_ParseURL(link.UTF8String, &parsed)) return nil;
    return @{@"address": @(parsed.address), @"password": @(parsed.password)};
}
