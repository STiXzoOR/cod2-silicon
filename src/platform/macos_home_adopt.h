/* The app was called CoD2 Silicon until 0.3.6, and kept its files in a folder of that name. The
   first start under the new name takes that folder over: when "CoD2 Unbound" is not there and
   "CoD2 Silicon" is a real folder, it is renamed. One step on the same volume, nothing copied,
   and a folder that already has the new name is never touched. Included by the game, its setup
   wrapper and the launcher, whichever starts first. */
#ifndef MACOS_HOME_ADOPT_H
#define MACOS_HOME_ADOPT_H
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>

/* `home` is the user's home folder. Returns 1 when the old folder was taken over. */
static inline int MacHome_AdoptOldName(const char *home)
{
    char old[PATH_MAX], current[PATH_MAX];
    struct stat info;
    if (!home || !home[0]) return 0;
    if (snprintf(old, sizeof(old), "%s/Library/Application Support/CoD2 Silicon", home) >= (int)sizeof(old)) return 0;
    if (snprintf(current, sizeof(current), "%s/Library/Application Support/CoD2 Unbound", home) >= (int)sizeof(current)) return 0;
    if (lstat(current, &info) == 0) return 0;
    if (lstat(old, &info) != 0 || !S_ISDIR(info.st_mode)) return 0;
    return rename(old, current) == 0;
}
#endif
