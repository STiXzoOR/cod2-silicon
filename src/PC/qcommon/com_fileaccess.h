#ifndef COD2_COM_FILEACCESS_H
#define COD2_COM_FILEACCESS_H
#include "cod2_platform.h"
#include <stddef.h>
#if COD2_APPLE_SDK && defined(COD2_X64)
/* Resolve existing components, retaining spelling for components to be created.
 * Returns 0 on success or -1 with errno set. Output must not alias input. */
int FS_ResolveOSPath(const char *path, char *resolved, size_t capacity);
/* One canonical name per case-insensitive identity; caller frees the result. */
char **FS_ListOSDirectory(const char *path, int *count);
void FS_FreeOSDirectory(char **names, int count);
int FS_RemoveOSPath(const char *path);
int FS_RenameOSPath(const char *from, const char *to);
#endif
#endif
