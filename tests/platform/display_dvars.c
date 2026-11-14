/* Display fixtures have no engine dvar registry. Exercise the native window
 * adapter with the original archived-position defaults. */
#include <string.h>
static int xpos = 3, ypos = 22;
const void *Dvar_FindVar(const char *name)
{
    return !strcmp(name, "vid_xpos") ? &xpos : (!strcmp(name, "vid_ypos") ? &ypos : 0);
}
int Dvar_GetInt(const char *name)
{
    const int *value = Dvar_FindVar(name);
    return value ? *value : 0;
}
void Dvar_SetInt(const void *dvar, int value) { *(int *)dvar = value; }
void Dvar_ClearModified(const void *dvar) { (void)dvar; }
