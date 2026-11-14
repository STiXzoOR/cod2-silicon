#ifndef COD2_MACOS_DISPLAY_MODES_H
#define COD2_MACOS_DISPLAY_MODES_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct { int width, height, depth, refresh; } MacDisplayMode;
enum { MAC_MODE_LIMIT = 127, MAC_MODE_NAME_SIZE = 32 };

/* Pure render-size policy. SDL's Retina modes can be point sizes, so retain
 * the standard render sizes as well as the display's native pixel size.
 * Windows 1.3 orders by width, then height, and removes refresh duplicates. */
static int MacPlatform_BuildModeNames(int nativeWidth, int nativeHeight,
                                    const MacDisplayMode *available, size_t count,
                                    char storage[MAC_MODE_LIMIT][MAC_MODE_NAME_SIZE],
                                    const char *names[MAC_MODE_LIMIT + 1])
{
    static const MacDisplayMode standard[] = {
        {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1920, 1080},
        {2560, 1440}, {3008, 1692}, {3840, 2160}, {5120, 2880}, {6016, 3384}
    };
    MacDisplayMode sorted[MAC_MODE_LIMIT];
    const size_t standards = sizeof(standard) / sizeof(*standard);
    int used = 0;
    /* Insert native first so it survives even a truncated driver mode list. */
    for (size_t i = 0; i < 1 + standards + count; ++i) {
        MacDisplayMode mode = i == 0 ? (MacDisplayMode){nativeWidth, nativeHeight} :
            i <= standards ? standard[i - 1] : available[i - 1 - standards];
        if (mode.width < 640 || mode.height < 480 ||
            mode.width > nativeWidth || mode.height > nativeHeight)
            continue;
        int at = 0;
        while (at < used && (sorted[at].width < mode.width ||
               (sorted[at].width == mode.width && sorted[at].height < mode.height)))
            ++at;
        if ((at < used && sorted[at].width == mode.width && sorted[at].height == mode.height) ||
            used == MAC_MODE_LIMIT)
            continue;
        memmove(sorted + at + 1, sorted + at, (used - at) * sizeof(*sorted));
        sorted[at] = mode;
        ++used;
    }
    for (int i = 0; i < used; ++i) {
        snprintf(storage[i], MAC_MODE_NAME_SIZE, "%dx%d", sorted[i].width, sorted[i].height);
        names[i] = storage[i];
    }
    names[used] = NULL;
    return used;
}

/* Preserve a saved size by name across a changed enum. If it disappeared,
 * choose the largest offered size that fits inside the request. */
static int MacPlatform_ChooseMode(const char **names, const char *requested)
{
    int width = 0, height = 0, selected = 0;
    if (sscanf(requested, "%dx%d", &width, &height) != 2) {
        int index;
        char extra;
        if (sscanf(requested, "%d%c", &index, &extra) == 1 && index >= 0) {
            for (int i = 0; names[i]; ++i)
                if (i == index) return i;
        }
        return 0;
    }
    for (int i = 0; names[i]; ++i) {
        int w, h;
        if (!strcmp(names[i], requested))
            return i;
        if (sscanf(names[i], "%dx%d", &w, &h) == 2 && w <= width && h <= height)
            selected = i;
    }
    return selected;
}
#endif
