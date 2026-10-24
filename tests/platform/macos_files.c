#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "files_source.h"

fileHandleData_t fsh[74];
searchpath_t *fs_searchpaths;
char fs_gamedir[256] = "global";
static dvar_t ignoreLocalized;
const dvar_t *fs_ignoreLocalized = &ignoreLocalized;
static int language;
static int failures;
static int closed;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

FILE *FS_FileOpen(const char *path, const char *mode) { return fopen(path, mode); }
int FS_FileClose(FILE *file) { ++closed; return fclose(file); }
size_t FS_FileRead(void *p, size_t size, size_t count, FILE *file) { return fread(p, size, count, file); }
int FS_FileSeek(FILE *file, long offset, int origin) { return fseek(file, offset, origin); }
void Com_Memset(void *p, int value, int size) { memset(p, value, size); }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
int SEH_GetCurrentLanguage(void) { return language; }
const char *va(const char *fmt, ...)
{
    static char result[512];
    va_list ap;
    va_start(ap, fmt); vsnprintf(result, sizeof(result), fmt, ap); va_end(ap);
    return result;
}
#if defined(COD2_CODX) && COD2_CODX
/* Unrelated config redirection stays outside these non-config lookup cases. */
const char *Cod2x_ConfigGame(const char *game, const char *qpath) { (void)qpath; return game; }
#endif
void Sys_EndStreamedFile(fileHandle_t h) { (void)h; abort(); }
void Sys_StreamSeek(fileHandle_t h, int offset, int origin) { (void)h; (void)offset; (void)origin; abort(); }
static void check_archive(const char *path)
{
    unzFile archive = unzOpen(path);
    assert(archive);
    unsigned long position;
    CHECK(unzGetCurrentFileInfoPosition(archive, &position) == 0);
    CHECK(unzOpenCurrentFile(archive) == 0);
    pack_t pack = {0};
    fsh[1].handleFiles.file.z = archive;
    fsh[1].handleFiles.unique = 1;
    fsh[1].zipFile = &pack;
    fsh[1].zipFilePos = (int)position;
    CHECK(FS_filelength(1) == 9000);
    CHECK(FS_Seek(1, 0, 2) == 0 && FS_FTell(1) == 0);
    char bytes[40];
    CHECK(FS_Read(bytes, sizeof(bytes), 1) == 40);
    CHECK(!memcmp(bytes, "0123456789012345678901234567890123456789", 40));

    /* Mac FS_Seek routes these through a zero-length zip read: return -1. */
    CHECK(FS_Seek(1, 40, 2) == -1 && FS_FTell(1) == 40);
    CHECK(FS_Seek(1, -8960, 1) == -1 && FS_FTell(1) == 40);
    CHECK(FS_Seek(1, -40, 0) == -1 && FS_FTell(1) == 0);

    /* Keep the native scratch-buffer adaptation working across chunk sizes. */
    CHECK(FS_Seek(1, 30, 2) == 0 && FS_FTell(1) == 30);
    CHECK(FS_Seek(1, 4097, 0) == 0 && FS_FTell(1) == 4127);
    CHECK(FS_Seek(1, 0, 1) == 0 && FS_FTell(1) == 9000);
    CHECK(FS_Seek(1, 0, 1) == -1 && FS_FTell(1) == 9000);
    CHECK(FS_Seek(1, 9000, 2) == -1 && FS_FTell(1) == 9000);
    CHECK(FS_Seek(1, 1, 0) == -1 && FS_FTell(1) == 9000);

    /* These explicit zero-offset fast paths succeed in the Mac reference. */
    CHECK(FS_Seek(1, 0, 0) == 0 && FS_FTell(1) == 9000);
    CHECK(FS_Seek(1, 0, 2) == 0 && FS_FTell(1) == 0);
    FS_FCloseFile(1);
    fileHandleData_t empty = {0};
    CHECK(!memcmp(&fsh[1], &empty, sizeof(empty)));
}

int main(void)
{
    char temporary[] = "files-XXXXXX";
    CHECK(mkdtemp(temporary));
    char directory[256], filename[256];
    snprintf(directory, sizeof(directory), "%s/mod", temporary);
    CHECK(mkdir(directory, 0700) == 0);
    snprintf(filename, sizeof(filename), "%s/asset.bin", directory);
    FILE *file = fopen(filename, "wb");
    assert(file);
    CHECK(fwrite("abcdef", 1, 6, file) == 6);
    CHECK(fclose(file) == 0);

    directory_t dir = {0};
    strcpy(dir.path, temporary);
    strcpy(dir.gamedir, "mod");
    searchpath_t search = {0};
    search.dir = &dir;
    fs_searchpaths = &search;
    const char *shortPath = FS_ShortOSFilePath("asset.bin");
    CHECK(shortPath && strcmp(shortPath, "mod/asset.bin") == 0);
    CHECK(closed == 1);
    CHECK(FS_ShortOSFilePath("missing.bin") == NULL);

    search.bLocalized = 1;
    search.language = 2;
    language = 1;
    CHECK(FS_ShortOSFilePath("asset.bin") == NULL);
    language = 2;
    ignoreLocalized.current.enabled = 1;
    CHECK(FS_ShortOSFilePath("asset.bin") == NULL);
    ignoreLocalized.current.enabled = 0;
    shortPath = FS_ShortOSFilePath("asset.bin");
    CHECK(shortPath && strcmp(shortPath, "mod/asset.bin") == 0);
    CHECK(closed == 2);

    fsh[1].handleFiles.file.o = fopen(filename, "rb");
    assert(fsh[1].handleFiles.file.o);
    CHECK(FS_filelength(1) == 6);
    CHECK(FS_FTell(1) == 0);
    char bytes[8] = {0};
    CHECK(FS_Read(bytes, 2, 1) == 2 && !memcmp(bytes, "ab", 2));
    CHECK(FS_Seek(1, 1, 0) == 0 && FS_FTell(1) == 3);
    CHECK(FS_Read(bytes, 8, 1) == 3 && !memcmp(bytes, "def", 3));
    CHECK(FS_Seek(1, -2, 1) == 0 && FS_FTell(1) == 4);
    CHECK(FS_Seek(1, 1, 2) == 0 && FS_FTell(1) == 1);
    CHECK(FS_Read(bytes, 0, 1) == 0);
    CHECK(FS_Read(bytes, 2, 0) == 0);
    FS_FCloseFile(1);
    fileHandleData_t empty = {0};
    CHECK(!memcmp(&fsh[1], &empty, sizeof(empty)));
    CHECK(unlink(filename) == 0);
    CHECK(rmdir(directory) == 0);
    CHECK(rmdir(temporary) == 0);
    check_archive("seek-stored.iwd");
    check_archive("seek-deflated.iwd");
    fprintf(stderr, "filesystem checks: %d failures\n", failures);
    return failures != 0;
}
