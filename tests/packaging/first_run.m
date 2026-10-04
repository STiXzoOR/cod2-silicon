#import <AppKit/AppKit.h>
#include <stdio.h>
#include <string.h>
#include "platform/cod2x_native_setup.h"

int Cod2xNativeApp_Arguments(char *buffer, int capacity)
{
    return Cod2xSetupAppArguments(NSBundle.mainBundle,
        [NSBundle.mainBundle objectForInfoDictionaryKey:@"CoD2LaunchArguments"], buffer, capacity);
}

int main(void)
{
    @autoreleasepool {
        char arguments[4096];
        if (Cod2xNativeApp_Arguments(arguments, sizeof(arguments)) < 0) return 1;
        if (!strstr(arguments, "CoD2 Silicon") || !strstr(arguments, "+set fs_basepath")) return 2;
        puts("CoD2 Silicon first-run paths prepared");
        return 0;
    }
}
