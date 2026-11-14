#include "common_types.h"
#include "../../src/PC/cgame_mp/cg_cod2x_spectator.c"
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern void Dvar_Shutdown(void);
int dvarCount, dvar_modifiedFlags;
dvar_t *sortedDvars;
const char str_00219524[] = "off", str_00219528[] = "on";
static jmp_buf exhausted;
static char error[512];
static void (*testLimit)(void);
static char *testArgument;
void Dvar_AddCommands(void) {}
void Cmd_AddCommand(const char *name, void (*function)(void))
{
    assert(!strcmp(name, "dvarTestLimit"));
    testLimit = function;
}
int Cmd_Argc(void) { return testArgument ? 2 : 1; }
char *Cmd_Argv(int index) { return index == 1 ? testArgument : (char *)"dvarTestLimit"; }
extern void Dvar_Init(void);


void Com_Error(int code, const char *format, ...)
{
    va_list args;
    (void)code;
    va_start(args, format);
    vsnprintf(error, sizeof(error), format, args);
    va_end(args);
    longjmp(exhausted, 1);
}
void Com_Printf(const char *format, ...) { (void)format; }
void Com_PrintMessage(int channel, const char *message) { (void)channel; (void)message; }
char *CopyStringInternal(const char *s) { return strdup(s); }
void Z_FreeInternal(void *p) { free(p); }
void *Z_MallocInternal(int n) { return malloc(n); }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }
int ___tolower(int c) { return tolower(c); }
void I_strncpyz(char *out, const char *s, int n) { snprintf(out, n, "%s", s); }
int Com_sprintf(char *out, int n, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format); result = vsnprintf(out, n, format, args); va_end(args);
    return result;
}
char *va(const char *format, ...)
{
    static char out[4096];
    va_list args;
    va_start(args, format); vsnprintf(out, sizeof(out), format, args); va_end(args);
    return out;
}

int main(void)
{
    static char names[4097][20];
    int i;
#if COD2X_SPECTATOR
    static const char *const spectatorNames[] = {
        "cg_xray", "cg_xraySpectator", "cg_xrayDead", "cg_xrayVisibleSpectator", "cg_xrayVisibleDead",
        "cg_xrayCenterFadeRadius", "cg_xrayCenterFadeAlpha", "cg_xrayHiddenThreshold", "cg_xrayOutlineWidth",
        "cg_xrayGlowIntensity", "cg_xrayGlowCoreIntensity", "cg_xrayColorAxis", "cg_xrayColorAllies",
        "cg_xrayHealth", "cg_xrayHealthColor", "cg_xrayHealthAlpha",
        "cg_headNationIconSpectator", "cg_headNameSpectator", "cg_headWeaponIconSpectator", "cg_headWeaponNameSpectator",
        "cg_headNationIconDead", "cg_headNameDead", "cg_headWeaponIconDead", "cg_headWeaponNameDead",
        "cg_headNationIconAlive", "cg_headNameAlive", "cg_headWeaponIconAlive", "cg_headWeaponNameAlive",
        "cg_headNameScale", "cg_headWeaponIconScale", "cg_headIconScale", "cg_headOffset", "cg_headMaxDistance"
    };
    CG_Cod2xSpectatorInit();
    assert(dvarCount == 33);
    for (i = 0; i < 33; ++i) {
        const dvar_t *var = Dvar_FindVar(spectatorNames[i]);
        printf("spectator retained registration %s: %s\n", spectatorNames[i], var ? "found" : "missing");
        fflush(stdout);
        assert(var && !strcmp(var->name, spectatorNames[i]));
    }
    CG_Cod2xSpectatorInit();
    assert(dvarCount == 33);
    Dvar_Shutdown();
#endif
    Dvar_Init();
#if defined(DEBUG) && DEBUG
    assert(testLimit);
    testArgument = "0"; testLimit(); assert(dvarCount == 1);
    testArgument = "4097"; testLimit(); assert(dvarCount == 1);
    testArgument = "3"; testLimit(); assert(dvarCount == 4);
    for (i = 0; i < 3; ++i) {
        char name[32]; snprintf(name, sizeof(name), "ui_testDvar%i", i);
        const dvar_t *var = Dvar_FindVar(name);
        assert(var && var->current.integer == 0 && var->domain.integer.min == 0 &&
               var->domain.integer.max == 1 && var->flags == 0x1000);
    }
    puts("DEBUG dvarTestLimit: bounds and named registrations passed");
#else
    assert(!testLimit);
#endif
    Dvar_Shutdown();
    if (!setjmp(exhausted)) {
        for (i = 0; i < 4096; ++i) {
            const dvar_t *var;
            snprintf(names[i], sizeof(names[i]), "ws10_%04d", i);
            var = Dvar_RegisterInt(names[i], i, 0, 5000, 0);
            assert(var && var->current.integer == i);
            assert(Dvar_FindVar(names[i]) == var);
        }
        assert(dvarCount == 4096);
        Dvar_RegisterInt("overflow", 0, 0, 1, 0);
        assert(!"expected exhaustion");
    }
    assert(dvarCount == 4096);
    assert(strstr(error, "4096") && strstr(error, "overflow"));
    Dvar_Shutdown();
    assert(dvarCount == 0 && !Dvar_FindVar("ws10_0000"));
    assert(Dvar_RegisterInt("reuse", 7, 0, 10, 0)->current.integer == 7);
    puts("CoD2x actual dvar pool: 4096 registrations, exhaustion, reuse passed");
    return 0;
}
