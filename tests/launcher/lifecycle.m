#import <AppKit/AppKit.h>
#include <stdio.h>
#include <unistd.h>
#include "../../src/PC/qcommon/cod2x_url.h"

/* Built with LIFECYCLE_STOCK, the fixture stands in for the original 1.3 engine: the same app layer
 * (bundle arguments, link handler), no CoD2x layer. */
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
    if (Cod2x_ParseURL(text.UTF8String, &link)) {
        printf("fixture link %s%s%s\n", link.address, link.hasPassword ? " password " : "", link.hasPassword ? link.password : "");
        fflush(stdout);
    }
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
        NSMutableArray *arguments = [NSMutableArray array];
        for (int i = 1; i < argc; ++i) [arguments addObject:@(argv[i])];
        NSData *receipt = [NSJSONSerialization dataWithJSONObject:arguments options:0 error:nil];
        [receipt writeToFile:[home stringByAppendingPathComponent:@"fixture-arguments.json"] atomically:YES];
        [@"Synthetic crash report\n" writeToFile:[home stringByAppendingPathComponent:@"cod2_crash_fixture.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
        for (;;) {
            if ([NSFileManager.defaultManager fileExistsAtPath:[home stringByAppendingPathComponent:@"exit-fixture"]]) {
                NSString *active = [NSString stringWithContentsOfFile:[home stringByAppendingPathComponent:@"main/players/active.txt"] encoding:NSUTF8StringEncoding error:nil];
                NSString *config = [home stringByAppendingPathComponent:[NSString stringWithFormat:@"main/players/%@/config_mp.cfg", active]];
                NSString *text = [NSString stringWithContentsOfFile:config encoding:NSUTF8StringEncoding error:nil];
#ifdef LIFECYCLE_STOCK
                /* Like the original engine: it refuses CoD2x's 125, keeps its default and writes that back. */
                text = [text stringByReplacingOccurrencesOfString:@"seta cl_maxpackets \"125\"" withString:@"seta cl_maxpackets \"30\""];
#endif
                if (text) [[text stringByAppendingString:@"seta sensitivity \"6\"\n"] writeToFile:config atomically:YES encoding:NSUTF8StringEncoding error:nil];
                [@"Synthetic crash report\n" writeToFile:[home stringByAppendingPathComponent:@"cod2_crash_fixture.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil]; return 37;
            }
            NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate dateWithTimeIntervalSinceNow:.05] inMode:NSDefaultRunLoopMode dequeue:YES];
            if (event) [NSApp sendEvent:event];
        }
    }
}
