#if defined(COD2_X64)
#include "PC/qcommon/port_debug.h"
#endif
#include "common_types.h"
#include "imports.h"
#include "PC/qcommon/com_fileaccess.h"
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#if COD2_APPLE_SDK
#include "platform/macos_system.h"
#endif

static char cwd[256];

extern int Com_sprintf(char *dest, int size, const char *fmt, ...);
extern qboolean Com_FilterPath(const char *filter, const char *name, int casesensitive);
extern char *CopyStringInternal(const char *in);
extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);
extern int I_stricmp(const char *s1, const char *s2);
extern int FS_Read(void *buffer, int len, fileHandle_t f);
extern int FS_Seek(fileHandle_t f, long int offset, int origin);
#if defined(__APPLE__) && defined(COD2_X64)
extern int MacFiles_RemoveDirectoryA(const char *inPath);
#else
extern OSStatus MacFiles_RemoveDirectoryA(const char *inPath);
#endif

void Sys_Mkdir(const char *path)
{
#if COD2_APPLE_SDK && defined(COD2_X64)
    char resolved[PATH_MAX];
    if (FS_ResolveOSPath(path, resolved, sizeof(resolved))) return;
    path = resolved;
#endif
    /* mode 7 (R|W|X_OK) is invalid for Windows _access (X_OK unsupported) -> CRT
       invalid-parameter -> __fastfail/abrupt process exit. We only need an existence check. */
#if defined(COD2_X64) && !defined(_WIN32)
    if (access(path, R_OK | W_OK | X_OK) != 0) {
#else
    if (access(path, 0) != 0) {
#endif
        mkdir(path, 0777);
    }
}

qboolean Sys_RemoveDirTree(const char *path)
{
#if COD2_APPLE_SDK && defined(COD2_X64)
    char resolved[PATH_MAX];
    if (FS_ResolveOSPath(path, resolved, sizeof(resolved))) return 0;
    path = resolved;
#endif
    return MacFiles_RemoveDirectoryA(path) == 0;
}

char *Sys_DefaultCDPath(void)
{
    return (char *)"";
}

char *Sys_DefaultHomePath(void)
{
#if COD2_APPLE_SDK
    return MacSystem_HomePath();
#else
    return NULL;
#endif
}

char *Sys_DefaultInstallPath(void)
{
    getcwd(cwd, 255);
    cwd[255] = '\0';
    return cwd;
}

#if COD2_APPLE_SDK && defined(COD2_X64)
typedef struct sysDirectoryAncestor_s {
    dev_t device;
    ino_t inode;
    const struct sysDirectoryAncestor_s *parent;
} sysDirectoryAncestor_t;

static void Sys_ListFilteredFilesNative(const char *basedir, const char *subdirs,
                                       const char *filter, char **list, int *numfiles,
                                       const sysDirectoryAncestor_t *parent)
{
    char search[PATH_MAX], requested[256], filename[256], child[256];
    struct stat st;
    char **names;
    int count;
    if (*numfiles >= 4095) return;
    if (snprintf(requested, sizeof(requested), "%s%s%s", basedir,
                 subdirs[0] ? "/" : "", subdirs) >= sizeof(requested)) return;
    if (FS_ResolveOSPath(requested, search, sizeof(search)) || stat(search, &st)) return;
    /* Directory symlinks are usable, but a link to an ancestor must not recurse
     * forever or fill the result with aliases of the same subtree. */
    for (const sysDirectoryAncestor_t *p = parent; p; p = p->parent)
        if (p->device == st.st_dev && p->inode == st.st_ino) return;
    sysDirectoryAncestor_t current = { st.st_dev, st.st_ino, parent };
    names = FS_ListOSDirectory(search, &count);
    for (int i = 0; i < count && *numfiles < 4095; ++i) {
        if (snprintf(filename, sizeof(filename), "%s/%s", search, names[i]) >= sizeof(filename)) continue;
        if (stat(filename, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (snprintf(child, sizeof(child), "%s%s%s", subdirs,
                         subdirs[0] ? "/" : "", names[i]) >= sizeof(child)) continue;
            Sys_ListFilteredFilesNative(basedir, child, filter, list, numfiles, &current);
        }
        if (*numfiles >= 4095) break;
        if (snprintf(filename, sizeof(filename), "%s/%s", subdirs, names[i]) >= sizeof(filename)) continue;
        if (Com_FilterPath(filter, filename, 0)) list[(*numfiles)++] = CopyStringInternal(filename);
    }
    FS_FreeOSDirectory(names, count);
}
#endif

void Sys_ListFilteredFiles(const char *basedir, const char *subdirs, const char *filter, char **list, int *numfiles)
{
#if COD2_APPLE_SDK && defined(COD2_X64)
    Sys_ListFilteredFilesNative(basedir, subdirs, filter, list, numfiles, NULL);
#else
    char search[256];
    char filename[256];
    char newsubdirs[256];
    struct stat st;
    DIR *fdir;
    struct dirent *d;

#if defined(COD2_X64)
    if (*numfiles > 0xFFE)
#else
    if (*numfiles >= 0xFFE)
#endif
        return;

    if (subdirs[0]) {
        Com_sprintf(search, 256, "%s/%s", basedir, subdirs);
    } else {
        Com_sprintf(search, 256, "%s", basedir);
    }

    fdir = opendir(search);
    if (!fdir)
        return;

    while ((d = readdir(fdir)) != NULL) {
        Com_sprintf(filename, 256, "%s/%s", search, d->d_name);

        if (stat(filename, &st) == -1)
            continue;

#if defined(COD2_X64)
        if (st.st_mode & S_IFDIR) {
#else
        if (S_ISDIR(st.st_mode)) {
#endif
            if (I_stricmp(d->d_name, ".") == 0 || I_stricmp(d->d_name, "..") == 0)
                goto check_file;

            if (subdirs[0]) {
                Com_sprintf(newsubdirs, 256, "%s/%s", subdirs, d->d_name);
            } else {
                Com_sprintf(newsubdirs, 256, "%s", d->d_name);
            }
            Sys_ListFilteredFiles(basedir, newsubdirs, filter, list, numfiles);
        }

    check_file:
#if defined(COD2_X64)
        if (*numfiles > 0xFFE)
#else
        if (*numfiles >= 0xFFE)
#endif
            break;

        Com_sprintf(filename, 256, "%s/%s", subdirs, d->d_name);

        if (!Com_FilterPath(filter, filename, 0))
            continue;

        list[*numfiles] = CopyStringInternal(filename);
        (*numfiles)++;
    }

    closedir(fdir);
#endif
}

char **Sys_ListFiles(const char *directory, const char *extension, const char *filter, int *numfiles, qboolean wantsubs)
{
    char *list[4096];
    char search[256];
    struct stat st;
    DIR *fdir;
#if COD2_APPLE_SDK && defined(COD2_X64)
    char resolved[PATH_MAX];
    char **names;
    int count;
#else
    struct dirent *d;
#endif
    int nfiles;
    int dironly;
    char **listCopy;
    int i;
    int extLen, nameLen;

    if (filter) {
        nfiles = 0;
        Sys_ListFilteredFiles(directory, "", filter, list, &nfiles);
        list[nfiles] = NULL;
        *numfiles = nfiles;

        if (nfiles == 0)
            return NULL;

#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64) || COD2_WINDOWS_NATIVE
        listCopy = (char **)Z_MallocInternal((nfiles + 1) * sizeof(char *));
#else
        listCopy = (char **)Z_MallocInternal((nfiles + 1) * 4);
#endif
        for (i = 0; i < nfiles; i++) {
            listCopy[i] = list[i];
        }
        listCopy[nfiles] = NULL;
        return listCopy;
    }

    if (!extension)
        extension = "";

    if (extension[0] == '/' && extension[1] == '\0') {
        dironly = 1;
        extension = "";
    } else {
        dironly = wantsubs;
    }

    nfiles = 0;

#if COD2_APPLE_SDK && defined(COD2_X64)
    if (FS_ResolveOSPath(directory, resolved, sizeof(resolved))) {
        *numfiles = 0;
        return NULL;
    }
    directory = resolved;
    names = FS_ListOSDirectory(directory, &count);
    for (int entry = 0; entry < count; ++entry) {
        const char *name = names[entry];
#else
    fdir = opendir(directory);
#ifdef W32_FSTRACE
#if defined(COD2_X64)
    COD2_NATIVE_TRACE_ONLY(
#endif
    fprintf(stderr, "[w32-ls] dir='%s' ext='%s' opendir=%p\n", directory, extension, (void *)fdir);
#if defined(COD2_X64)
    )
#endif
    fflush(stderr);
#endif
    if (!fdir) {
        *numfiles = 0;
        return NULL;
    }

    while ((d = readdir(fdir)) != NULL) {
        const char *name = d->d_name;
#if defined(COD2_X64)
        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
#else
        if (d->d_name[0] == '.')
#endif
            continue;

#endif
#if COD2_APPLE_SDK && defined(COD2_X64)
        if (snprintf(search, sizeof(search), "%s/%s", directory, name) >= sizeof(search)) continue;
#else
        Com_sprintf(search, 256, "%s/%s", directory, name);
#endif

        if (stat(search, &st) == -1)
            continue;

        if (dironly) {
#if defined(COD2_X64)
            if (!(st.st_mode & S_IFDIR))
#else
            if (!S_ISDIR(st.st_mode))
#endif
                continue;
        } else {
#if defined(COD2_X64)
            if (st.st_mode & S_IFDIR)
#else
            if (S_ISDIR(st.st_mode))
#endif
                continue;
        }

        if (extension[0]) {
            nameLen = strlen(name);
            extLen = strlen(extension);
#if defined(COD2_X64)
            /* Windows Sys_ListFiles searches *.<extension>. A suffix alone
             * is not an extension, and dot-prefixed archives are ordinary files. */
            if (extension[0] != '.' && (nameLen <= extLen || name[nameLen - extLen - 1] != '.'))
                continue;
#endif
            if (nameLen < extLen)
                continue;
            if (I_stricmp(name + nameLen - extLen, extension) != 0)
                continue;
        }

        if (nfiles == 0xFFF)
            break;

        list[nfiles] = CopyStringInternal(name);
        nfiles++;
    }

#if COD2_APPLE_SDK && defined(COD2_X64)
    FS_FreeOSDirectory(names, count);
#else
    closedir(fdir);
#endif

    *numfiles = nfiles;

    if (nfiles == 0)
        return NULL;

#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64) || COD2_WINDOWS_NATIVE
    listCopy = (char **)Z_MallocInternal((nfiles + 1) * sizeof(char *));
#else
    listCopy = (char **)Z_MallocInternal((nfiles + 1) * 4);
#endif
    for (i = 0; i < nfiles; i++) {
        listCopy[i] = list[i];
    }
    listCopy[nfiles] = NULL;
    return listCopy;
}

void Sys_FreeFileList(char **list)
{
    int i;

    if (!list)
        return;

    for (i = 0; list[i]; i++) {
        Z_FreeInternal(list[i]);
    }
    Z_FreeInternal(list);
}

qboolean Sys_DirectoryHasContents(const char *directory)
{
#if COD2_APPLE_SDK && defined(COD2_X64)
    char resolved[PATH_MAX], filename[PATH_MAX];
    char **names;
    int count, found = 0;
    struct stat st;
    if (FS_ResolveOSPath(directory, resolved, sizeof(resolved))) return 0;
    names = FS_ListOSDirectory(resolved, &count);
    for (int i = 0; i < count; ++i) {
        if (names[i][0] == '.') continue;
        if (snprintf(filename, sizeof(filename), "%s/%s", resolved, names[i]) >= sizeof(filename)) continue;
        if (!stat(filename, &st) && S_ISREG(st.st_mode)) { found = 1; break; }
    }
    FS_FreeOSDirectory(names, count);
    return found;
#else
    DIR *dir;
    struct dirent *d;

    dir = opendir(directory);
    if (!dir)
        return 0;

    while ((d = readdir(dir)) != NULL) {
        if (d->d_name[0] == '.')
            continue;
#if defined(COD2_X64) && !defined(_WIN32)
        if (d->d_type != DT_REG)
            continue;
#endif
        closedir(dir);
        return 1;
    }

    closedir(dir);
    return 0;
#endif
}

void Sys_InitStreamThread(void)
{

}

void Sys_BeginStreamedFile(fileHandle_t f, int readAhead)
{

}

void Sys_EndStreamedFile(fileHandle_t f)
{

}

int Sys_StreamedRead(void *buffer, int size, int count, fileHandle_t f)
{
    return FS_Read(buffer, size * count, f);
}

void Sys_StreamSeek(fileHandle_t f, int offset, int origin)
{
    FS_Seek(f, offset, origin);
}
