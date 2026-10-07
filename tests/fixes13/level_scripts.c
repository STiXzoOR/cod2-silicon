#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define COD2_DEBUG_ONLY(...)
static level_locals_t level;
static scr_data_t g_scr_data;
static qboolean itemRegistered[256];
static int numItems = 4;
static void *imp_bg_numItems = &numItems;
enum { GITEMS_CS_WEAPONS = 7, GITEMS_CS_ITEMS = 8 };
static int handles[3], calls, frees, writes;
static char weapons[128], items[128];
static WeaponDef rifle = { .szInternalName = "fixture_rifle" };

unsigned short Scr_ExecThread(unsigned int handle, unsigned int params)
{
    assert(params == 0 && calls < 3 && level.initializing);
    handles[calls++] = handle;
    itemRegistered[0] = itemRegistered[2] = 1;
    level.registerWeapons = level.bRegisterItems = 1;
    return calls;
}
void Scr_FreeThread(unsigned short thread) { assert(thread == ++frees); }
void CalculateRanks(void) { assert(calls == 3); }
int BG_GetNumWeapons(void) { return 1; }
WeaponDef *BG_GetWeaponDef(int index) { assert(index == 1); return &rifle; }
void I_strncat(char *dest, int size, const char *src)
{ assert(strlen(dest) + strlen(src) < (size_t)size); strcat(dest, src); }
void SV_SetConfigstring(int index, const char *value)
{
    assert(!level.initializing && calls == 3 && writes < 2);
    assert(index == (writes++ == 0 ? 7 : 8));
    strcpy(index == 7 ? weapons : items, value);
}
#include "level_scripts_source.h"

int main(void)
{
    g_scr_data.levelscript = 11;
    g_scr_data.gametype.main = 22;
    g_scr_data.gametype.startupgametype = 33;
    initialize_level_scripts();
    int failures = 0;
    if (calls != 3 || handles[0] != 22 || handles[1] != 11 || handles[2] != 33) {
        puts("FAIL Mac startup order: gametype, map, startup callback");
        ++failures;
    }
    if (writes != 2 || strcmp(weapons, "fixture_rifle") || strcmp(items, "5") ||
        level.registerWeapons || level.bRegisterItems) {
        puts("FAIL initial weapon/item configstrings published before first frame");
        ++failures;
    }
    assert(frees == 3);
    if (!failures) puts("PASS level script order and initial registration publication");
    return failures != 0;
}
