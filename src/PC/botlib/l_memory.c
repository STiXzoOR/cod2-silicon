#include "common_types.h"
#include "imports.h"
#include <string.h>

extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);

#define MEMORY_MAGIC 0x12345678

/* x86 prepends a 4-byte magic header and returns block+4 (still 4-byte/ptr
 * aligned on ILP32).  On x64 a 4-byte header would leave every returned
 * pointer only 4-byte aligned, but botlib stores 8-byte pointers all over
 * these allocations (punctuationtable, source_t, define hashes, token copies).
 * Use a 16-byte header on x64 so the returned pointer keeps malloc's 16-byte
 * alignment; the magic still lives in the last 4 bytes before the user ptr. */
#if defined(COD2_X64)
#define GM_HDR 16
#else
#define GM_HDR 4
#endif

void *GetMemory(unsigned long size)
{
    char *base;

    base = (char *)Z_MallocInternal(size + GM_HDR);
    if (base == 0)
        return 0;

    *(int *)(base + GM_HDR - 4) = MEMORY_MAGIC;
    return base + GM_HDR;
}

void FreeMemory(void *ptr)
{
    char *block;

    if (*((int *)ptr - 1) != MEMORY_MAGIC)
        return;

    block = (char *)ptr - GM_HDR;
    Z_FreeInternal(block);
}

void *GetClearedMemory(unsigned long size)
{
    void *ptr;

    ptr = GetMemory(size);
    memset(ptr, 0, size);
    return ptr;
}
