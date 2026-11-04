#if defined(__APPLE__) && defined(COD2_X64)
#include <CoreServices/CoreServices.h>
typedef struct {
    OSStatus status;
    ItemCount objects;
    FSCatalogInfo catalogInfo;
} MacRemoveDirectoryInfo;
#else
#include "common_types.h"
#include "imports.h"
#endif
#include <unistd.h>

void MacFiles_CleanPath(const char *inPath, char *outPath, int inForHFS);
#if defined(__APPLE__) && defined(COD2_X64)
static void RemoveDirectoryContents(const FSRef *directory, MacRemoveDirectoryInfo *info);
#else
static void RemoveDirectoryContents(void);
#endif
OSStatus MacFiles_RemoveDirectoryA(const char *inPath);
int MacFiles_access(const char *inPath, int inMode);

void MacFiles_CleanPath(const char *inPath, char *outPath, int inForHFS)
{
    const char *src;
    char *dst;
    char c;

    src = inPath;
    dst = outPath;

    c = *src;
    while (c != '\0') {
        if (c == '\\') {
#if defined(COD2_X64)
            c = (unsigned char)inForHFS ? ':' : '/';
#else
            c = inForHFS ? ':' : '/';
#endif
        }
        *dst = c;
        dst++;
        src++;
        c = *src;
    }
    *dst = '\0';
}

#if defined(__APPLE__) && defined(COD2_X64)
static void RemoveDirectoryContents(const FSRef *directory, MacRemoveDirectoryInfo *info)
{
    FSIterator iterator;
    FSRef child;

    info->status = FSOpenIterator(directory, kFSIterateDelete, &iterator);
    if (info->status != noErr)
        return;

    for (;;) {
        info->status = FSGetCatalogInfoBulk(iterator, 1, &info->objects, NULL,
                                            kFSCatInfoNodeFlags, &info->catalogInfo,
                                            &child, NULL, NULL);
        if (info->status != noErr)
            break;
        if (info->objects == 1) {
            UInt16 flags = info->catalogInfo.nodeFlags;
            if (flags & kFSNodeIsDirectoryMask)
                RemoveDirectoryContents(&child, info);
            if (info->status != noErr)
                break;
            if (flags & kFSNodeLockedMask) {
                info->catalogInfo.nodeFlags = flags & ~kFSNodeLockedMask;
                FSSetCatalogInfo(&child, kFSCatInfoNodeFlags, &info->catalogInfo);
            }
            info->status = FSDeleteObject(&child);
            if (info->status != noErr)
                break;
        }
    }
    if (info->status == errFSNoMoreItems)
        info->status = noErr;
    FSCloseIterator(iterator);
}
#else
static void RemoveDirectoryContents(void)
{
}
#endif

OSStatus MacFiles_RemoveDirectoryA(const char *inPath)
{
#if defined(__APPLE__) && defined(COD2_X64)
    FSRef directory;
    Boolean isDirectory;
    MacRemoveDirectoryInfo info;
    OSStatus status = FSPathMakeRef((const UInt8 *)inPath, &directory, &isDirectory);
    if (status != noErr)
        return status;
    if (!isDirectory)
        return paramErr;

    RemoveDirectoryContents(&directory, &info);
    if (info.status == noErr)
        FSDeleteObject(&directory);
    /* Mac 0x8b68-0x8b7a returns the saved initial success even if deletion fails. */
    return status;
#else
    return 0;
#endif
}

int MacFiles_access(const char *inPath, int inMode)
{
    char tempPath[1024];
    const char *src;
    char *dst;
    char c;

    src = inPath;
    dst = tempPath;

    c = *src;
    while (c != '\0') {
        if (c == '\\') {
            c = '/';
        }
        *dst = c;
        dst++;
        src++;
        c = *src;
    }
    *dst = '\0';

    return access(tempPath, inMode);
}
