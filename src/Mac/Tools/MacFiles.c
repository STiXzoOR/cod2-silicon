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
#if defined(__linux__) && defined(COD2_X64)
#include <ftw.h>
#include <stdio.h>
#include <sys/stat.h>
#endif

void MacFiles_CleanPath(const char *inPath, char *outPath, int inForHFS);
#if defined(__APPLE__) && defined(COD2_X64)
static void RemoveDirectoryContents(const FSRef *directory, MacRemoveDirectoryInfo *info);
#elif !(defined(__linux__) && defined(COD2_X64)) && !COD2_WINDOWS_NATIVE
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
#elif defined(__linux__) && defined(COD2_X64)
static int RemoveDirectoryEntry(const char *path, const struct stat *status, int type, struct FTW *walk)
{
    (void)status;
    (void)type;
    (void)walk;
    return remove(path);
}
#elif COD2_WINDOWS_NATIVE
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

static void RemoveDirectoryEntries(const char *path)
{
    DIR *directory = opendir(path);
    struct dirent *entry;
    char child[512];
    struct stat status;

    if (!directory)
        return;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (stat(child, &status) == 0 && S_ISDIR(status.st_mode)) {
            RemoveDirectoryEntries(child);
            rmdir(child);
        } else {
            remove(child);
        }
    }
    closedir(directory);
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
#elif defined(__linux__) && defined(COD2_X64)
    struct stat status;
    if (stat(inPath, &status) != 0)
        return -43; /* fnfErr, as FSPathMakeRef reports. */
    if (!S_ISDIR(status.st_mode))
        return -50; /* paramErr */
    /* Like the Mac path, the result reports only that the directory was found. */
    nftw(inPath, RemoveDirectoryEntry, 16, FTW_DEPTH | FTW_PHYS);
    return 0;
#elif COD2_WINDOWS_NATIVE
    struct stat status;
    if (stat(inPath, &status) != 0)
        return -43; /* fnfErr, as FSPathMakeRef reports. */
    if (!S_ISDIR(status.st_mode))
        return -50; /* paramErr */
    RemoveDirectoryEntries(inPath);
    rmdir(inPath);
    return 0;
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
