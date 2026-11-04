#if defined(__APPLE__) && defined(COD2_X64)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <string.h>

int MacLaunch_OpenURL(const char *text)
{
    CFURLRef url = CFURLCreateWithBytes(NULL, (const UInt8 *)text, strlen(text), 0, NULL);
    if (!url)
        return 0;
    OSStatus status = LSOpenCFURLRef(url, NULL);
    CFRelease(url);
    return status == noErr;
}

int MacLaunch_Application(const char *bundleName)
{
    CFURLRef bundle = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    if (!bundle)
        return 0;
    CFURLRef folder = CFURLCreateCopyDeletingLastPathComponent(NULL, bundle);
    CFRelease(bundle);
    if (!folder)
        return 0;
    CFStringRef name = CFStringCreateWithCString(NULL, bundleName, kCFStringEncodingUTF8);
    CFURLRef application = name ? CFURLCreateCopyAppendingPathComponent(NULL, folder, name, true) : NULL;
    if (name)
        CFRelease(name);
    CFRelease(folder);
    if (!application)
        return 0;
    OSStatus status = LSOpenCFURLRef(application, NULL);
    CFRelease(application);
    return status == noErr;
}

int MacRuntime_UserActivity(void)
{
    /* IOKit updates or replaces this timed assertion; pass its latest ID back. */
    static IOPMAssertionID assertion = kIOPMNullAssertionID;
    return IOPMAssertionDeclareUserActivity(CFSTR("CoD2 loading"), kIOPMUserActiveLocal, &assertion);
}
#endif
