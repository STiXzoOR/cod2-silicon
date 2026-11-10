#include "cod2_feature_config.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "common_types.h"
#include "cod2x.h"
#include "cod2x_features.h"
#include "cod2x_pose.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <sched.h>
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
extern void Com_Error(int, const char *, ...);

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

/* ---- iw_CoD2x_01.iwd (CoD2x src/shared/iwd.cpp) ----------------------------
   CoD2x carries its data IWD inside its mss32.dll and writes it to main/ when the
   file system starts. This build does not carry it: it is copied from the
   player's own CoD2x mss32.dll in the game folder, as tools/cod2x/extract_iwd.py
   does, and nothing is downloaded. */
#if defined(_WIN32)
#define COD2X_IWD_SEPARATOR "\\"
#else
#define COD2X_IWD_SEPARATOR "/"
#endif
#define COD2X_IWD_MAX_DLL (256u * 1024 * 1024)
#define COD2X_IWD_MAX (64u * 1024 * 1024)

static unsigned int Cod2x_Le16(const unsigned char *p)
{
    return (unsigned int)p[0] | (unsigned int)p[1] << 8;
}

static unsigned int Cod2x_Le32(const unsigned char *p)
{
    return Cod2x_Le16(p) | Cod2x_Le16(p + 2) << 16;
}

/* A ZIP whose central directory names CoD2x's menu and radar arrow. */
static int Cod2x_IwdArchive(const unsigned char *zip, size_t size, size_t directory, size_t directorySize,
                            unsigned int count)
{
    size_t position = directory, end = directory + directorySize;
    unsigned long long expanded = 0;
    int menu = 0, arrow = 0;
    unsigned int i;
    for (i = 0; i < count; ++i) {
        char name[256];
        unsigned int nameLength, extra, comment, k;
        if (position + 46 > end || end > size || Cod2x_Le32(zip + position) != 0x02014b50u)
            return 0;
        expanded += Cod2x_Le32(zip + position + 24);
        nameLength = Cod2x_Le16(zip + position + 28);
        extra = Cod2x_Le16(zip + position + 30);
        comment = Cod2x_Le16(zip + position + 32);
        if (position + 46 + nameLength > end)
            return 0;
        if (nameLength < sizeof(name)) {
            for (k = 0; k < nameLength; ++k) {
                char c = (char)zip[position + 46 + k];
                name[k] = c == '\\' ? '/' : (char)tolower((unsigned char)c);
            }
            name[nameLength] = '\0';
            menu |= !strcmp(name, "ui_mp/main.menu");
            arrow |= !strcmp(name, "materials/radar_player_arrow");
        }
        position += 46 + nameLength + extra + comment;
    }
    return menu && arrow && expanded <= COD2X_IWD_MAX;
}

/* The one CoD2x IWD embedded in a CoD2x mss32.dll: found by its end-of-directory
   record, as the extraction tool does. */
static int Cod2x_IwdFromDll(const unsigned char *data, size_t size, size_t *start, size_t *length)
{
    size_t position, found = 0;
    if (size < 64 || data[0] != 'M' || data[1] != 'Z' || Cod2x_Le32(data + 60) > size - 4 ||
        memcmp(data + Cod2x_Le32(data + 60), "PE\0\0", 4))
        return 0;
    for (position = 0; position + 22 <= size; ++position) {
        size_t directorySize, directory, begin, end;
        if (memcmp(data + position, "PK\5\6", 4))
            continue;
        directorySize = Cod2x_Le32(data + position + 12);
        directory = Cod2x_Le32(data + position + 16);
        end = position + 22 + Cod2x_Le16(data + position + 20);
        if (Cod2x_Le16(data + position + 4) || Cod2x_Le16(data + position + 6) ||
            Cod2x_Le16(data + position + 8) != Cod2x_Le16(data + position + 10) ||
            directorySize + directory > position || end > size)
            continue;
        begin = position - directorySize - directory;
        if (memcmp(data + begin, "PK\3\4", 4) || end - begin > COD2X_IWD_MAX ||
            !Cod2x_IwdArchive(data + begin, end - begin, directory, directorySize, Cod2x_Le16(data + position + 10)))
            continue;
        if (found++)
            return 0; /* more than one: not a release the tool would accept either */
        *start = begin;
        *length = end - begin;
    }
    return found != 0;
}

static unsigned char *Cod2x_ReadFile(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    unsigned char *data = NULL;
    long length;
    if (!file)
        return NULL;
    if (!fseek(file, 0, SEEK_END) && (length = ftell(file)) > 0 && (unsigned long)length <= COD2X_IWD_MAX_DLL &&
        !fseek(file, 0, SEEK_SET) && (data = malloc((size_t)length)) != NULL &&
        fread(data, 1, (size_t)length, file) == (size_t)length) {
        *size = (size_t)length;
    } else {
        free(data);
        data = NULL;
    }
    fclose(file);
    return data;
}

/* Newer and test builds of CoD2x leave iw_CoD2x_NN with NN above 01, or
   iw_CoD2x_01_<version>.iwd; CoD2x 1.4.6.8 deletes them. */
static int Cod2x_IwdObsolete(const char *name)
{
    size_t length = strlen(name);
    if (length < 4 || strcasecmp(name + length - 4, ".iwd") || strncasecmp(name, "iw_CoD2x_", 9) ||
        !isdigit((unsigned char)name[9]) || !isdigit((unsigned char)name[10]))
        return 0;
    return (name[9] - '0') * 10 + (name[10] - '0') > 1 || name[11] != '.';
}

static void Cod2x_IwdCleanup(const char *base)
{
    char directory[1024], path[1280];
    struct dirent *entry;
    struct stat st;
    DIR *dir;
    snprintf(directory, sizeof(directory), "%s" COD2X_IWD_SEPARATOR "main", base);
    dir = opendir(directory);
    if (!dir)
        return;
    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        snprintf(path, sizeof(path), "%s" COD2X_IWD_SEPARATOR "%s", directory, entry->d_name);
        if ((!stat(path, &st) && S_ISDIR(st.st_mode)) || !Cod2x_IwdObsolete(entry->d_name))
            continue;
        if (remove(path) == 0)
            Com_Printf("Deleted CoD2x IWD: %s\n", path);
        else
            Com_Error(0, "Failed to delete CoD2x IWD: %s (errno=%d)\n", path, errno);
    }
    closedir(dir);
}

void Cod2x_IwdStartup(const char *base)
{
    char dll[1024], target[1024];
    unsigned char *data;
    size_t size = 0, start = 0, length = 0, written;
    struct stat st;
    FILE *file;
    if (!base || !*base) {
        Com_Printf("CoD2x IWD cleanup: fs_basepath not set, skipping.\n");
        return;
    }
    Cod2x_IwdCleanup(base);
    snprintf(dll, sizeof(dll), "%s" COD2X_IWD_SEPARATOR "mss32.dll", base);
    data = Cod2x_ReadFile(dll, &size);
    if (!data)
        return;
    if (!Cod2x_IwdFromDll(data, size, &start, &length)) {
        free(data);
        return;
    }
    snprintf(target, sizeof(target), "%s" COD2X_IWD_SEPARATOR "main" COD2X_IWD_SEPARATOR "iw_CoD2x_01.iwd", base);
    if (!stat(target, &st)) {
        if ((size_t)st.st_size == length) {
            free(data);
            return;
        }
        if (remove(target)) {
            free(data);
            Com_Error(0, "Error while extracting IWD file to '%s'. Error while deleting existing file: %s\n", target,
                      strerror(errno));
            return;
        }
    }
    file = fopen(target, "wb");
    if (!file) {
        free(data);
        Com_Error(0, "Error while extracting IWD file to '%s'. Error while opening file: %s\n", target,
                  strerror(errno));
        return;
    }
    written = fwrite(data + start, 1, length, file);
    fclose(file);
    free(data);
    if (written != length)
        Com_Error(0, "Error while extracting IWD file to '%s'. Error while writing to file: %s\n", target,
                  strerror(errno));
}
#endif
