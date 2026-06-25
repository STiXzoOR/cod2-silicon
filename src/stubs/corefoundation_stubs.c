#include "corefoundation_stubs.h"

CFIndex CFArrayGetCount(CFArrayRef array)
{
    return 0;
}

const void *CFArrayGetValueAtIndex(CFArrayRef array, CFIndex idx)
{
    return 0;
}

CFNumberRef CFNumberCreate(CFAllocatorRef alloc, int theType, const void *valuePtr)
{
    return (CFNumberRef)0;
}

int CFNumberGetValue(CFNumberRef number, int theType, void *valuePtr)
{
    return 0;
}

const void *CFDictionaryGetValue(CFDictionaryRef dict, const void *key)
{
    return 0;
}

void CFRetain(CFTypeRef cf)
{
}

int CFPreferencesAppSynchronize(void)
{
    return 1;
}

static void *kCFPrefsCurrentApp_storage;
void **kCFPreferencesCurrentApplication = &kCFPrefsCurrentApp_storage;
