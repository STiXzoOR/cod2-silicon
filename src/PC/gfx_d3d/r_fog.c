#include "common_types.h"
#include "imports.h"

typedef void (*ArchiveProc)(MemoryFile *, int, void *);

extern r_globals_t rg;

void R_ClearFogs(void)
{
    r_globals_t *rgg = &rg;
    memset(rgg->fogSettings, 0, sizeof(rgg->fogSettings));
    rgg->fogIndex = 0;
}

void R_SetFog(int fogvar, float start, float end, int r, int g, int b, float density)
{
    GfxFog *fog = &rg.fogSettings[fogvar];

    fog->color.array[0] = (byte)b;
    fog->color.array[1] = (byte)g;
    fog->color.array[2] = (byte)r;
    fog->color.array[3] = 0xFF;

    fog->fogStart = start;
    fog->fogEnd = end;

    if (density >= 1.0f) {
        fog->techniqueOffset = 1;
        fog->drawSky = 1;
        fog->clearScreen = 0;
        fog->density = 1.0f;
    } else {
        fog->techniqueOffset = 2;
        fog->drawSky = 1;
        fog->clearScreen = 0;
        fog->density = density;
    }

    fog->registered = 1;
}

void R_SwitchFog(int fogvar, int startTime, int transitionTime)
{
    r_globals_t *rgg = &rg;

    rgg->fogIndex = fogvar;

    if (rgg->fogSettings[2].registered) {
        rgg->fogSettings[3] = rgg->fogSettings[2];
    } else {
        rgg->fogSettings[3] = rgg->fogSettings[fogvar];
        transitionTime = 0;
    }

    rgg->fogSettings[4] = rgg->fogSettings[rgg->fogIndex];

    if (transitionTime == 0) {
        rgg->fogSettings[4].startTime = 0;
        rgg->fogSettings[4].finishTime = 0;
    } else {
        rgg->fogSettings[4].startTime = startTime;
        rgg->fogSettings[4].finishTime = startTime + transitionTime;
    }
}

void R_ArchiveFogState(MemoryFile *memFile)
{
    r_globals_t *rgg = &rg;
    ((ArchiveProc)memFile->archiveProc)(memFile, sizeof(rgg->fogSettings), rgg->fogSettings);
    ((ArchiveProc)memFile->archiveProc)(memFile, sizeof(rgg->fogIndex), &rgg->fogIndex);
}
