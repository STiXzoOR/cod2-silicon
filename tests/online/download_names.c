#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int count = 2, checksums[2] = {123, 456};
static char *names[2] = {"main/zpam_maps_v7", "mod/map"};
static searchpath_t *searchpaths;
void *imp_fs_numServerReferencedIwds = &count;
void *imp_fs_serverReferencedIwds = checksums;
void *imp_fs_serverReferencedIwdNames = names;
void *imp_fs_searchpaths = &searchpaths;
static dvar_t home = { .current.string = "/private" };
const dvar_t *fs_homepath = &home;
static int collision;
qboolean FS_iwIwd(char *name, char *base) { (void)name; (void)base; return 0; }
void I_strncat(char *dest, int size, const char *src)
{ assert(strlen(dest) + strlen(src) < (unsigned int)size); strcat(dest, src); }
char *va(const char *format, ...)
{
    static char text[512];
    va_list args; va_start(args, format); vsnprintf(text, sizeof(text), format, args); va_end(args);
    return text;
}
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *out)
{ snprintf(out, 256, "%s/%s/%s", base, game, qpath); }
FILE *FS_FileOpen(const char *path, const char *mode)
{ (void)path; (void)mode; return collision ? (FILE *)(uintptr_t)1 : NULL; }
int FS_FileClose(FILE *f) { (void)f; return 0; }
int Com_sprintf(char *dest, int size, const char *format, ...)
{
    va_list args; va_start(args, format); int n = vsnprintf(dest, size, format, args); va_end(args);
    return n;
}
void Com_Printf(const char *format, ...) { (void)format; }
#include "download_names_source.h"
int main(void)
{
    char list[1024];
    assert(FS_CompareIwds(list, sizeof(list), 1));
    assert(!strcmp(list, "@main/zpam_maps_v7.iwd@main/zpam_maps_v7.iwd@mod/map.iwd@mod/map.iwd"));
    collision = 1;
    assert(FS_CompareIwds(list, sizeof(list), 1));
    assert(!strcmp(list, "@main/zpam_maps_v7.iwd@main/zpam_maps_v7.0000007b.iwd@mod/map.iwd@mod/map.000001c8.iwd"));
    puts("online: downloadable IWD names, delimiters and checksum collisions passed");
    return 0;
}
