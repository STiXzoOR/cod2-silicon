/* Item registration contracts through the complete production items TU. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void G_RegisterWeapon(int);
extern void RegisterItem(int, qboolean);
extern void ClearRegisteredItems(void);
extern void SaveRegisteredItems(void);
extern void SaveRegisteredWeapons(void);
extern qboolean IsItemRegistered(int);
qboolean itemRegistered[256];
level_locals_t level;
gitem_t bg_itemlist[256];
void *imp_bg_itemlist = bg_itemlist;
static int itemCount;
void *imp_bg_numItems = &itemCount;
static WeaponDef weapons[3];
static int failedHint, hintCalls, modelCalls, overrides, configIndex;
static char output[8192];
static jmp_buf errorJump;
WeaponDef *BG_GetWeaponDef(int index) { assert(index == 1 || index == 2); return &weapons[index]; }
int BG_GetNumWeapons(void) { return 2; }
int G_GetHintStringIndex(int *index, const char *s)
{
    ++hintCalls; *index = hintCalls;
    assert(!strcmp(s, hintCalls == 1 ? "use" : "drop"));
    return failedHint != hintCalls;
}
int G_ModelIndex(const char *s)
{
    assert(!strcmp(s, modelCalls == 0 ? "world" : "projectile"));
    return ++modelCalls;
}
qboolean G_XModelBad(int index) { assert(index == 1); return 1; }
void G_OverrideModel(int index, const char *s)
{ assert(index == 1 && !strcmp(s, "xmodel/defaultweapon")); ++overrides; }
void Com_Error(int code, const char *format, ...)
{
    assert(code == 1 && failedHint);
    va_list args; va_start(args, format);
    vsnprintf(output, sizeof(output), format, args); va_end(args);
    longjmp(errorJump, 1);
}
char *va(const char *format, ...)
{
    static char text[512]; va_list args; va_start(args, format);
    vsnprintf(text, sizeof(text), format, args); va_end(args); return text;
}
void Scr_Error(const char *s) { abort(); }
void I_strncat(char *dest, int size, const char *src)
{ assert(strlen(dest) + strlen(src) < (size_t)size); strcat(dest, src); }
void SV_SetConfigstring(int index, const char *s)
{ configIndex = index; snprintf(output, sizeof(output), "%s", s); }
static void Reset(void)
{
    memset(&level, 0, sizeof(level)); memset(itemRegistered, 0, sizeof(itemRegistered));
    memset(weapons, 0, sizeof(weapons)); memset(bg_itemlist, 0, sizeof(bg_itemlist));
    hintCalls = modelCalls = overrides = 0;
    weapons[1].szUseHintString = "use"; weapons[1].dropHintString = "drop";
    weapons[1].szWorldModel = "world"; weapons[1].szProjectileModel = "projectile";
    weapons[1].szInternalName = "first"; weapons[2].szInternalName = "second";
}
static void HintError(int index)
{
    Reset(); failedHint = index;
    if (!setjmp(errorJump)) { G_RegisterWeapon(1); abort(); }
    assert(!strcmp(output, "\x15" "Too many different hintstring values on weapons. Max allowed is 32 different strings"));
    assert(hintCalls == index && modelCalls == 0);
}
static void Controls(void)
{
    Reset(); G_RegisterWeapon(1);
    assert(itemRegistered[1] && level.bRegisterItems && level.registerWeapons);
    assert(hintCalls == 2 && modelCalls == 2 && overrides == 1);
    ClearRegisteredItems();
    assert(IsItemRegistered(0) && !IsItemRegistered(1) && !IsItemRegistered(255));
    level.initializing = 1;
    RegisterItem(2, 1); assert(IsItemRegistered(2));
    itemRegistered[3] = 1; itemCount = 9;
    SaveRegisteredItems(); assert(configIndex == 8 && !strcmp(output, "d00"));
    assert(!level.bRegisterItems);
    SaveRegisteredWeapons(); assert(configIndex == 7 && !strcmp(output, "first second"));
    assert(!level.registerWeapons);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "use-error")) HintError(1);
    else if (!strcmp(argv[1], "drop-error")) HintError(2);
    else if (!strcmp(argv[1], "controls")) Controls();
    else abort();
    printf("item registry %s: pass\n", argv[1]); return 0;
}
