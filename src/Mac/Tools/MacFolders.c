#include "common_types.h"
#include "imports.h"
#include "bytematch.h"

#include <string.h>

#define noErr 0

static SInt16 sAppFolderVRefNum;
static SInt32 sAppFolderDirID;
static FSRef sAppBundleRef;
static SInt32 sExecutableDirID;
static SInt32 sDataFolderDirID;

SInt32 MacFolders_GetExecutableFolderID(void);
OSStatus MacFolders_GetApplicationFolderItemPath(const char *inItem, char *outPath, int inMaxPath);
OSStatus MacFolders_GetApplicationFolderItemRef(const char *inItem, FSRef *outRef);
SInt32 MacFolders_GetDataFolderID(void);
OSStatus MacFolders_GetDataFolderPath(char *outPath, int inMaxPath);
OSStatus MacFolders_GetDataFolderItemPath(const char *inItem, char *outPath, int inMaxPath);
OSStatus MacFolders_GetDataFolderItemRef(const char *inItem, FSRef *outRef);
SInt16 MacFolders_GetApplicationVRefNum(void);
unsigned char MacFolders_GetApplicationRef(FSRef *outRef);

extern void MacFiles_CleanPath(const char *inPath, char *outPath, int inForHFS);

static void EnsureAppFolderInitialized(void)
{
    ProcessSerialNumber psn;
    FSCatalogInfo info;

    if (sAppFolderDirID != 0)
        return;

    if (GetCurrentProcess(&psn) != noErr)
        return;

    if (GetProcessBundleLocation(&psn, &sAppBundleRef) != noErr)
        return;

    if (FSGetCatalogInfo(&sAppBundleRef, 0xC, &info, NULL, NULL, NULL) != noErr)
        return;

    sAppFolderVRefNum = info.volume;
    sAppFolderDirID = info.parentDirID;
}

SInt32 MacFolders_GetExecutableFolderID(void)
{
    return 0;
}

OSStatus MacFolders_GetApplicationFolderItemPath(const char *inItem, char *outPath, int inMaxPath)
{
    return 0;
}

OSStatus MacFolders_GetApplicationFolderItemRef(const char *inItem, FSRef *outRef)
{
    char path[0x400];
    OSStatus error;

    error = MacFolders_GetApplicationFolderItemPath(inItem, path, 0x400);
    if (error != noErr)
        return error;

    return FSPathMakeRef((const UInt8 *)path, outRef, NULL);
}

SInt32 MacFolders_GetDataFolderID(void)
{
    return 0;
}

OSStatus MacFolders_GetDataFolderPath(char *outPath, int inMaxPath)
{
    return 0;
}

OSStatus MacFolders_GetDataFolderItemPath(const char *inItem, char *outPath, int inMaxPath)
{
    FSSpec spec;
    FSRef ref;
    OSStatus error;

    EnsureAppFolderInitialized();

    spec.vRefNum = sAppFolderVRefNum;
    spec.parID = MacFolders_GetDataFolderID();
    spec.name[0] = 0;

    error = FSpMakeFSRef(&spec, &ref);
    if (error != noErr)
        return error;

    error = FSRefMakePath(&ref, (UInt8 *)outPath, inMaxPath);
    if (error != noErr)
        return error;

    if (inItem[0] != '/' && inItem[0] != '\\') {
        size_t len = strlen(outPath);
        outPath[len] = '/';
        outPath[len + 1] = '\0';
    }

    strcat(outPath, inItem);
    MacFiles_CleanPath(outPath, outPath, 0);

    return noErr;
}

OSStatus MacFolders_GetDataFolderItemRef(const char *inItem, FSRef *outRef)
{
    char path[0x400];
    OSStatus error;

    error = MacFolders_GetDataFolderItemPath(inItem, path, 0x400);
    if (error != noErr)
        return error;

    return FSPathMakeRef((const UInt8 *)path, outRef, NULL);
}

SInt16 MacFolders_GetApplicationVRefNum(void)
{
    EnsureAppFolderInitialized();

    return sAppFolderVRefNum;
}

unsigned char MacFolders_GetApplicationRef(FSRef *outRef)
{
    EnsureAppFolderInitialized();

    memcpy(outRef, &sAppBundleRef, sizeof(FSRef));

    return 0;
}
