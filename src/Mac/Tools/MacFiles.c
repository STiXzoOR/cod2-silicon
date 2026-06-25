#include "common_types.h"
#include "imports.h"
#include <unistd.h>

void MacFiles_CleanPath(const char *inPath, char *outPath, int inForHFS);
static void RemoveDirectoryContents(void);
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
            c = inForHFS ? ':' : '/';
        }
        *dst = c;
        dst++;
        src++;
        c = *src;
    }
    *dst = '\0';
}

static void RemoveDirectoryContents(void)
{
}

OSStatus MacFiles_RemoveDirectoryA(const char *inPath)
{
    return 0;
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
