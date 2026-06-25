#include "common_types.h"
#include "imports.h"

extern void *Z_MallocGarbageInternal(int size);
extern void Z_FreeInternal(void *ptr);
extern int FS_FOpenFileRead(const char *filename, int *file, int uniqueFILE);
extern int FS_Read(void *buffer, int len, int f);
extern void FS_FCloseFile(fileHandle_t f);
extern unsigned int Com_BlockChecksum(const void *buffer, int length);
extern void Com_Error(int code, const char *fmt, ...);
extern char *va(const char *fmt, ...);

static dheader_t *comBspGlob;
static int comBspFileSize;
static unsigned int comBspChecksum;

const dheader_t *Com_GetBsp(int *fileSize, unsigned int *checksum)
{
    if (fileSize)
        *fileSize = comBspFileSize;
    if (checksum)
        *checksum = comBspChecksum;
    return comBspGlob;
}

void Com_UnloadBsp(void)
{
    Z_FreeInternal(comBspGlob);
    comBspGlob = 0;
}

void Com_LoadBsp(const char *filename)
{
    int h;
    int bytesRead;
    int i;

    comBspFileSize = FS_FOpenFileRead(filename, &h, 0);
    if (!h) {
        Com_Error(1, va("EXE_ERR_COULDNT_LOAD%s", filename));
    }

    comBspGlob = (dheader_t *)Z_MallocGarbageInternal(comBspFileSize);
    bytesRead = FS_Read(comBspGlob, comBspFileSize, h);
    FS_FCloseFile(h);

    if (bytesRead != comBspFileSize || (unsigned int)bytesRead <= 0x13f) {
        Z_FreeInternal(comBspGlob);
        Com_Error(1, va("EXE_ERR_COULDNT_LOAD%s", filename));
    }

    comBspChecksum = Com_BlockChecksum(comBspGlob, comBspFileSize);

    if (comBspGlob->ident != 0x50534249 || comBspGlob->version != 4) {
        Z_FreeInternal(comBspGlob);
        Com_Error(1, va("EXE_ERR_WRONG_MAP_VERSION_NUM%s", filename));
    }

    for (i = 0; i < 39; i++) {

    }
}

void Com_CleanupBsp(void)
{
    if (comBspGlob) {
        Z_FreeInternal(comBspGlob);
        comBspGlob = 0;
    }
}
