#include "common_types.h"
#include "imports.h"
#include <string.h>

extern void Com_Error(int code, const char *fmt, ...);

void MemFile_ReadData(MemoryFile *memFile, int bytes, void *data);
static void MemFile_WriteDataForArchive(MemoryFile *memFile, int bytes, void *data);

void MemFile_InitForWriting(MemoryFile *memFile, int size, void *buffer, int errorOnOverflow)
{
    memFile->buffer = buffer;
    memFile->bufferSize = size;
    memFile->bytesUsed = 0;
    memFile->errorOnOverflow = (Bool)errorOnOverflow;
    memFile->memoryOverflow = 0;
    *(void (**)(MemoryFile *, int, void *))&memFile->archiveProc = MemFile_WriteDataForArchive;
}

void MemFile_InitForReading(MemoryFile *memFile, int size, void *buffer)
{
    memFile->buffer = buffer;
    memFile->bufferSize = size;
    memFile->bytesUsed = 0;
    memFile->errorOnOverflow = 1;
    memFile->memoryOverflow = 0;
    *(void (**)(MemoryFile *, int, void *))&memFile->archiveProc = (void (*)(MemoryFile *, int, void *))MemFile_ReadData;
}

void MemFile_ReadData(MemoryFile *memFile, int bytes, void *data)
{
    if (bytes == 0)
        return;
    if (memFile->memoryOverflow)
        return;
    if (memFile->bytesUsed + bytes > memFile->bufferSize) {
        if (memFile->errorOnOverflow) {
            Com_Error(1, "Couldn't read %i bytes from %i-byte buffer (only %i bytes left)\n", bytes, memFile->bufferSize, memFile->bufferSize - memFile->bytesUsed);
        }
        memFile->memoryOverflow = 1;
        return;
    }
    memcpy(data, memFile->buffer + memFile->bytesUsed, bytes);
    memFile->bytesUsed += bytes;
}

void MemFile_WriteData(MemoryFile *memFile, int bytes, const void *data)
{
    if (bytes == 0)
        return;
    if (memFile->memoryOverflow)
        return;
    if (memFile->bytesUsed + bytes > memFile->bufferSize) {
        if (memFile->errorOnOverflow) {
            Com_Error(1, "Couldn't write %i bytes to %i-byte buffer (only %i bytes free)\n", bytes, memFile->bufferSize, memFile->bufferSize - memFile->bytesUsed);
        }
        memFile->memoryOverflow = 1;
        return;
    }
    memcpy(memFile->buffer + memFile->bytesUsed, data, bytes);
    memFile->bytesUsed += bytes;
}

static void MemFile_WriteDataForArchive(MemoryFile *memFile, int bytes, void *data)
{
    if (bytes == 0)
        return;
    if (memFile->memoryOverflow)
        return;
    if (memFile->bytesUsed + bytes > memFile->bufferSize) {
        if (memFile->errorOnOverflow) {
            Com_Error(1, "Couldn't write %i bytes to %i-byte buffer (only %i bytes free)\n", bytes, memFile->bufferSize, memFile->bufferSize - memFile->bytesUsed);
        }
        memFile->memoryOverflow = 1;
        return;
    }
    memcpy(memFile->buffer + memFile->bytesUsed, data, bytes);
    memFile->bytesUsed += bytes;
}

const char *MemFile_ReadCString(MemoryFile *memFile)
{
    int start, pos;

    if (memFile->memoryOverflow)
        return "";

    start = memFile->bytesUsed;
    pos = start;

    while (memFile->buffer[pos] != '\0') {
        pos++;
        memFile->bytesUsed = pos;
        if (pos == memFile->bufferSize) {
            if (memFile->errorOnOverflow) {
                Com_Error(1, "End of memory file while reading string (%i bytes read)\n", pos - start);
            }
            memFile->memoryOverflow = 1;
            return "";
        }
    }

    pos++;
    memFile->bytesUsed = pos;
    return (const char *)(memFile->buffer + start);
}

void MemFile_WriteCString(MemoryFile *memFile, const char *string)
{
    int len = strlen(string) + 1;
    MemFile_WriteData(memFile, len, string);
}
