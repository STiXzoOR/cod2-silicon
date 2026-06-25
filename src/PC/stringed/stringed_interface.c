#include "common_types.h"
#include "imports.h"
#include <string.h>

extern int giFilesFound;

extern int FS_ReadFile(const char *qpath, void **buffer);
extern void FS_FreeFile(void *buffer);
extern const char **FS_ListFiles(const char *path, const char *extension, int behavior, int *numfiles, int allocTrackType);
extern void FS_FreeFileList(const char **list, int allocTrackType);

extern void *__ZNSs6appendEPKcm(void *_this, const char *s, unsigned int n);

extern void __ZNSs7reserveEm(void *_this, unsigned int n);

extern void *__ZNSs6assignEPKcm(void *_this, const char *s, unsigned int n);

extern char __ZNSs4_Rep11_S_terminalE;

unsigned char *SE_LoadFileData(const char *psFileName);
void SE_FreeFileDataAfterLoad(unsigned char *psLoadedFile);
static void SE_R_ListFiles(const char *psExtension, const char *psDir, LocalizeString *strResults) __attribute_regparm__(3);
int SE_BuildFileList(const char *psStartDir, LocalizeString *strResults);

unsigned char *SE_LoadFileData(const char *psFileName)
{
    void *pvLoadedData;

    if (FS_ReadFile(psFileName, &pvLoadedData) > 0) {
        return (unsigned char *)pvLoadedData;
    }
    return (unsigned char *)0;
}

void SE_FreeFileDataAfterLoad(unsigned char *psLoadedFile)
{
    FS_FreeFile(psLoadedFile);
}

static void __attribute_regparm__(3)
    SE_R_ListFiles(const char *psExtension, const char *psDir, LocalizeString *strResults)
{
    char sDirName[64];
    int numdirs;
    int numSysFiles;
    const char **dirFiles;
    const char **sysFiles;
    int i;
    int j;

    dirFiles = FS_ListFiles(psDir, "/", 0, &numdirs, 10);
    for (i = 0; i < numdirs; i++) {
        const char *entry = dirFiles[i];
        if (entry[0] == '\0' || entry[0] == '.') {
            continue;
        }
        sprintf(sDirName, "%s/%s", psDir, entry);
        SE_R_ListFiles(psExtension, sDirName, strResults);
    }

    sysFiles = FS_ListFiles(psDir, psExtension, 0, &numSysFiles, 10);
    for (j = 0; j < numSysFiles; j++) {
        char *data;
        unsigned int size;
        unsigned int new_size;

        sprintf(sDirName, "%s/%s", psDir, sysFiles[j]);

        __ZNSs6appendEPKcm(strResults, sDirName, strlen(sDirName));

        data = *(char **)strResults;
        size = *(unsigned int *)(data - 12);
        new_size = size + 1;

        if (new_size > *(unsigned int *)(data - 8) ||
            *(int *)(data - 4) > 0) {
            __ZNSs7reserveEm(strResults, new_size);
            data = *(char **)strResults;
        }

        data[size] = ';';
        *(char **)strResults = data;
        data = *(char **)strResults;
        *(int *)(data - 4) = 0;
        *(unsigned int *)(data - 12) = new_size;
        data[new_size] = __ZNSs4_Rep11_S_terminalE;

        giFilesFound++;
    }

    FS_FreeFileList(sysFiles, 10);
    FS_FreeFileList(dirFiles, 10);
}

int SE_BuildFileList(const char *psStartDir, LocalizeString *strResults)
{
    giFilesFound = 0;
    __ZNSs6assignEPKcm(strResults, "", 0);
    SE_R_ListFiles("str", psStartDir, strResults);
    return giFilesFound;
}
