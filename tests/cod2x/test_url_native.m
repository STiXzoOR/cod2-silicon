#import <AppKit/AppKit.h>
#include "platform/cod2x_native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char queued[512];
static int queueCount;
static char gamePath[1024];
void Dvar_SetStringByName(const char *name, const char *value)
{
    assert(!strcmp(name, "fs_basepath") && strlen(value) < sizeof(gamePath));
    strcpy(gamePath, value);
}
void Cbuf_AddText(const char *text)
{
    assert(strlen(text) < sizeof(queued));
    strcpy(queued, text);
    ++queueCount;
}
@interface NSObject (Cod2xURLProbe)
- (void)openURL:(NSAppleEventDescriptor *)event reply:(NSAppleEventDescriptor *)reply;
@end

int main(void)
{
    @autoreleasepool {
        Cod2xNativeURL_SetupPaths();
        const char *expectedGame = getenv("WS10_EXPECT_GAME");
        if (expectedGame)
            assert(!strcmp(gamePath, expectedGame));
        Cod2xNativeURL_Install();
        assert(Cod2xNativeURL_Queue("cod2x://%2Bconnect%20localhost/"));
        assert(!Cod2xNativeURL_Queue("cod2x://%2Bquit/"));
        Cod2xNativeURL_Frame();
        assert(queueCount == 1 && !strcmp(queued, "password \"\"\nconnect localhost\n"));
        Cod2xNativeURL_Frame();
        assert(queueCount == 1);
        NSObject *handler = [[NSClassFromString(@"Cod2xURLHandler") alloc] init];
        NSAppleEventDescriptor *event = [NSAppleEventDescriptor appleEventWithEventClass:kInternetEventClass
            eventID:kAEGetURL targetDescriptor:nil returnID:kAutoGenerateReturnID transactionID:kAnyTransactionID];
        [event setParamDescriptor:[NSAppleEventDescriptor descriptorWithString:@"cod2x://%2Bconnect%20localhost:28960%20%2Bpassword%20pivo/"]
            forKeyword:keyDirectObject];
        [handler openURL:event reply:nil];
        Cod2xNativeURL_Frame();
        assert(queueCount == 2 && !strcmp(queued, "password \"pivo\"\nconnect localhost:28960\n"));
        Cod2xNativeURL_Shutdown();
        puts("cod2x native AppleEvent decoding and command queue passed");
    }
    return 0;
}
