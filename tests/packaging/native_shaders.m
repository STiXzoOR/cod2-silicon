#import "platform/cod2x_native_shaders.h"
#include <string.h>
int main(int argc, const char **argv)
{
    @autoreleasepool {
        if (argc != 3) return 2;
        NSString *cache = [NSString stringWithUTF8String:argv[2]];
        if (!strcmp(argv[1], "--verify")) return Cod2xShadersVerify(cache) ? 0 : 1;
        return Cod2xShadersSetup(@[[NSString stringWithUTF8String:argv[1]]], cache) ? 0 : 1;
    }
}
