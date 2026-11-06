#if defined(__APPLE__) && defined(COD2_X64)
#include <CoreServices/CoreServices.h>
#else
#include "common_types.h"
#include "imports.h"
#endif

void MacStrings_CopyAndClean(const char *inSrcString, char *inDstString, int inDstSize);

void MacStrings_GetCString(const HFSUniStr255 *inUniStr, char *outCString, int inMaxString)
{
    CFStringRef stringRef;

    *outCString = '\0';

    stringRef = (CFStringRef)CFStringCreateWithCharacters(NULL, (const UniChar *)inUniStr + 1, *(const UInt16 *)inUniStr);
    if (!stringRef)
        return;

    CFStringGetCString(stringRef, outCString, (short)inMaxString, 0);
    CFRelease(stringRef);
}

void MacStrings_CopyAndClean(const char *inSrcString, char *inDstString, int inDstSize)
{
#if defined(COD2_X64)
    int capacity = (short)inDstSize;
    int count = 0;
    int previousCR = 0;
    int previousSpace = 0;
    if (capacity <= 0)
        return;
    while (*inSrcString && count < capacity - 1) {
        unsigned char c = (unsigned char)*inSrcString++;
        if (c == '\n')
            continue;
        if (c == '\r') {
            if (previousCR) {
                /* Mac writes both CRs even with only one byte remaining. */
                inDstString[count++] = '\r';
                if (count < capacity - 1)
                    inDstString[count++] = '\r';
                previousCR = 0;
                continue;
            }
            previousCR = 1;
            c = ' ';
            if (previousSpace)
                continue;
            previousSpace = 1;
        } else if (c == ' ') {
            previousCR = 0;
            if (previousSpace)
                continue;
            previousSpace = 1;
        } else {
            previousCR = 0;
            previousSpace = 0;
        }
        inDstString[count++] = (char)c;
    }
    inDstString[count] = '\0';
#endif
}
