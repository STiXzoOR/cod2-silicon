#include "common_types.h"
#include "imports.h"

extern void *Hunk_ReallocateTempMemoryInternal(int minimumSize);

extern int currentPos;

void TempMemoryReset(void)
{
    currentPos = 0;
}

char *TempMalloc(int len)
{
    int newCurrentPos;
    char *base;

    newCurrentPos = currentPos + len;
    base = (char *)Hunk_ReallocateTempMemoryInternal(newCurrentPos);
    base += currentPos;
    currentPos = newCurrentPos;
    return base;
}

char *TempMallocAlign(int len)
{
    int newCurrentPos;
    char *base;

    newCurrentPos = currentPos + len;
    base = (char *)Hunk_ReallocateTempMemoryInternal(newCurrentPos);
    base += currentPos;
    currentPos = newCurrentPos;
    return base;
}

char *TempMallocAlignStrict(int len)
{
    int newCurrentPos;
    char *base;

    newCurrentPos = currentPos + len;
    base = (char *)Hunk_ReallocateTempMemoryInternal(newCurrentPos);
    base += currentPos;
    currentPos = newCurrentPos;
    return base;
}

void TempMemorySetPos(char *pos)
{
    char *base;

    base = (char *)Hunk_ReallocateTempMemoryInternal(currentPos);
    base += currentPos;
    currentPos -= (int)(base - pos);
    Hunk_ReallocateTempMemoryInternal(currentPos);
}
