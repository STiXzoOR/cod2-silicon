#if defined(COD2_CODX) && COD2_CODX
#include "cod2x.h"
#include <stddef.h>
#include <string.h>
#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#if defined(COD2_X64) && COD2_X64
#include <dlfcn.h>

static int Cod2x_SDKMachineUUID(char uuid[64])
{
    /* Legacy engine stubs export these names. Resolve the installed SDK images
       explicitly, without changing the original engine's import bindings. */
    void *iokit = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_LAZY | RTLD_LOCAL);
    void *core = dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", RTLD_LAZY | RTLD_LOCAL);
    CFMutableDictionaryRef (*matching)(const char *) = iokit ? dlsym(iokit, "IOServiceMatching") : NULL;
    CFTypeRef (*property)(io_registry_entry_t, CFStringRef, CFAllocatorRef, IOOptionBits) =
        iokit ? dlsym(iokit, "IORegistryEntryCreateCFProperty") : NULL;
    kern_return_t (*releaseObject)(io_object_t) = iokit ? dlsym(iokit, "IOObjectRelease") : NULL;
    Boolean (*stringBytes)(CFStringRef, char *, CFIndex, CFStringEncoding) =
        core ? dlsym(core, "CFStringGetCString") : NULL;
    void (*releaseValue)(CFTypeRef) = core ? dlsym(core, "CFRelease") : NULL;
    int ok = 0;
    uuid[0] = '\0';
    if (matching && property && releaseObject && stringBytes && releaseValue) {
        io_service_t platform = IOServiceGetMatchingService(kIOMainPortDefault, matching("IOPlatformExpertDevice"));
        if (platform) {
            CFTypeRef value = property(platform, CFSTR("IOPlatformUUID"), NULL, 0);
            releaseObject(platform);
            if (value) {
                if (CFGetTypeID(value) == CFStringGetTypeID())
                    ok = stringBytes((CFStringRef)value, uuid, 64, kCFStringEncodingASCII);
                releaseValue(value);
            }
        }
    }
    if (core) dlclose(core);
    if (iokit) dlclose(iokit);
    return ok;
}
#endif

static void Cod2x_Hex(const unsigned char *bytes, char hex[33])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 16; ++i) {
        hex[2 * i] = digits[bytes[i] >> 4];
        hex[2 * i + 1] = digits[bytes[i] & 15];
    }
    hex[32] = '\0';
}
#endif

int Cod2x_HwidFromUUID(const char *uuid, char id[33])
{
    id[0] = '\0';
#if defined(__APPLE__)
    /* Domain-separated SHA-256, truncated to 128 bits. No raw UUID is sent.
       CoD2x src/shared/server.cpp:428 requires a 32-character identity. */
    char input[] = "opencod2:cod2x:hwid2:v1:00000000-0000-0000-0000-000000000000";
    const size_t prefix = sizeof("opencod2:cod2x:hwid2:v1:") - 1;
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    size_t i;
    int nonzero = 0;
    if (!uuid || strlen(uuid) != 36)
        return 0;
    for (i = 0; i < 36; ++i) {
        char c = uuid[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-')
                return 0;
        } else {
            if (c >= 'A' && c <= 'F')
                c += 'a' - 'A';
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                return 0;
            nonzero |= c != '0';
        }
        input[prefix + i] = c;
    }
    if (!nonzero)
        return 0;
    CC_SHA256(input, (CC_LONG)(sizeof(input) - 1), digest);
    Cod2x_Hex(digest, id);
    return 1;
#else
    (void)uuid;
    return 0;
#endif
}

int Cod2x_ReadMachineHwid(char id[33])
{
    id[0] = '\0';
#if defined(__APPLE__) && defined(COD2_X64) && COD2_X64
    char uuid[64];
    return Cod2x_SDKMachineUUID(uuid) && Cod2x_HwidFromUUID(uuid, id);
#elif defined(__APPLE__)
    io_service_t platform;
    CFTypeRef value;
    char uuid[64];
    int ok = 0;
    platform = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (!platform)
        return 0;
    value = IORegistryEntryCreateCFProperty(platform, CFSTR("IOPlatformUUID"), kCFAllocatorDefault, 0);
    IOObjectRelease(platform);
    if (value) {
        if (CFGetTypeID(value) == CFStringGetTypeID() &&
            CFStringGetCString((CFStringRef)value, uuid, sizeof(uuid), kCFStringEncodingASCII))
            ok = Cod2x_HwidFromUUID(uuid, id);
        CFRelease(value);
    }
    return ok;
#else
    return 0;
#endif
}

int Cod2x_CDKeyHash(const char *key, char hash[33])
{
    hash[0] = '\0';
#if defined(__APPLE__)
    char normalized[33];
    size_t i, n = 0;
    unsigned char digest[CC_MD5_DIGEST_LENGTH];
    CC_MD5_CTX context;
    if (!key)
        return 0;
    for (i = 0; key[i] && i < 32; ++i) {
        unsigned char c = (unsigned char)key[i];
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            normalized[n++] = (char)c;
    }
    if (!n)
        return 0;
    /* Stock CD-key auth uses seeded MD5, not the standard digest. Verified in
       CoD2x src/other/CoD2MP_s.c:22160 and Mac MD5Init at :375944. */
    CC_MD5_Init(&context);
    context.A += 11u * 0xb684a3u;
    context.B += 71u * 0xb684a3u;
    context.C += 37u * 0xb684a3u;
    context.D += 97u * 0xb684a3u;
    CC_MD5_Update(&context, normalized, (CC_LONG)n);
    CC_MD5_Final(digest, &context);
    Cod2x_Hex(digest, hash);
    return 1;
#else
    (void)key;
    return 0;
#endif
}
#endif
