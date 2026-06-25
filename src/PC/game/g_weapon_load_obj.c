#include "common_types.h"
#include "imports.h"
#include <stdlib.h>

extern int FS_FOpenFileByMode(const char *filename, int *f, int mode);
extern int FS_Read(void *buffer, int len, int f);
extern void FS_FCloseFile(fileHandle_t f);
extern void Com_BeginParseSession(const char *name);
extern void Com_EndParseSession(void);
extern const char *Com_Parse(const char **buf);
extern void Com_Printf(const char *fmt, ...);
extern void *Hunk_AllocLowAlignInternal(int size, int align);

__attribute__((used, packed, aligned(4)))
const char *accuracyDirName[] = {
    "aivsai",
    "aivsplayer",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static Bool G_ParseWeaponAccuracyGraph(const char *dirName, const char *graphName,
                                       float accuracyGraphKnots[16][2],
                                       float **outKnots, int *outKnotCount)
{
    char fileName[64];
    char buffer[8192];
    int fileHandle;
    int fileLength;

    int weapType = 0;
    if (weapType != 0 && weapType != 2)
        return 1;

    if (*graphName == '\0')
        return 1;

    sprintf(fileName, "accuracy/%s/%s", dirName, graphName);

    fileLength = FS_FOpenFileByMode(fileName, &fileHandle, 0);
    if (fileLength < 0) {
        Com_Printf("^3WARNING: Could not load ai weapon accuracy file '%s'\n", fileName);
        return 0;
    }

    char header[14];
    FS_Read(header, 14, fileHandle);
    header[14] = '\0';

    if (memcmp(header, "WEAPONACCURACY", 14) != 0) {
        Com_Printf("^3WARNING: \"%s\" does not appear to be an ai weapon accuracy file\n", fileName);
        FS_FCloseFile(fileHandle);
        return 0;
    }

    fileLength -= 14;
    if (fileLength > 0x1fff) {
        Com_Printf("^3WARNING: \"%s\" Is too long of an ai weapon accuracy file to read\n", fileName);
        FS_FCloseFile(fileHandle);
        return 0;
    }

    memset(buffer, 0, 0x2000);
    FS_Read(buffer, fileLength, fileHandle);
    buffer[fileLength] = '\0';
    FS_FCloseFile(fileHandle);

    const char *bufPtr = buffer;
    Com_BeginParseSession(fileName);

    const char *token = Com_Parse(&bufPtr);
    int knotCountInternal = atoi(token);

    int knotCountIndex;
    for (knotCountIndex = 0; knotCountIndex < 16; knotCountIndex++) {
        token = Com_Parse(&bufPtr);
        if (*token == '\0' || *token == '}')
            break;

        accuracyGraphKnots[knotCountIndex][0] = (float)atof(token);

        token = Com_Parse(&bufPtr);
        if (*token == '\0' || *token == '}')
            break;

        accuracyGraphKnots[knotCountIndex][1] = (float)atof(token);
    }

    if (knotCountIndex >= 16) {
        Com_Printf("^3WARNING: \"%s\" has too many graph knots\n", fileName);
        Com_EndParseSession();
        return 0;
    }

    Com_EndParseSession();

    if (knotCountInternal != knotCountIndex) {
        Com_Printf("^3WARNING: \"%s\" Error in parsing an ai weapon accuracy file\n", fileName);
        return 0;
    }

    if (knotCountIndex <= 0)
        return 1;

    int size = knotCountIndex * 8;
    *outKnots = (float *)Hunk_AllocLowAlignInternal(size, 4);
    memcpy(*outKnots, accuracyGraphKnots, size);
    *outKnotCount = knotCountIndex;

    return 1;
}

Bool G_ParseWeaponAccurayGraphs(WeaponDef *weaponDef)
{
    float accuracyGraphKnots[16][2];
    int dirIdx;

    WeaponDef *wdef = (WeaponDef *)weaponDef;

    for (dirIdx = 0; dirIdx < 2; dirIdx++) {
        memset(accuracyGraphKnots, 0, sizeof(accuracyGraphKnots));

        const char *dirName = accuracyDirName[dirIdx];
        const char *graphName = wdef->accuracyGraphName[dirIdx];

        if (wdef->weapType != 0 && wdef->weapType != 2)
            continue;

        if (!graphName || *graphName == '\0')
            continue;

        char fileName[64];
        sprintf(fileName, "accuracy/%s/%s", dirName, graphName);

        int fileHandle;
        int fileLength = FS_FOpenFileByMode(fileName, &fileHandle, 0);
        if (fileLength < 0) {
            Com_Printf("^3WARNING: Could not load ai weapon accuracy file '%s'\n", fileName);
            return 0;
        }

        char buffer[8192];
        FS_Read(buffer, 14, fileHandle);
        buffer[14] = '\0';

        if (memcmp(buffer, "WEAPONACCUFILE", 14) != 0) {
            Com_Printf("^3WARNING: \"%s\" does not appear to be an ai weapon accuracy file\n", fileName);
            FS_FCloseFile(fileHandle);
            return 0;
        }

        fileLength -= 14;
        if (fileLength > 0x1fff) {
            Com_Printf("^3WARNING: \"%s\" Is too long of an ai weapon accuracy file to read\n", fileName);
            FS_FCloseFile(fileHandle);
            return 0;
        }

        memset(buffer, 0, 0x2000);
        FS_Read(buffer, fileLength, fileHandle);
        buffer[fileLength] = '\0';
        FS_FCloseFile(fileHandle);

        const char *bufPtr = buffer;
        Com_BeginParseSession(fileName);

        const char *token = Com_Parse(&bufPtr);
        int knotCountInternal = atoi(token);

        int knotCountIndex;
        for (knotCountIndex = 0; knotCountIndex < 16; knotCountIndex++) {
            token = Com_Parse(&bufPtr);
            if (*token == '\0' || *token == '}')
                break;

            accuracyGraphKnots[knotCountIndex][0] = (float)atof(token);

            token = Com_Parse(&bufPtr);
            if (*token == '\0' || *token == '}')
                break;

            accuracyGraphKnots[knotCountIndex][1] = (float)atof(token);
        }

        if (knotCountIndex >= 16) {
            Com_Printf("^3WARNING: \"%s\" has too many graph knots\n", fileName);
            Com_EndParseSession();
            return 0;
        }

        Com_EndParseSession();

        if (knotCountInternal != knotCountIndex) {
            Com_Printf("^3WARNING: \"%s\" Error in parsing an ai weapon accuracy file\n", fileName);
            return 0;
        }

        if (knotCountIndex <= 0)
            continue;

        int size = knotCountIndex * 8;
        float *knots = (float *)Hunk_AllocLowAlignInternal(size, 4);
        wdef->accuracyGraphKnots[dirIdx] = (vec2_t *)knots;
        memcpy(knots, accuracyGraphKnots, size);
        wdef->accuracyGraphKnotCount[dirIdx] = knotCountIndex;
    }

    return 1;
}
