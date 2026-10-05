#import <AppKit/AppKit.h>
#include <stdio.h>
#include <unistd.h>
#include "../../src/PC/qcommon/cod2x_url.h"

int Cod2xNativeApp_Arguments(char *buffer, int capacity) { (void)buffer; (void)capacity; return 0; }
@interface FixtureURLs : NSObject
- (void)openURL:(NSAppleEventDescriptor *)event reply:(NSAppleEventDescriptor *)reply;
@end
@implementation FixtureURLs
- (void)openURL:(NSAppleEventDescriptor *)event reply:(NSAppleEventDescriptor *)reply
{
    (void)reply;
    Cod2xURL link;
    NSString *text = [event paramDescriptorForKeyword:keyDirectObject].stringValue;
    if (Cod2x_ParseURL(text.UTF8String, &link)) { printf("fixture link %s\n", link.address); fflush(stdout); }
}
@end
int main(int argc, char **argv)
{
    @autoreleasepool {
        if (argc == 3 && !strcmp(argv[1], "find")) {
            for (NSRunningApplication *app in [NSRunningApplication runningApplicationsWithBundleIdentifier:@(argv[2])])
                if (!app.isTerminated) printf("%d\n", app.processIdentifier);
            return 0;
        }
        if (argc >= 3 && !strcmp(argv[1], "policy")) {
            NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:atoi(argv[2])];
            printf("%ld\n", (long)app.activationPolicy); return app ? 0 : 1;
        }
        if (argc == 4 && !strcmp(argv[1], "event")) {
            NSAppleEventDescriptor *event = [[NSAppleEventDescriptor alloc] initWithEventClass:kInternetEventClass eventID:kAEGetURL
                targetDescriptor:[NSAppleEventDescriptor descriptorWithProcessIdentifier:atoi(argv[2])]
                returnID:kAutoGenerateReturnID transactionID:kAnyTransactionID];
            [event setParamDescriptor:[NSAppleEventDescriptor descriptorWithString:@(argv[3])] forKeyword:keyDirectObject];
            NSError *error = nil;
            [event sendEventWithOptions:NSAppleEventSendNoReply timeout:2 error:&error];
            return error ? 1 : 0;
        }
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular]; [NSApp finishLaunching];
        FixtureURLs *handler = [[FixtureURLs alloc] init];
        [NSAppleEventManager.sharedAppleEventManager setEventHandler:handler andSelector:@selector(openURL:reply:) forEventClass:kInternetEventClass andEventID:kAEGetURL];
        printf("fixture ready %d\n", getpid()); fflush(stdout);
        NSString *home = @(getenv("WS25_FIXTURE_HOME"));
        [@"Synthetic crash report\n" writeToFile:[home stringByAppendingPathComponent:@"cod2_crash_fixture.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
        for (;;) {
            if ([NSFileManager.defaultManager fileExistsAtPath:[home stringByAppendingPathComponent:@"exit-fixture"]]) {
                [@"Synthetic crash report\n" writeToFile:[home stringByAppendingPathComponent:@"cod2_crash_fixture.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil]; return 37;
            }
            NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate dateWithTimeIntervalSinceNow:.05] inMode:NSDefaultRunLoopMode dequeue:YES];
            if (event) [NSApp sendEvent:event];
        }
    }
}
