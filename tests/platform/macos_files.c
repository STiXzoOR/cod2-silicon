#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>
#include "PC/qcommon/com_fileaccess.h"

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

extern int FS_RealFileClose(FILE *file);
int FS_FileClose(FILE *file) { ++closed; return FS_RealFileClose(file); }
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


static dvar_t home, zero;
const dvar_t *fs_homepath = &home, *fs_debug = &zero, *fs_restrict = &zero;
const dvar_t *fs_copyfiles = &zero, *fs_cdpath = &zero, *fs_basepath = &home;
int com_fileAccessed, fs_fakeChkSum, fs_loadStack, fs_checksumFeed, fs_serverIwds[1024];
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
static LegacyHacks downloadUi;
static LegacyHacks *downloadUiPointer = &downloadUi;
void *imp_legacyHacks = &downloadUiPointer;
extern int DL_BeginDownload(const char *local, const char *remote, int debug);
extern void DL_CancelDownload(void);
extern int Sys_RemoveDirTree(const char *path);
void *Z_MallocInternal(int size) { return calloc(1, size); }
void Z_FreeInternal(void *ptr) { free(ptr); }
char *CopyStringInternal(const char *s) { return strdup(s); }
void *Hunk_AllocateTempMemoryInternal(int size) { return malloc(size); }
void Hunk_FreeTempMemory(void *p) { free(p); }
Bool I_islower(int c) { return c >= 'a' && c <= 'z'; }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int I_strcmp(const char *a, const char *b) { return strcmp(a, b); }
int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int I_strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }
void I_strncpyz(char *d, const char *s, int n) { snprintf(d, n, "%s", s); }
int Com_sprintf(char *d, int n, const char *f, ...)
{
    va_list a; va_start(a, f); int r = vsnprintf(d, n, f, a); va_end(a); return r;
}
const char *Com_GetExtensionSubString(const char *s)
{
    const char *p = strrchr(s, '.'); return p ? p : s + strlen(s);
}
extern void FS_SV_Rename(const char *from, const char *to);
extern int Sys_DirectoryHasContents(const char *directory);

static void expect_os_file(const char *path, const char *expected)
{
    char bytes[64] = {0};
    FILE *file = FS_FileOpen(path, "rb");
    CHECK(file != NULL);
    if (!file) return;
    size_t count = fread(bytes, 1, sizeof(bytes) - 1, file);
    CHECK(count == strlen(expected) && !strcmp(bytes, expected));
    FS_FileClose(file);
}

static void expect_game_file(const char *path, const char *expected)
{
    char bytes[64] = {0};
    fileHandle_t handle = 0;
    int size = FS_FOpenFileRead(path, &handle, 1);
    CHECK(handle && size == (int)strlen(expected));
    if (handle) {
        CHECK(FS_Read(bytes, sizeof(bytes) - 1, handle) == size);
        CHECK(!strcmp(bytes, expected));
        FS_FCloseFile(handle);
    }
}

static int listed(char **names, int count, const char *name)
{
    int matches = 0;
    for (int i = 0; i < count; ++i) if (!strcasecmp(names[i], name)) ++matches;
    return matches;
}

static void check_case_paths(void)
{
    int before = failures;
    directory_t directory = {0};
    searchpath_t loose = {0};
    strcpy(directory.path, "mixedroot");
    strcpy(directory.gamedir, "mod");
    loose.dir = &directory;
    fs_searchpaths = &loose;
    home.current.string = "mixedroot";
    strcpy(fs_gamedir, "mOd");
    expect_game_file("subdir/case.CFG", "ORIGINAL");
    expect_game_file("SuBdIr\\CaSe.cfg", "ORIGINAL");
    expect_os_file("MIXEDROOT/MOD/SUBDIR/CASE.CFG", "ORIGINAL");
    CHECK(FS_FileExists("SUBDIR/CASE.CFG"));
    char osPath[256];
    CHECK(FS_GetFileOsPath("SUBDIR/case.CFG", osPath) == 0);
    FILE *rawFile = fopen(osPath, "rb");
    CHECK(rawFile != NULL);
    if (rawFile) fclose(rawFile);
    CHECK(FS_WriteFile("subdir/CASE.cfg", "WRITE", 5));
    fileHandle_t handle = FS_FOpenFileAppend("SUBDIR/case.CFG");
    CHECK(handle);
    if (handle) { CHECK(FS_Write("+", 1, handle) == 1); FS_FCloseFile(handle); }
    expect_game_file("sUbDiR/cAsE.CfG", "WRITE+");
    CHECK(FS_WriteFile("subdir/NewDir/Fresh.cfg", "NEW", 3));
    expect_os_file("mixedroot/MOD/SUBDIR/newdir/fRESH.CFG", "NEW");
    handle = FS_FOpenTextFileWrite("SUBDIR/newdir/FRESH.CFG");
    CHECK(handle);
    if (handle) { CHECK(FS_Write("TEXT", 4, handle) == 4); FS_FCloseFile(handle); }
    expect_game_file("subdir/NEWDIR/fresh.cfg", "TEXT");
    FS_SV_Rename("mod/subdir/newdir/fresh.cfg", "MOD/SUBDIR/renamed.CFG");
    expect_game_file("subdir/RENAMED.cfg", "TEXT");
    CHECK(!FS_FileExists("subdir/NewDir/fresh.cfg"));
    FS_SV_Rename("mod/subdir/renamed.cfg", "mod/subdir/cASE.cfg");
    expect_game_file("subdir/case.cfg", "TEXT");
    CHECK(!FS_FileExists("subdir/renamed.cfg"));
    FS_SV_Rename("mod/subdir/case.cfg", "MOD/SUBDIR/CASE.CFG");
    expect_game_file("subdir/case.cfg", "TEXT");
    int caseCount;
    char **caseNames = Sys_ListFiles("mixedroot/mod/subdir", "cfg", NULL, &caseCount, 0);
    int exactCase = 0;
    for (int i = 0; i < caseCount; ++i) if (!strcmp(caseNames[i], "CASE.CFG")) ++exactCase;
    CHECK(exactCase == 1);
    Sys_FreeFileList(caseNames);
    CHECK(FS_WriteFile("subdir/delete.cfg", "D", 1));
    FS_Remove("MIXEDROOT/mod/SUBDIR/DELETE.CFG");
    CHECK(!FS_FileExists("subdir/Delete.cfg"));
    expect_game_file("link/CASE.cfg", "TEXT");
    CHECK(FS_WriteFile("SubDir/RemoveTree/Nested/File.cfg", "REMOVE", 6));
    CHECK(Sys_RemoveDirTree("MIXEDROOT/mod/SUBDIR/removetree"));
    CHECK(!FS_FileExists("SubDir/RemoveTree/Nested/File.cfg"));
    CHECK(FS_WriteFile("SubDir/Download.tmp", "OLD", 3));
    char downloadPath[] = "MIXEDROOT/mod/SUBDIR/download.TMP";
    CHECK(DL_BeginDownload(downloadPath, "http://127.0.0.1:1/fixture", 0));
    DL_CancelDownload(); /* No download loop is run, so no network request is sent. */
    expect_os_file("mixedroot/MOD/subdir/DOWNLOAD.TMP", "");
    CHECK(Sys_DirectoryHasContents("MIXEDROOT/mOd/SUBDIR"));

    int count;
    char **names = Sys_ListFiles("mixedroot/MOD", "cfg", NULL, &count, 0);
    CHECK(listed(names, count, "Twin.cfg") == 1);
    Sys_FreeFileList(names);
    names = Sys_ListFiles("MIXEDROOT/mod", "/", NULL, &count, 1);
    CHECK(listed(names, count, "Fork") == 1 && listed(names, count, "SubDir") == 1);
    Sys_FreeFileList(names);
    names = Sys_ListFiles("mixedroot/MOD", "", "*", &count, 0);
    CHECK(count > 5 && count < 40);
    CHECK(listed(names, count, "SubDir/Case.cfg") == 1);
    Sys_FreeFileList(names);
    expect_game_file("twin.cfg", "UPPER");
    expect_game_file("TWIN.CFG", "UPPER");
    expect_game_file("fork/item.cfg", "UPPER-DIR");
    CHECK(FS_WriteFile("twin.cfg", "CANONICAL", 9));
    expect_os_file("MixedRoot/MoD/Twin.cfg", "CANONICAL");
    FS_SV_Rename("mod/twin.cfg", "mod/tWiN.CfG");
    expect_game_file("TWIN.CFG", "CANONICAL");
    expect_os_file("mixedroot/mod/cafÉ.cfg", "UNICODE");
    expect_os_file("mixedroot/mod/Café.cfg", "UNICODE");
    expect_os_file("mixedroot/mod/STRAßE.CFG", pathconf(".", _PC_CASE_SENSITIVE) ? "SHARP-S" : "DOUBLE-S");
    expect_os_file("mixedroot/mod/STRASSE.CFG", "DOUBLE-S");
    /* APFS limits UTF-16 characters, so a legal UTF-8 component can exceed
     * NAME_MAX bytes. The native boundary must not truncate such names. */
    char unicodePath[512] = "mixedroot/mod/";
    size_t unicodeLength = strlen(unicodePath);
    for (int i = 0; i < 130; ++i) {
        unicodePath[unicodeLength++] = (char)0xc3;
        unicodePath[unicodeLength++] = (char)0x89;
    }
    strcpy(unicodePath + unicodeLength, ".CFG");
    expect_os_file(unicodePath, "LONG-UNICODE");

    /* Real IWD open and unique-handle reopen pass through the same resolver. */
    pack_t pack = {0};
    fileInPack_t entry = {0};
    fileInPack_t *hash[128] = {0};
    searchpath_t packed = {0};
    strcpy(pack.iwdFilename, "MIXEDROOT/mod/PACKS/case.iwd");
    pack.handle = unzOpen(pack.iwdFilename);
    CHECK(pack.handle);
    if (pack.handle) {
        pack.hashSize = 128; pack.hashTable = hash;
        entry.name = "subdir/frompack.cfg";
        CHECK(unzGetCurrentFileInfoPosition(pack.handle, &entry.pos) == 0);
        hash[FS_HashFileName(entry.name, 128)] = &entry;
        packed.pack = &pack; packed.next = (intptr_t)&loose;
        fs_searchpaths = &packed;
        expect_game_file("SUBDIR/FROMPACK.CFG", "ARCHIVE");
        unzClose(pack.handle);
    }
    names = Sys_ListFiles("mixedROOT/mod/pACKS", "iwd", NULL, &count, 0);
    CHECK(count == 1 && listed(names, count, "Case.IWD") == 1);
    Sys_FreeFileList(names);
    fs_searchpaths = &loose;
    fs_numServerIwds = 1;
    handle = 0;
    CHECK(FS_FOpenFileRead("subdir/blocked.STR", &handle, 0) < 0 && !handle);
    expect_game_file("subdir/case.cfg", "TEXT");
    loose.bLocalized = 1; loose.language = language;
    expect_game_file("subdir/blocked.STR", "PURE");
    fs_numServerIwds = 0;
    fs_searchpaths = NULL;
    fprintf(stderr, "case path checks: %d failures\n", failures - before);
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
    check_case_paths();
    fprintf(stderr, "filesystem checks: %d failures\n", failures);
    return failures != 0;
}
