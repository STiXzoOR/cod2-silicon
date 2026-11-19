#include "cod2_platform.h"
#if !COD2_APPLE_SDK || !defined(COD2_X64)
#include "common_types.h"
#include "imports.h"
#endif
#include <stdio.h>
#include "com_fileaccess.h"
#if COD2_APPLE_SDK && defined(COD2_X64)
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

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
#if COD2_APPLE_SDK && defined(COD2_X64)
    char resolved[PATH_MAX];
    if (FS_ResolveOSPath(filename, resolved, sizeof(resolved)) != 0)
        return NULL;
    return fopen(resolved, mode);
#else
    return fopen(filename, mode);
#endif
}

int FS_FileClose(FILE *stream)
{
    return fclose(stream);
}

int FS_FileSeek(FILE *file, long int offset, int whence)
{
    return fseek(file, offset, whence);
}

#if COD2_APPLE_SDK && defined(COD2_X64)
/* Windows resolves every component without case sensitivity. On a native
 * case-insensitive volume, leave that operation to the filesystem. On a
 * case-sensitive volume choose the bytewise-smallest equivalent name, even
 * when an exact spelling exists: reads, writes and enumeration must agree.
 * UTF-8 names use non-expanding, invariant uppercase per UTF-16 code unit,
 * as in Windows name comparison. No extra normalization is applied; invalid
 * UTF-8 names are compared bytewise. No locale or cached directory
 * contents are involved. Symlinks retain normal POSIX traversal semantics.
 * Linux volumes are taken as case-sensitive and, without CoreFoundation, fold
 * ASCII letters only. Windows resolves names itself, so nothing is searched. */
#if defined(__APPLE__)
#define FS_OS_PATH_CASE_SENSITIVE(path) (pathconf(path, _PC_CASE_SENSITIVE) != 0)
#define FS_OS_DIR_CASE_SENSITIVE(directory) (fpathconf(dirfd(directory), _PC_CASE_SENSITIVE) != 0)
#elif defined(_WIN32)
#define FS_OS_PATH_CASE_SENSITIVE(path) 0
#define FS_OS_DIR_CASE_SENSITIVE(directory) 0
#else
#define FS_OS_PATH_CASE_SENSITIVE(path) 1
#define FS_OS_DIR_CASE_SENSITIVE(directory) 1
#endif

#if defined(__APPLE__)
static UniChar FS_OSUpperCharacter(UniChar character)
{
    if (character < 128)
        return character >= 'a' && character <= 'z' ? character - ('a' - 'A') : character;
    CFMutableStringRef upper = CFStringCreateMutable(NULL, 1);
    if (!upper) return character;
    CFStringAppendCharacters(upper, &character, 1);
    CFStringUppercase(upper, NULL);
    /* Full Unicode folds such as sharp-s -> SS would merge distinct Windows
     * names. Preserve characters whose uppercase expands to several units. */
    if (CFStringGetLength(upper) == 1) character = CFStringGetCharacterAtIndex(upper, 0);
    CFRelease(upper);
    return character;
}
#endif

static int FS_OSNameCompare(const char *a, const char *b)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    for (; *pa < 128 && *pb < 128; ++pa, ++pb) {
        unsigned ca = *pa, cb = *pb;
        if (ca >= 'a' && ca <= 'z') ca -= 'a' - 'A';
        if (cb >= 'a' && cb <= 'z') cb -= 'a' - 'A';
        if (ca != cb) return ca < cb ? -1 : 1;
        if (!ca) return 0;
    }
#if !defined(__APPLE__)
    for (;; ++pa, ++pb) {
        unsigned ca = *pa, cb = *pb;
        if (ca >= 'a' && ca <= 'z') ca -= 'a' - 'A';
        if (cb >= 'a' && cb <= 'z') cb -= 'a' - 'A';
        if (ca != cb) return ca < cb ? -1 : 1;
        if (!ca) return 0;
    }
#else
    CFStringRef sa = CFStringCreateWithCString(NULL, a, kCFStringEncodingUTF8);
    CFStringRef sb = CFStringCreateWithCString(NULL, b, kCFStringEncodingUTF8);
    int result = strcmp(a, b);
    if (sa && sb) {
        CFIndex la = CFStringGetLength(sa), lb = CFStringGetLength(sb), i = 0;
        for (; i < la && i < lb; ++i) {
            UniChar ca = FS_OSUpperCharacter(CFStringGetCharacterAtIndex(sa, i));
            UniChar cb = FS_OSUpperCharacter(CFStringGetCharacterAtIndex(sb, i));
            if (ca != cb) { result = ca < cb ? -1 : 1; break; }
        }
        if (i == la || i == lb) result = la == lb ? 0 : la < lb ? -1 : 1;
    }
    if (sa) CFRelease(sa);
    if (sb) CFRelease(sb);
    return result;
#endif
}

static int FS_OSNameSort(const void *a, const void *b)
{
    const char *sa = *(char *const *)a, *sb = *(char *const *)b;
    int result = FS_OSNameCompare(sa, sb);
    return result ? result : strcmp(sa, sb);
}

static int FS_OSDirectoryIsCaseSensitive(DIR *directory)
{
    /* Unknown filesystems take the conservative resolving path. */
    (void)directory;
    return FS_OS_DIR_CASE_SENSITIVE(directory);
}

int FS_ResolveOSPath(const char *path, char *resolved, size_t capacity)
{
    size_t used = 0;
    const char *cursor = path;
    if (!path || !resolved || !capacity) {
        errno = EINVAL;
        return -1;
    }
    if (*cursor == '/') {
        if (capacity < 2) goto too_long;
        resolved[used++] = *cursor++;
    }
    resolved[used] = 0;
    while (*cursor) {
        const char *start;
        size_t length;
        char component[PATH_MAX], selected[PATH_MAX];
        DIR *directory;
        while (*cursor == '/') ++cursor;
        if (!*cursor) break;
        start = cursor;
        while (*cursor && *cursor != '/') ++cursor;
        length = (size_t)(cursor - start);
        if (length >= sizeof(component)) goto too_long;
        memcpy(component, start, length);
        component[length] = 0;
        strcpy(selected, component);
        /* Checking the parent directly avoids opening each directory on the
         * usual case-insensitive volume. Still check every parent: a nested
         * mount or symlink can enter a case-sensitive filesystem mid-path. */
        int sensitive = FS_OS_PATH_CASE_SENSITIVE(used ? resolved : ".");
        directory = sensitive ? opendir(used ? resolved : ".") : NULL;
        if (directory) {
            if (strcmp(component, ".") && strcmp(component, "..")) {
                struct dirent *entry;
                int found = 0, saved;
                errno = 0;
                while ((entry = readdir(directory)) != NULL) {
                    if (!FS_OSNameCompare(component, entry->d_name) &&
                        (!found || strcmp(entry->d_name, selected) < 0)) {
                        strcpy(selected, entry->d_name);
                        found = 1;
                    }
                    errno = 0;
                }
                saved = errno;
                if (saved) { closedir(directory); errno = saved; return -1; }
            }
            closedir(directory);
        } else if (sensitive && errno != ENOENT && errno != ENOTDIR) {
            return -1;
        }
        length = strlen(selected);
        if (used && resolved[used - 1] != '/') {
            if (used + 1 >= capacity) goto too_long;
            resolved[used++] = '/';
        }
        if (length >= capacity - used) goto too_long;
        memcpy(resolved + used, selected, length + 1);
        used += length;
    }
    /* A trailing slash must still require a directory at the syscall. */
    if (cursor > path && cursor[-1] == '/' && used && resolved[used - 1] != '/') {
        if (used + 1 >= capacity) goto too_long;
        resolved[used++] = '/';
        resolved[used] = 0;
    }
    return 0;
too_long:
    errno = ENAMETOOLONG;
    return -1;
}

char **FS_ListOSDirectory(const char *path, int *count)
{
    char resolved[PATH_MAX];
    char **names = NULL;
    size_t capacity = 0;
    struct dirent *entry;
    DIR *directory;
    int sensitive, saved;
    *count = 0;
    if (FS_ResolveOSPath(path, resolved, sizeof(resolved))) return NULL;
    directory = opendir(resolved);
    if (!directory) return NULL;
    sensitive = FS_OSDirectoryIsCaseSensitive(directory);
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if ((size_t)*count == capacity) {
            size_t next = capacity ? capacity * 2 : 32;
            char **grown = realloc(names, next * sizeof(*names));
            if (!grown) goto failed;
            names = grown;
            capacity = next;
        }
        names[*count] = strdup(entry->d_name);
        if (!names[*count]) goto failed;
        ++*count;
        errno = 0;
    }
    if (errno) goto failed;
    closedir(directory);
    if (sensitive && *count > 1) {
        int write = 1;
        qsort(names, *count, sizeof(*names), FS_OSNameSort);
        for (int read = 1; read < *count; ++read) {
            if (!FS_OSNameCompare(names[write - 1], names[read])) free(names[read]);
            else names[write++] = names[read];
        }
        *count = write;
    }
    return names;
failed:
    saved = errno;
    closedir(directory);
    FS_FreeOSDirectory(names, *count);
    *count = 0;
    errno = saved;
    return NULL;
}

void FS_FreeOSDirectory(char **names, int count)
{
    for (int i = 0; i < count; ++i) free(names[i]);
    free(names);
}

int FS_RemoveOSPath(const char *path)
{
    char resolved[PATH_MAX];
    return FS_ResolveOSPath(path, resolved, sizeof(resolved)) ? -1 : remove(resolved);
}

int FS_RenameOSPath(const char *from, const char *to)
{
    char source[PATH_MAX], destination[PATH_MAX];
    if (FS_ResolveOSPath(from, source, sizeof(source)) ||
        FS_ResolveOSPath(to, destination, sizeof(destination))) return -1;
    if (!strcmp(source, destination)) {
        const char *requestedName = strrchr(to, '/');
        char *storedName = strrchr(destination, '/');
        requestedName = requestedName ? requestedName + 1 : to;
        storedName = storedName ? storedName + 1 : destination;
        if (*requestedName && strcmp(storedName, requestedName)) {
            char parent[PATH_MAX];
            size_t prefix = (size_t)(storedName - destination);
            if (prefix) { memcpy(parent, destination, prefix); parent[prefix] = 0; }
            else strcpy(parent, ".");
            DIR *directory = opendir(parent);
            int matches = 0;
            struct dirent *entry;
            if (!directory) return -1;
            while ((entry = readdir(directory)) != NULL)
                if (!FS_OSNameCompare(storedName, entry->d_name)) ++matches;
            closedir(directory);
            /* Windows can update stored casing. If a case-sensitive disk has
             * multiple aliases, keep its canonical entry instead of replacing
             * a hidden alias or changing which file future lookups select. */
            if (matches == 1) {
                if (strlen(requestedName) >= sizeof(destination) - prefix) {
                    errno = ENAMETOOLONG;
                    return -1;
                }
                strcpy(storedName, requestedName);
            }
        }
    }
    return rename(source, destination);
}
#endif
