#include "cod2_feature_config.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "common_types.h"
#include "cod2x.h"
#include "cod2x_features.h"
#include "cod2x_pose.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#endif
#if !defined(_WIN32)
#include <unistd.h>
#endif

extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterFloat(const char *, float, float, float, unsigned short);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern void Dvar_SetInt(const dvar_t *, int);
extern void Dvar_SetFloat(const dvar_t *, float);
extern void Dvar_SetBool(const dvar_t *, unsigned char);
extern void Dvar_SetString(const dvar_t *, const char *);
extern void Cmd_AddCommand(const char *, void (*)(void));
extern int Cmd_Argc(void);
extern char *Cmd_Argv(int);
extern void Com_Printf(const char *, ...);

static const dvar_t *thirdPersonMode, *debugBullets, *printDoubleColors;
static const dvar_t *drawSpectatedName, *drawCompass, *compassOffsetX, *compassOffsetY;
/* The CoD2x release whose client behaviour this implements. */
#define COD2X_APP_VERSION "1.4.6.8"

static const dvar_t *matchLogin, *affinity;
static int playingDemo, lastClientState = -1;
#if defined(_WIN32)
/* src/win32/win_system.c */
int Sys_Cod2xProcessorCount(void);
int Sys_Cod2xSetAffinity(unsigned long long mask, unsigned long *error);
#endif

static void Cod2x_IncreaseDecrease_f(void)
{
    const dvar_t *var;
    int direction = strcmp(Cmd_Argv(0), "decrease") ? 1 : -1;
    /* CoD2x prints the same usage line for both commands. */
    if (Cmd_Argc() != 2) {
        Com_Printf("%s <variablename> : increase value\n", Cmd_Argv(0));
        return;
    }
    var = Dvar_FindVar(Cmd_Argv(1));
    if (!var) {
        Com_Printf("%s not found\n", Cmd_Argv(1));
        return;
    }
    /* An internal set, as in CoD2x: the dvar's own domain still clamps the value. */
    if (var->type == 5)
        Dvar_SetInt(var, Cod2x_StepInt(var->current.integer, direction));
    else if (var->type == 1)
        Dvar_SetFloat(var, Cod2x_StepFloat(var->current.value, direction));
    else
        Com_Printf("%s is not an int or float\n", Cmd_Argv(1));
}

static void Cod2x_MatchUsage(void)
{
    Com_Printf("USAGE: match <command> <options>\n");
    Com_Printf("Client:\n");
    Com_Printf("  match login <hash>\n");
    Com_Printf("    <hash> - The hash to login with to the match server; will be removed after disconnect\n");
    Com_Printf("Server:\n");
    Com_Printf("  match create <endpoint>\n");
    Com_Printf("    <endpoint> - URL for getting match data\n");
    Com_Printf("    Example: match create http://web.com/matches/MATCHID1\n");
    Com_Printf("  match status\n");
    Com_Printf("    Displays the current match status\n");
    Com_Printf("  match upload\n");
    Com_Printf("    Forces upload of match data\n");
    Com_Printf("  match cancel\n");
    Com_Printf("    Cancels the current match\n");
}

static void Cod2x_Match_f(void)
{
    const dvar_t *dedicated;
    const char *hash;
    if (Cmd_Argc() < 2 || strcasecmp(Cmd_Argv(1), "login")) {
        /* The server commands belong to the match service, which this client does not run. */
        Cod2x_MatchUsage();
        return;
    }
    dedicated = Dvar_FindVar("dedicated");
    if (dedicated && dedicated->current.integer > 0) {
        Com_Printf("Match login command is only available for clients.\n");
        return;
    }
    hash = Cmd_Argv(2);
    if (!*hash) {
        Com_Printf("Please provide a hash to login.\n");
        return;
    }
    Dvar_SetString(matchLogin, hash);
}

static int Cod2x_ProcessorCount(void)
{
#if defined(_WIN32)
    return Sys_Cod2xProcessorCount();
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (int)count : 1;
#endif
}

#if defined(_WIN32) || defined(__linux__)
static void Cod2x_SetAffinity(unsigned long long mask)
{
    unsigned long long bits;
    unsigned long error = 0;
    int cores = 0, applied;
#if defined(_WIN32)
    applied = Sys_Cod2xSetAffinity(mask, &error);
#else
    cpu_set_t set;
    DIR *tasks;
    struct dirent *task;
    int cpu;
    CPU_ZERO(&set);
    for (cpu = 0; cpu < 64 && cpu < CPU_SETSIZE; ++cpu)
        if (mask & (1ULL << cpu))
            CPU_SET(cpu, &set);
    /* The mask applies to the whole process, as on Windows: every thread, not only this one. */
    applied = !sched_setaffinity(0, sizeof(set), &set);
    if (!applied)
        error = (unsigned long)errno;
    if (applied && (tasks = opendir("/proc/self/task")) != NULL) {
        while ((task = readdir(tasks)) != NULL)
            if (task->d_name[0] != '.')
                sched_setaffinity((pid_t)atoi(task->d_name), sizeof(set), &set);
        closedir(tasks);
    }
#endif
    if (!applied) {
        Com_Printf("CPU Affinity: Error setting affinity: %lu\n", error);
        return;
    }
    for (bits = mask; bits; bits >>= 1)
        cores += (int)(bits & 1);
    Com_Printf("CPU Affinity: Affinity set to %d core%s (mask 0x%08X).\n", cores, cores == 1 ? "" : "s",
               (unsigned int)mask);
}
#else
static void Cod2x_SetAffinity(unsigned long long mask)
{
    /* macOS has no process CPU affinity: com_affinity is accepted and changes nothing. */
    (void)mask;
}
#endif

static void Cod2x_AffinityFrame(void)
{
    int cores;
    if (!affinity || !affinity->modified)
        return;
    ((dvar_t *)affinity)->modified = 0;
    cores = affinity->current.integer;
    if (cores == 0) {
        Com_Printf("CPU Affinity disabled.\n");
        return;
    }
    if (cores == -1)
        cores = affinity->domain.integer.max;
    Cod2x_SetAffinity(cores >= 64 ? ~0ULL : (1ULL << cores) - 1);
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
    /* -1 uses every processor, 0 leaves the affinity alone, n uses the first n. */
    affinity = Dvar_RegisterInt("com_affinity", -1, -1, Cod2x_ProcessorCount(), 0x1001);
    ((dvar_t *)affinity)->modified = 1;
}

void Cod2x_FeaturesFrame(int active, int demo)
{
    (void)active;
    playingDemo = demo;
    Cod2x_AffinityFrame();
}

void Cod2x_FeaturesClientState(int state)
{
    /* CoD2x reports the server's revision whenever g_cod2x changes, and again
       when a 1.3 server's map becomes active. */
    dvar_t *game = (dvar_t *)Dvar_FindVar("g_cod2x");
    int changed = state != lastClientState, previous = lastClientState;
    lastClientState = state;
    if (changed && state == CA_CONNECTED) {
        /* A server may have turned the mouse off with m_enable; a new connection turns it back on. */
        const dvar_t *mouse = Dvar_FindVar("m_enable");
        if (mouse)
            Dvar_SetBool(mouse, 1);
    }
    if (!game)
        return;
    if (game->current.integer > 0 && changed && state <= CA_CONNECTED) {
        Dvar_SetInt(game, 0);
        game->modified = 1;
    }
    if (game->current.integer == 0 && changed && state == CA_ACTIVE && previous <= CA_PRIMED)
        game->modified = 1;
    if (!game->modified)
        return;
    game->modified = 0;
    Com_Printf("---------------------------------------------------------------------------------\n");
    if (game->current.integer == 0) {
        Com_Printf("CoD2x: Changes turned off, using legacy CoD2 1.3\n");
    } else {
        Com_Printf("CoD2x: Changes turned on, using changes according to server version 1.4.%d.x\n",
                   game->current.integer);
        if (game->current.integer != COD2X_REVISION)
            Com_Printf("CoD2x: ^3Server is running older version 1.4.%d.x, your version is %s\n",
                       game->current.integer, COD2X_APP_VERSION);
    }
    Com_Printf("---------------------------------------------------------------------------------\n");
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
