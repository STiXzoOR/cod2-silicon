#if defined(__APPLE__) && defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#import <AppKit/AppKit.h>
#include "cod2x_native.h"
#include "PC/qcommon/cod2x_url.h"
#include <string.h>
#include <stdio.h>

extern void Cbuf_AddText(const char *text);
extern void Dvar_SetStringByName(const char *name, const char *value);
static char pendingURL[512];

@interface Cod2xURLHandler : NSObject
- (void)openURL:(NSAppleEventDescriptor *)event reply:(NSAppleEventDescriptor *)reply;
@end
@implementation Cod2xURLHandler
- (void)openURL:(NSAppleEventDescriptor *)event reply:(NSAppleEventDescriptor *)reply
{
    (void)reply;
    NSString *url = [event paramDescriptorForKeyword:keyDirectObject].stringValue;
    if (!Cod2xNativeURL_Queue(url.UTF8String))
        fprintf(stderr, "CoD2x: rejected launch link; only connect and password are accepted.\n");
}
@end

static Cod2xURLHandler *urlHandler;

int Cod2xNativeApp_Arguments(char *buffer, int capacity)
{
    id arguments = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CoD2LaunchArguments"];
    if (![arguments isKindOfClass:NSString.class])
        return 0;
    int length = snprintf(buffer, capacity, "%s", [arguments UTF8String]);
    return length >= 0 && length < capacity ? length : -1;
}

int Cod2xNativeURL_Queue(const char *url)
{
    Cod2xURL parsed;
    char commands[sizeof(pendingURL)];
    if (!Cod2x_ParseURL(url, &parsed) || !Cod2x_URLCommands(&parsed, commands, sizeof(commands)))
        return 0;
    memcpy(pendingURL, commands, strlen(commands) + 1);
    return 1;
}

void Cod2xNativeURL_Install(void)
{
    if (urlHandler) return;
    urlHandler = [[Cod2xURLHandler alloc] init];
    [NSAppleEventManager.sharedAppleEventManager setEventHandler:urlHandler andSelector:@selector(openURL:reply:)
        forEventClass:kInternetEventClass andEventID:kAEGetURL];
}

void Cod2xNativeURL_SetupPaths(void)
{
    id path = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CoD2GameDirectory"];
    if ([path isKindOfClass:NSString.class] && [path length])
        Dvar_SetStringByName("fs_basepath", [path UTF8String]);
}

void Cod2xNativeURL_Shutdown(void)
{
    [NSAppleEventManager.sharedAppleEventManager removeEventHandlerForEventClass:kInternetEventClass andEventID:kAEGetURL];
    urlHandler = nil;
    memset(pendingURL, 0, sizeof(pendingURL));
}

void Cod2xNativeURL_Frame(void)
{
    if (pendingURL[0]) {
        Cbuf_AddText(pendingURL);
        memset(pendingURL, 0, sizeof(pendingURL));
    }
}
#endif
