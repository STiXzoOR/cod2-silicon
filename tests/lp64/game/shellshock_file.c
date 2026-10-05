#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern qboolean CG_LoadShellShockDvars(const char *);
extern const char *cg_shock_dvar_names[29];
static char sharedVa[1024];

const char *va(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(sharedVa, sizeof(sharedVa), format, args);
    va_end(args);
    return sharedVa;
}

int FS_FOpenFileByMode(const char *name, int *handle, int mode)
{
    assert(!strcmp(name, "shock/synthetic.shock") && mode == 0);
    *handle = 1;
    return 0;
}
int FS_Read(void *buffer, int length, int handle)
{
    assert(buffer && length == 0 && handle == 1);
    strcpy(sharedVa, "file-service scratch");
    return 0;
}
void FS_FCloseFile(fileHandle_t handle) { assert(handle == 1); }
void *Z_MallocInternal(int size) { return malloc(size); }
void Z_FreeInternal(void *memory) { free(memory); }
void Com_Printf(const char *format, ...) { (void)format; assert(0); }

qboolean Com_LoadDvarsFromBuffer(const char **names, int count, const char *buffer, const char *filename)
{
    /* Verified Mac 1.3 table: 29 entries; view fade stays at its 3000 ms default. */
    assert(names == cg_shock_dvar_names && count == 29 && !*buffer);
    assert(!strcmp(names[4], "cg_shock_sound"));
    assert(!strcmp(names[28], "cg_shock_mouse_fadeTime"));
    for (int i = 0; i < count; ++i)
        assert(strcmp(names[i], "cg_shock_viewKickFadeTime"));
    strcpy(sharedVa, "dvar-parser scratch");
    assert(!strcmp(filename, "shock/synthetic.shock"));
    return 1;
}

int main(void)
{
    assert(CG_LoadShellShockDvars("synthetic"));
    puts("native shellshock: stable filename and reference table/count passed");
}
