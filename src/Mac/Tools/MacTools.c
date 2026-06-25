#include "common_types.h"
#include "imports.h"

__attribute__((used)) static UInt8 sSystemCursorVisible[32] = { 1 };
static Point sGlobalMouse;

void MacTools_SetCursorID(short inCursorID);
void MacTools_ShowCursor(int inWarpIt, const Point *inWarpPt);
void MacTools_HideCursor(int inWarpIt);
Boolean MacTools_IsCursorVisible(void);
SInt16 MacTools_FindDiscVolume(CFStringRef inVolumeName, char *outVolumePath);
void MacTools_CenterRect(MacRect *ioRect, const MacRect *inBounds);
void MacTools_Sleep(UInt32 inMillisecondsToSleep);
void MacTools_SendQuitAppleEvent(const ProcessSerialNumber *inPSN);
Boolean MacTools_FindApplicationPSN(OSType inCreatorCode, ProcessSerialNumber *outPSN);
SInt32 MacTools_GetDictionaryValue(CFDictionaryRef inDictionaryRef, CFStringRef inKeyRef);
Boolean MacTools_GetIORegistryValue(io_registry_entry_t inEntryRef, CFStringRef inKeyRef, char *outValue);
Boolean MacTools_PostPrivateEventToWindow(WindowRef inWindow, UInt32 inMessage, UInt32 inParam1, UInt32 inParam2);
Boolean MacTools_QuestionAlert(CFStringRef inMessage1, CFStringRef inMessage2);
void ZN12StShowCursorD1Ev(void *_this);
void MacTools_MessageAlert(CFStringRef inMessage1, CFStringRef inMessage2, int inStop);
void MacTools_MessageAlertFromKey(CFStringRef inKeyRef, int inStop);
Boolean SearchForApplication(OSType inCreator, FSRef *outAppRef);
void StShowCursor_StShowCursor(const StShowCursor *_this);
void ZNSt6vectorI5FSRefSaIS0_EE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPS0_S2_EERKS0_(void);

void MacTools_SetCursorID(short inCursorID)
{
#ifdef BYTE_MATCH_DARWIN
    void *h = (void *)GetCursor(inCursorID);
    if (h != 0)
        SetCursor(*(void **)h);
#endif
}

void MacTools_ShowCursor(int inWarpIt, const Point *inWarpPt)
{
}

__attribute__((noinline)) void MacTools_HideCursor(int inWarpIt)
{
    sSystemCursorVisible[0] = 0;
}

Boolean MacTools_IsCursorVisible(void)
{
    return sSystemCursorVisible[0];
}

SInt16 MacTools_FindDiscVolume(CFStringRef inVolumeName, char *outVolumePath)
{
    return 0;
}

void MacTools_CenterRect(MacRect *ioRect, const MacRect *inBounds)
{
}

void MacTools_Sleep(UInt32 inMillisecondsToSleep)
{
    long long now = UpTime();
    long long deadline = AddDurationToAbsolute(inMillisecondsToSleep, now);
    MPDelayUntil(&deadline);
}

void MacTools_SendQuitAppleEvent(const ProcessSerialNumber *inPSN)
{
}

Boolean MacTools_FindApplicationPSN(OSType inCreatorCode, ProcessSerialNumber *outPSN)
{
    return 0;
}

SInt32 MacTools_GetDictionaryValue(CFDictionaryRef inDictionaryRef, CFStringRef inKeyRef)
{
    return 0;
}

Boolean MacTools_GetIORegistryValue(io_registry_entry_t inEntryRef, CFStringRef inKeyRef, char *outValue)
{
    return 0;
}

Boolean MacTools_PostPrivateEventToWindow(WindowRef inWindow, UInt32 inMessage, UInt32 inParam1, UInt32 inParam2)
{
    return 0;
}

Boolean MacTools_QuestionAlert(CFStringRef inMessage1, CFStringRef inMessage2)
{
    return 0;
}

void ZN12StShowCursorD1Ev(void *_this)
{
    if (*(unsigned char *)_this != 0)
        return;
    MacTools_HideCursor(1);
}

void MacTools_MessageAlert(CFStringRef inMessage1, CFStringRef inMessage2, int inStop)
{
}

void MacTools_MessageAlertFromKey(CFStringRef inKeyRef, int inStop)
{
}

Boolean SearchForApplication(OSType inCreator, FSRef *outAppRef)
{
    return 0;
}

void StShowCursor_StShowCursor(const StShowCursor *_this)
{
}

void ZNSt6vectorI5FSRefSaIS0_EE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPS0_S2_EERKS0_(void)
{
}
