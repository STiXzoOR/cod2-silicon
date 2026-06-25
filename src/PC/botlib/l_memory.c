#include "common_types.h"
#include "imports.h"
#include <string.h>

extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);

#define MEMORY_MAGIC 0x12345678

void *GetMemory(unsigned long size)
{
    int *ptr;

    ptr = (int *)Z_MallocInternal(size + 4);
    if (ptr == 0)
        return 0;

    *ptr = MEMORY_MAGIC;
    return ptr + 1;
}

void FreeMemory(void *ptr)
{
    int *block;

    block = (int *)ptr - 1;
    if (*block != MEMORY_MAGIC)
        return;

    Z_FreeInternal(block);
}

void *GetClearedMemory(unsigned long size)
{
    void *ptr;

    ptr = GetMemory(size);
    memset(ptr, 0, size);
    return ptr;
}
