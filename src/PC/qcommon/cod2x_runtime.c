#if defined(COD2_CODX) && COD2_CODX
#include "common_types.h"
#include "cod2x.h"
#if defined(COD2_X64) && COD2_X64 && !defined(DEDICATED)
#include "cod2x_policy.h"
#endif
#include <string.h>

extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern void Dvar_SetInt(const dvar_t *, int);
extern void Dvar_SetBool(const dvar_t *, unsigned char);
extern void Dvar_SetString(const dvar_t *, const char *);
extern void Com_Printf(const char *, ...);

static const dvar_t *cod2x_game, *cod2x_competitive, *cod2x_fpsLimit;
static const dvar_t *cod2x_revision, *cod2x_identity;
static char cod2x_hwid[33];
static int cod2x_playing, cod2x_demo;

int Cod2x_GameVersion(void)
{
    return cod2x_game ? cod2x_game->current.integer : 0;
}

int Cod2x_Competitive(void)
{
    return cod2x_playing && !cod2x_demo && Cod2x_GameVersion() > 0 &&
           cod2x_competitive && cod2x_competitive->current.enabled;
}

static int Cod2x_FPSLimited(void)
{
    return Cod2x_Competitive() ||
        (cod2x_playing && !cod2x_demo && Cod2x_GameVersion() > 0 &&
         cod2x_fpsLimit && cod2x_fpsLimit->current.enabled);
}

int Cod2x_FrameFPS(int requested)
{
    return Cod2x_LimitedFPS(requested, Cod2x_FPSLimited());
}

const char *Cod2x_Hwid(void)
{
    return cod2x_hwid;
}

void Cod2x_Init(void)
{
    /* NOWRITE allows internal server updates, blocks console/config changes.
       CoD2x src/shared/game.cpp:14, src/shared/server.cpp:1313-1315. */
    cod2x_game = Dvar_RegisterInt("g_cod2x", 0, 0, COD2X_REVISION, 0x18);
    cod2x_competitive = Dvar_RegisterBool("g_competitive", 0, 0x18);
    cod2x_fpsLimit = Dvar_RegisterBool("com_maxfps_limit", 0, 0x98);
    cod2x_revision = Dvar_RegisterInt("protocol_cod2x", COD2X_REVISION, COD2X_REVISION, COD2X_REVISION, 0x42);
    Dvar_RegisterString("cl_cod2x_version", COD2X_VERSION, 0x40);
    if (!Cod2x_ReadMachineHwid(cod2x_hwid))
        Com_Printf("CoD2x: machine identity unavailable; CoD2x servers will reject this client.\n");
    cod2x_identity = Dvar_RegisterString("cl_hwid2", cod2x_hwid, 0x42);
}

void Cod2x_PrepareConnect(void)
{
    /* A previous server may have used an internal cvar command. Always send
       this machine's identity and the implemented revision on a new connection. */
    Dvar_SetInt(cod2x_revision, COD2X_REVISION);
    Dvar_SetString(cod2x_identity, cod2x_hwid);
}

/* Only the limits are restored: like CoD2x, the user keeps the applied values.
   Preserve each original domain/type instead of assuming renderer dvar types. */
static struct {
    const char *name;
    int min, max, value;
    const dvar_t *var;
    DvarLimits domain;
    byte type;
} cod2x_settings[] = {
    { .name = "com_maxfps", .min = 125, .max = 250, .value = 250 },
    { .name = "rate", .min = 25000, .max = 25000, .value = 25000 },
    { .name = "snaps", .min = 40, .max = 40, .value = 40 },
    { .name = "cl_maxpackets", .min = 125, .max = 125, .value = 125 },
    { .name = "sc_enable", .min = 0, .max = 0, .value = 0 },
    { .name = "fx_sort", .min = 1, .max = 1, .value = 1 },
    { .name = "mss_q3fs", .min = 1, .max = 1, .value = 1 }
};

void Cod2x_Frame(int active, int demo)
{
    unsigned int i;
    int competitive;
    cod2x_playing = active;
    cod2x_demo = demo;
    competitive = Cod2x_Competitive();
    /* CoD2x src/mss32/competitive.cpp:54-135. */
    for (i = 0; i < sizeof(cod2x_settings) / sizeof(cod2x_settings[0]); ++i) {
        dvar_t *var;
        if (competitive || (i == 0 && Cod2x_FPSLimited())) {
            if (!cod2x_settings[i].var) {
                cod2x_settings[i].var = Dvar_FindVar(cod2x_settings[i].name);
                if (!cod2x_settings[i].var)
                    continue;
                cod2x_settings[i].domain = cod2x_settings[i].var->domain;
                cod2x_settings[i].type = cod2x_settings[i].var->type;
            }
            var = (dvar_t *)cod2x_settings[i].var;
            {
                int wasBool = var->type == 0;
                var->type = 5; /* DVAR_TYPE_INT */
                var->domain.integer.min = cod2x_settings[i].min;
                var->domain.integer.max = cod2x_settings[i].max;
                if (wasBool || var->current.integer < cod2x_settings[i].min ||
                    var->current.integer > cod2x_settings[i].max)
                    Dvar_SetInt(var, cod2x_settings[i].value);
            }
        } else if (cod2x_settings[i].var) {
            var = (dvar_t *)cod2x_settings[i].var;
            var->domain = cod2x_settings[i].domain;
            var->type = cod2x_settings[i].type;
            cod2x_settings[i].var = 0;
        }
    }
}

void Cod2x_Disconnect(void)
{
    Cod2x_Frame(0, 0);
    Cod2x_ResetAnimation();
#if defined(COD2_X64) && COD2_X64 && !defined(DEDICATED)
    Cod2x_IwdSystemInfo("");
#endif
    if (cod2x_game)
        Dvar_SetInt(cod2x_game, 0);
    if (cod2x_competitive)
        Dvar_SetBool(cod2x_competitive, 0);
    if (cod2x_fpsLimit)
        Dvar_SetBool(cod2x_fpsLimit, 0);
}
#endif
