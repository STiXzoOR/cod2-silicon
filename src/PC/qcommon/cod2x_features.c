#include "cod2_feature_config.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "common_types.h"
#include "cod2x.h"
#include "cod2x_features.h"
#include "cod2x_pose.h"
#include <stdio.h>
#include <string.h>

extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterFloat(const char *, float, float, float, unsigned short);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern void Dvar_SetIntFromSource(const dvar_t *, int, DvarSetSource);
extern void Dvar_SetFloatFromSource(const dvar_t *, float, DvarSetSource);
extern void Dvar_SetString(const dvar_t *, const char *);
extern void Cmd_AddCommand(const char *, void (*)(void));
extern int Cmd_Argc(void);
extern char *Cmd_Argv(int);
extern void Com_Printf(const char *, ...);

static const dvar_t *thirdPersonMode, *debugBullets, *printDoubleColors;
static const dvar_t *drawSpectatedName, *drawCompass, *compassOffsetX, *compassOffsetY;
static const dvar_t *matchLogin;
static int lastVersion = -1, lastActive, playingDemo;

static void Cod2x_IncreaseDecrease_f(void)
{
    const dvar_t *var;
    int direction = strcmp(Cmd_Argv(0), "decrease") ? 1 : -1;
    if (Cmd_Argc() != 2) {
        Com_Printf("%s <variablename>\n", Cmd_Argv(0));
        return;
    }
    var = Dvar_FindVar(Cmd_Argv(1));
    if (!var) {
        Com_Printf("%s not found\n", Cmd_Argv(1));
        return;
    }
    /* Use the external setter so binds retain cheat/ROM/latch permissions. */
    if (var->type == 5)
        Dvar_SetIntFromSource(var, Cod2x_StepInt(var->current.integer, direction), DVAR_SOURCE_EXTERNAL);
    else if (var->type == 1)
        Dvar_SetFloatFromSource(var, Cod2x_StepFloat(var->current.value, direction), DVAR_SOURCE_EXTERNAL);
    else
        Com_Printf("%s is not an int or float\n", Cmd_Argv(1));
}

static void Cod2x_Match_f(void)
{
    const char *hash;
    if (Cmd_Argc() != 3 || strcmp(Cmd_Argv(1), "login")) {
        Com_Printf("match login <hash>\n");
        return;
    }
    hash = Cmd_Argv(2);
    if (!*hash) {
        Com_Printf("Invalid match login hash\n");
        return;
    }
    Dvar_SetString(matchLogin, hash);
}

void Cod2x_FeaturesInit(void)
{
    Cod2x_PoseInit();
    /* CoD2x cgame.cpp:65 and drawing.cpp:270-279 describe these controls. */
    thirdPersonMode = Dvar_RegisterInt("cg_thirdPersonMode", 0, 0, 1, 0x1080);
    debugBullets = Dvar_RegisterBool("cg_debugBullets", 0, 0x1080);
    printDoubleColors = Dvar_RegisterBool("con_printDoubleColors", 1, 0x1000);
    drawSpectatedName = Dvar_RegisterBool("cg_drawSpectatedPlayerName", 1, 0x1000);
    drawCompass = Dvar_RegisterBool("cg_drawCompass", 1, 0x1000);
    compassOffsetX = Dvar_RegisterFloat("cg_hudCompassOffsetX", 0, -640, 640, 0x1000);
    compassOffsetY = Dvar_RegisterFloat("cg_hudCompassOffsetY", 0, -480, 480, 0x1000);
    matchLogin = Dvar_RegisterString("match_login", "", 0x12);
    Cmd_AddCommand("increase", Cod2x_IncreaseDecrease_f);
    Cmd_AddCommand("decrease", Cod2x_IncreaseDecrease_f);
    Cmd_AddCommand("match", Cod2x_Match_f);
}

void Cod2x_FeaturesFrame(int active, int demo)
{
    int version = Cod2x_GameVersion();
    playingDemo = demo;
    if (active && (version != lastVersion || !lastActive)) {
        if (version)
            Com_Printf("CoD2x: compatibility enabled for server extension %d\n", version);
        else
            Com_Printf("CoD2x: using legacy CoD2 1.3 behavior\n");
        lastVersion = version;
    }
    lastActive = active;
}

int Cod2x_FeaturesDemo(void)
{
    return playingDemo;
}

int Cod2x_ThirdPersonMode(void)
{
    return thirdPersonMode ? thirdPersonMode->current.integer : 0;
}

int Cod2x_DebugBullets(void)
{
    const dvar_t *cheats = Dvar_FindVar("sv_cheats");
    return debugBullets && debugBullets->current.enabled && cheats && cheats->current.enabled;
}

int Cod2x_PrintDoubleColors(void)
{
    return !printDoubleColors || printDoubleColors->current.enabled;
}

int Cod2x_DrawSpectatedName(void)
{
    return !drawSpectatedName || drawSpectatedName->current.enabled;
}

int Cod2x_DrawCompass(void)
{
    return !drawCompass || drawCompass->current.enabled;
}

void Cod2x_CompassOffset(float *x, float *y)
{
    if (compassOffsetX)
        *x += compassOffsetX->current.value;
    if (compassOffsetY)
        *y += compassOffsetY->current.value;
}
#endif
