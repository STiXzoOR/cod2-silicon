#include "common_types.h"
#include "imports.h"

extern Bool Dvar_IsSystemActive(void);
extern Bool Dvar_GetBool(const char *name);
extern int Dvar_GetInt(const char *name);

static Bool shouldQuitOnError;

static inline __attribute__((always_inline)) void RefreshQuitOnErrorCondition_inline(void)
{
    if (!Dvar_IsSystemActive())
        return;

    if (Dvar_GetBool("QuitOnError") || Dvar_GetInt("r_vc_compile") == 2)
        shouldQuitOnError = 1;
    else
        shouldQuitOnError = 0;
}

void RefreshQuitOnErrorCondition(void)
{
    RefreshQuitOnErrorCondition_inline();
}

Bool QuitOnError(void)
{
    RefreshQuitOnErrorCondition_inline();
    return shouldQuitOnError;
}
