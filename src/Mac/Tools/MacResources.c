#include "common_types.h"
#include "imports.h"

HCURSOR SetWinCursor(HCURSOR hCursor);

__attribute__((used, packed, aligned(4)))
UInt32 sResult[8] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};
#define sResult (*(int *)&sResult[0])
extern int sSavedWinCursor;
extern int sCurrentCursor;
static CFStringRef sResult_00334b04;
static CFStringRef sResult_00334b08;
static CFStringRef sResult_00334b0c;
static CFStringRef sResult_00334b10;
static SInt32 sResult_00308060;
static CFStringRef sResult_00334b18;
static CFStringRef sResult_00334b1c;
static CFStringRef sResult_00334b20;
static CFStringRef sResult_00334b24;
static CFStringRef sResult_00334b28;
static CFStringRef sResult_00334b2c;

static int MacResources_GetLocalizedString(int keyAddr, int tableAddr);
int MacResources_GetGameString(int inKeyRef, unsigned char *outString);
int MacResources_GetProductFamily(void);
int MacResources_GetDiscName(void);
SInt32 MacResources_GetLanguageCode(void);
CFStringRef MacResources_GetNeedsNewerOSError(void);
int MacResources_GetNoQuickTimeError(void);
CFStringRef MacResources_GetMissingDataFolderError(void);
CFStringRef MacResources_GetInsertDiscError(void);
CFStringRef MacResources_GetNeeds32BitError(void);
CFStringRef MacResources_GetCantRunFromDiscError(void);

static int MacResources_GetLocalizedString(int keyAddr, int tableAddr)
{
    int bundle = CFBundleGetMainBundle();
    return CFBundleCopyLocalizedString(bundle, keyAddr, keyAddr, tableAddr);
}

int MacResources_GetGameString(int inKeyRef, unsigned char *outString)
{
    int bundle = CFBundleGetMainBundle();
    int stringRef = CFBundleCopyLocalizedString(bundle, inKeyRef, inKeyRef, 0x32e6b4);
    if (!stringRef)
        return 0;

    int result = CFStringGetPascalString(stringRef, outString, 0x100, 0);
    CFRelease(stringRef);
    return result != 0;
}

int MacResources_GetProductFamily(void)
{
    if (sResult)
        return sResult;

    int bundle = CFBundleGetMainBundle();
    sResult = CFBundleCopyLocalizedString(bundle, 0x32e6c4, 0x32e6c4, 0x32e6b4);
    return sResult;
}

int MacResources_GetDiscName(void)
{
    if (sResult)
        return sResult;

    int bundle = CFBundleGetMainBundle();
    sResult = CFBundleCopyLocalizedString(bundle, 0x32e704, 0x32e704, 0x32e6b4);
    return sResult;
}

SInt32 MacResources_GetLanguageCode(void)
{
    return 0;
}

CFStringRef MacResources_GetNeedsNewerOSError(void)
{
    return 0;
}

int MacResources_GetNoQuickTimeError(void)
{
    if (sResult)
        return sResult;

    int bundle = CFBundleGetMainBundle();
    sResult = CFBundleCopyLocalizedString(bundle, 0x32e744, 0x32e744, 0);
    return sResult;
}

CFStringRef MacResources_GetMissingDataFolderError(void)
{
    return 0;
}

CFStringRef MacResources_GetInsertDiscError(void)
{
    return 0;
}

CFStringRef MacResources_GetNeeds32BitError(void)
{
    return 0;
}

CFStringRef MacResources_GetCantRunFromDiscError(void)
{
    return 0;
}
