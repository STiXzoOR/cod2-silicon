#include "common_types.h"
#include "imports.h"
#include <stdio.h>

size_t FS_FileRead(void *ptr, size_t size, size_t nitems, FILE *stream)
{
    return fread(ptr, size, nitems, stream);
}

size_t FS_FileWrite(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
    return fwrite(ptr, size, nitems, stream);
}

FILE *FS_FileOpen(const char *filename, const char *mode)
{
    return fopen(filename, mode);
}

int FS_FileClose(FILE *stream)
{
    return fclose(stream);
}

int FS_FileSeek(FILE *file, long int offset, int whence)
{
    return fseek(file, offset, whence);
}
