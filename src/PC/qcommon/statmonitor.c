#include "common_types.h"
#include "imports.h"
#include "bytematch.h"

extern int Sys_Milliseconds(void);
extern void Com_Error(int code, const char *fmt, ...);

static statmonitor_t stats[7];
static int statCount;

extern char **sm_dvar_ptr;
extern char *sm_mtl_init;
extern int (**sm_mtl_vtable)();

void StatMon_Warning(int type, int duration, const char *materialName)
{
    if (!(*(dvar_t **)(sm_dvar_ptr))->current.enabled)
        return;

    if ((unsigned int)type > 6)
        Com_Error(1, "StatMon_UpdateEntry: invalid entry '%i'\n", type);

    stats[type].endtime = Sys_Milliseconds() + duration;
    if (!stats[type].material) {
        if (((clientStatic_t *)sm_mtl_init)->rendererStarted) {
            stats[type].material = (MaterialHandle)((int (*)(const char *, int, int))((void **)sm_mtl_vtable)[4])(materialName, 0x30, 1);
        }
    }
    if (type >= statCount)
        statCount = type + 1;
}

void StatMon_GetStatsArray(const statmonitor_t **array, int *count)
{
    *array = stats;
    *count = statCount;
}

void StatMon_Reset(void)
{
    memset(stats, 0, sizeof(stats));
    statCount = 0;
}
