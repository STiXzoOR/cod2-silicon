#include "cod2_feature_config.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "common_types.h"
#include "cod2x.h"
#include "cod2x_features.h"
#include "cod2x_pose.h"
#include "cod2x_policy.h"
#include <stdarg.h>
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

#if defined(_WIN32)
#define COD2X_IWD_SEPARATOR "\\"
#else
#define COD2X_IWD_SEPARATOR "/"
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
extern char *Info_ValueForKey(const char *, const char *);

static const dvar_t *thirdPersonMode, *debugBullets;
static const dvar_t *drawSpectatedName, *drawCompass, *compassOffsetX, *compassOffsetY;
/* The CoD2x release whose client behaviour this implements. */
#define COD2X_APP_VERSION "1.4.6.8"

static const dvar_t *matchLogin, *hideServerIp, *messageOffsetX, *messageOffsetY, *messageScale;
static int ipHidden;
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

/* com_autoaffinity, CoD2x mss32/affinity.cpp: every core, set once at start. CoD2x
   registers it before the config or the command line is read, so it always applies. */
#if defined(_WIN32) || defined(__linux__)
static void Cod2x_AutoAffinity(void)
{
    int count = Cod2x_ProcessorCount(), applied;
    unsigned long long mask = count >= 64 ? ~0ULL : (1ULL << count) - 1;
    unsigned long error = 0;
#if defined(_WIN32)
    applied = Sys_Cod2xSetAffinity(mask, &error);
#else
    cpu_set_t set;
    DIR *tasks;
    struct dirent *task;
    int cpu;
    CPU_ZERO(&set);
    for (cpu = 0; cpu < count && cpu < CPU_SETSIZE; ++cpu)
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
    if (applied)
        Com_Printf("CPU Affinity: set to all %lu cores (mask 0x%08X).\n", (unsigned long)count, (unsigned int)mask);
    else
        Com_Printf("CPU Affinity: error setting affinity: %lu\n", error);
}
#else
static void Cod2x_AutoAffinity(void)
{
    /* macOS has no process CPU affinity: com_autoaffinity is accepted and changes nothing. */
}
#endif

/* ---- fs_processedList (CoD2x src/shared/iwd.cpp) ------------------------- */
typedef struct {
    char *text;
    size_t length, capacity;
} Cod2xLog;

static Cod2xLog processedCurrent, processedPrevious;

static void Cod2x_IwdLog(const char *format, ...)
{
    char line[1024];
    size_t length;
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    Com_Printf("%s", line);
    length = strlen(line);
    if (processedCurrent.length + length + 1 > processedCurrent.capacity) {
        size_t capacity = processedCurrent.capacity ? processedCurrent.capacity * 2 : 4096;
        char *text;
        while (capacity < processedCurrent.length + length + 1)
            capacity *= 2;
        text = realloc(processedCurrent.text, capacity);
        if (!text)
            return;
        processedCurrent.text = text;
        processedCurrent.capacity = capacity;
    }
    memcpy(processedCurrent.text + processedCurrent.length, line, length + 1);
    processedCurrent.length += length;
}

/* The directory as the platform writes paths: CoD2x shows Windows paths with backslashes. */
static const char *Cod2x_IwdLogPath(const char *directory, char *buffer, size_t size)
{
#if defined(_WIN32)
    size_t i;
    snprintf(buffer, size, "%s", directory);
    for (i = 0; buffer[i]; ++i)
        if (buffer[i] == '/')
            buffer[i] = '\\';
    return buffer;
#else
    (void)buffer;
    (void)size;
    return directory;
#endif
}

void Cod2x_IwdLogRotate(void)
{
    free(processedPrevious.text);
    processedPrevious = processedCurrent;
    memset(&processedCurrent, 0, sizeof(processedCurrent));
}

void Cod2x_IwdLogDirectory(const char *directory, const char *basepath, const char *game, int version,
                           const char *names)
{
    const char *p = names ? names : "";
    char path[1024];
    int count = 0;
    Cod2x_IwdLog("Processing directory ^9%s^7\n", Cod2x_IwdLogPath(directory, path, sizeof(path)));
    Cod2x_IwdLog("  fs_basepath:        ^9%s^7\n", basepath && *basepath ? basepath : "(empty)");
    Cod2x_IwdLog("  fs_game:            ^9%s^7\n", game && *game ? game : "(empty)");
    Cod2x_IwdLog("  CoD2x version:      ^9%d^7\n", version);
    /* Every name the server requires, quoted: these are what the files are matched against. */
    for (;;) {
        while (*p && isspace((unsigned char)*p))
            ++p;
        if (!*p)
            break;
        ++count;
        while (*p && !isspace((unsigned char)*p))
            ++p;
    }
    Cod2x_IwdLog("  Required by server: ^9%d file(s)^7\n", count);
    for (p = names ? names : ""; *p;) {
        const char *start;
        while (*p && isspace((unsigned char)*p))
            ++p;
        start = p;
        while (*p && !isspace((unsigned char)*p))
            ++p;
        if (p > start)
            Cod2x_IwdLog("    ^9\"%.*s\"^7\n", (int)(p - start), start);
    }
}

void Cod2x_IwdLogFile(const char *directory, const char *file, int reason)
{
    char why[300], path[1024];
    switch (reason) {
    case COD2X_IWD_STOCK: snprintf(why, sizeof(why), "original IW / localized file"); break;
    case COD2X_IWD_MOVIE: snprintf(why, sizeof(why), "demo movie folder"); break;
    case COD2X_IWD_REQUIRED: snprintf(why, sizeof(why), "required by server/demo: %s", Cod2x_IwdRequiredName()); break;
    case COD2X_IWD_LATEST_ZPAM: snprintf(why, sizeof(why), "latest ZPAM IWD file"); break;
    case COD2X_IWD_LATEST_ZPAM_MAPS: snprintf(why, sizeof(why), "latest ZPAM_MAPS IWD file"); break;
    case COD2X_IWD_GAME: snprintf(why, sizeof(why), "fs_game folder"); break;
    default: snprintf(why, sizeof(why), "not required"); break;
    }
    Cod2x_IwdLog(reason ? "  ^2[LOADED]^7 %s" COD2X_IWD_SEPARATOR "%s ^9(%s)^7\n"
                        : "  ^1[IGNORE]^7 %s" COD2X_IWD_SEPARATOR "%s ^9(%s)^7\n",
                 Cod2x_IwdLogPath(directory, path, sizeof(path)), file, why);
}

static void Cod2x_ProcessedList_f(void)
{
    Com_Printf("------------------ fs_processedList ------------------\n");
    Com_Printf("=== Previous session ===\n");
    Com_Printf("%s", processedPrevious.length ? processedPrevious.text : "(empty)\n");
    Com_Printf("\n\n=== Current session ===\n");
    Com_Printf("%s", processedCurrent.length ? processedCurrent.text : "(empty)\n");
    Com_Printf("------------------------------------------------------\n");
}

void Cod2x_FeaturesInit(void)
{
    Cod2x_PoseInit();
    /* CoD2x cgame.cpp:65 and drawing.cpp:270-279 describe these controls. */
    thirdPersonMode = Dvar_RegisterInt("cg_thirdPersonMode", 0, 0, 1, 0x1080);
    debugBullets = Dvar_RegisterBool("cg_debugBullets", 0, 0x1080);
    drawSpectatedName = Dvar_RegisterBool("cg_drawSpectatedPlayerName", 1, 0x1000);
    drawCompass = Dvar_RegisterBool("cg_drawCompass", 1, 0x1000);
    compassOffsetX = Dvar_RegisterFloat("cg_hudCompassOffsetX", 0, -640, 640, 0x1000);
    compassOffsetY = Dvar_RegisterFloat("cg_hudCompassOffsetY", 0, -480, 480, 0x1000);
    matchLogin = Dvar_RegisterString("match_login", "", 0x12);
    /* CoD2x cgame.cpp: hides IP addresses on screen, for streaming. */
    hideServerIp = Dvar_RegisterBool("cl_hideServerIp", 0, 0x1001);
    /* CoD2x text.cpp: position and size of the game messages and killfeed, in 640x480 units. */
    messageOffsetX = Dvar_RegisterFloat("cg_gameMessageOffsetX", 0, -640, 640, 0x1001);
    messageOffsetY = Dvar_RegisterFloat("cg_gameMessageOffsetY", 0, -480, 480, 0x1001);
    messageScale = Dvar_RegisterFloat("cg_gameMessageScale", 1, .5f, 3, 0x1001);
    Cmd_AddCommand("increase", Cod2x_IncreaseDecrease_f);
    Cmd_AddCommand("decrease", Cod2x_IncreaseDecrease_f);
    Cmd_AddCommand("match", Cod2x_Match_f);
    Cmd_AddCommand("fs_processedList", Cod2x_ProcessedList_f);
    Dvar_RegisterBool("com_autoaffinity", 1, 0x1001);
    Cod2x_AutoAffinity();
}

void Cod2x_FeaturesFrame(int active, int demo)
{
    (void)active;
    playingDemo = demo;
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

/* ---- Text codes (CoD2x src/mss32/text.cpp, src/shared/shared.cpp) ----------
   Besides the vanilla ^0-^9, CoD2x reads $N, #N and @N as dark, light and deep
   shades of ten colour families, $* $- as rainbow on/off, @+ @= @- as bold,
   console and default font, and #_ #- as a background bar on/off. A doubled
   prefix ("$$11", "@@++") is the same code. Each code is kept as one character:
   '0'-'9' vanilla, 'a'-'t' and 'A'-'J' the shades, and * ! + = - _ . the styles. */
static char Cod2x_StyleCode(char prefix, char symbol)
{
    static const char styles[][3] = {
        {'$', '*', '*'}, {'$', '-', '!'}, {'@', '+', '+'}, {'@', '=', '='},
        {'@', '-', '-'}, {'#', '_', '_'}, {'#', '-', '.'},
    };
    size_t i;
    for (i = 0; i < sizeof(styles) / sizeof(styles[0]); ++i)
        if (styles[i][0] == prefix && styles[i][1] == symbol)
            return styles[i][2];
    return 0;
}

/* Dark ($), light (#) and deep (@) shades of brown, red, green, yellow, blue, cyan,
   pink, grey, orange and violet. */
static const float cod2xShades[30][3] = {
    {0.25f, 0.13f, 0.07f}, {0.45f, 0.05f, 0.10f}, {0.05f, 0.35f, 0.15f}, {0.50f, 0.47f, 0.10f},
    {0.05f, 0.10f, 0.40f}, {0.00f, 0.45f, 0.45f}, {0.45f, 0.10f, 0.40f}, {0.30f, 0.30f, 0.30f},
    {0.60f, 0.25f, 0.05f}, {0.25f, 0.10f, 0.50f},
    {0.80f, 0.62f, 0.42f}, {1.00f, 0.50f, 0.45f}, {0.65f, 0.95f, 0.20f}, {1.00f, 0.95f, 0.65f},
    {0.45f, 0.70f, 1.00f}, {0.70f, 0.95f, 1.00f}, {1.00f, 0.65f, 0.85f}, {0.80f, 0.80f, 0.80f},
    {1.00f, 0.75f, 0.55f}, {0.75f, 0.65f, 1.00f},
    {0.55f, 0.30f, 0.12f}, {0.80f, 0.05f, 0.20f}, {0.00f, 0.72f, 0.40f}, {0.95f, 0.75f, 0.10f},
    {0.15f, 0.35f, 0.95f}, {0.10f, 0.80f, 0.85f}, {0.95f, 0.15f, 0.60f}, {0.50f, 0.50f, 0.50f},
    {1.00f, 0.50f, 0.00f}, {0.55f, 0.15f, 0.95f},
};

/* The colour of a shade code ('a'-'t', 'A'-'J'); 0 for any other code. */
int Cod2x_ShadeColor(char code, float *rgb)
{
    int index = code >= 'a' && code <= 't' ? code - 'a' : code >= 'A' && code <= 'J' ? 20 + code - 'A' : -1;
    if (index < 0)
        return 0;
    rgb[0] = cod2xShades[index][0];
    rgb[1] = cod2xShades[index][1];
    rgb[2] = cod2xShades[index][2];
    return 1;
}

int Cod2x_IsStyleCode(char code)
{
    return code && strchr("*!+=-_.", code) != NULL;
}

int Cod2x_TextCode(const char *p, char *code)
{
    int shade;
    if (!p[0] || !p[1])
        return 0;
    if (p[0] == '^') {
        if (p[1] < '0' || p[1] > '9')
            return 0;
        *code = p[1];
        return 2;
    }
    shade = p[0] == '$' ? 0 : p[0] == '#' ? 1 : p[0] == '@' ? 2 : -1;
    if (shade < 0)
        return 0;
    if (p[1] == p[0] && p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9') {
        int index = shade * 10 + p[3] - '0';
        *code = (char)(index < 20 ? 'a' + index : 'A' + index - 20);
        return 4;
    }
    if (p[1] >= '0' && p[1] <= '9') {
        int index = shade * 10 + p[1] - '0';
        *code = (char)(index < 20 ? 'a' + index : 'A' + index - 20);
        return 2;
    }
    if (p[1] == p[0] && p[2] && p[3] == p[2] && Cod2x_StyleCode(p[0], p[2])) {
        *code = Cod2x_StyleCode(p[0], p[2]);
        return 4;
    }
    if (Cod2x_StyleCode(p[0], p[1])) {
        *code = Cod2x_StyleCode(p[0], p[1]);
        return 2;
    }
    return 0;
}

/* A chat line is team colour + name + "^7: " + message. The name loses its codes,
   so it shows in the team colour; the team colour and the message keep theirs. */
void Cod2x_StripChatName(char *line)
{
    char *end = strstr(line, "^7: "), *out = line, code;
    const char *p = line;
    if (!end)
        return;
    if (p[0] == '^' && Cod2x_TextCode(p, &code) == 2) {
        out += 2;
        p += 2;
    }
    while (p < end) {
        int length = Cod2x_TextCode(p, &code);
        if (length && p + length <= end) {
            p += length;
            continue;
        }
        *out++ = *p++;
    }
    memmove(out, end, strlen(end) + 1);
}

/* The console keeps a code as the colour byte of the characters after it (see
   r_font.c): 0-9 vanilla, or 0x40 + colour + 40 * rainbow, colours 10-39 being the
   shades. Font and bar codes have no effect there; "#_" goes back to the default.
   Returns the code's length, 0 when the text has no code here. */
int Cod2x_ConsoleCode(const char *text, int *color)
{
    char code;
    int length = Cod2x_TextCode(text, &code), value, index = 7, rainbow = 0;
    if (!length)
        return 0;
    value = *color & 0xff;
    if (value <= 9) {
        index = value;
    } else if (value >= 0x40 && value < 0x40 + 80) {
        index = (value - 0x40) % 40;
        rainbow = (value - 0x40) / 40;
    }
    if (code == '7') {
        index = 7;
        rainbow = 0;
    } else if (code >= '0' && code <= '9') {
        index = code - '0';
    } else if (code >= 'a' && code <= 't') {
        index = 10 + code - 'a';
    } else if (code >= 'A' && code <= 'J') {
        index = 30 + code - 'A';
    } else if (code == '*') {
        rainbow = 1;
    } else if (code == '!') {
        rainbow = 0;
    } else if (code == '_') {
        index = 7;
    }
    *color = !rainbow && index <= 9 ? index : 0x40 + index + 40 * rainbow;
    return length;
}

/* "&&1" in a chat message's arguments is written "^7&^7&^7", so a name like
   "&&1foo" is not read as the placeholder for the name. Returns whether it changed. */
int Cod2x_EscapeChatPlaceholder(const char *text, char *out, size_t size)
{
    int changed = 0, arguments = strchr(text, 0x14) == NULL;
    size_t length = 0, i;
    for (i = 0; text[i] && length < size - 1; ++i) {
        if (text[i] == 0x14)
            arguments = 1;
        if (arguments && text[i] == '&' && text[i + 1] == '&' && text[i + 2] == '1' &&
            (text[i + 3] < '0' || text[i + 3] > '9')) {
            const char *escaped = "^7&^7&^7";
            while (*escaped && length < size - 1)
                out[length++] = *escaped++;
            ++i;
            changed = 1;
            continue;
        }
        out[length++] = text[i];
    }
    out[length] = '\0';
    return changed;
}

/* Vanilla double colours ("^^11Name") drawn as the colour they stand for ("^1Name"). */
const char *Cod2x_ResolveDoubleColors(const char *text, char *buffer, size_t size)
{
    const char *p;
    size_t length = 0;
    int changed = 0;
    if (!text || !strstr(text, "^^"))
        return text;
    for (p = text; *p;) {
        if (length + 2 >= size)
            return text;
        if (p[0] == '^' && p[1] == '^' && p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9') {
            buffer[length++] = '^';
            buffer[length++] = p[3];
            p += 4;
            changed = 1;
            continue;
        }
        buffer[length++] = *p++;
    }
    buffer[length] = '\0';
    return changed ? buffer : text;
}

/* ---- Hidden IP addresses (CoD2x cgame.cpp and text.cpp) -------------------
   With cl_hideServerIp, or sv_hideServerIp "1" in the connected server's info,
   the scoreboard leaves the server address out and every IPv4 address drawn on
   screen has its digits drawn as '*'. Nothing stored changes, so turning it off
   brings the text back. */
void Cod2x_HideServerIpFrame(const char *serverinfo)
{
    ipHidden = (hideServerIp && hideServerIp->current.enabled) ||
               (serverinfo && Info_ValueForKey(serverinfo, "sv_hideServerIp")[0] == '1');
}

int Cod2x_HideServerIp(void)
{
    return ipHidden;
}

static int Cod2x_IpDigit(char c)
{
    return c >= '0' && c <= '9';
}

static int Cod2x_IpAlnum(char c)
{
    return Cod2x_IpDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Length of the address "a.b.c.d[:port]" at text + start, or 0. Octets are
   0-255; an address of single digits only reads as a version ("1.4.6.8"). */
static int Cod2x_IpLength(const char *text, int len, int start)
{
    int p = start, octet, longer = 0;
    for (octet = 0; octet < 4; ++octet) {
        int first = p, value = 0;
        while (p < len && Cod2x_IpDigit(text[p]) && p - first < 3)
            value = value * 10 + text[p++] - '0';
        if (p == first || value > 255 || (p < len && Cod2x_IpDigit(text[p])))
            return 0;
        longer |= p - first > 1;
        if (octet < 3) {
            if (p >= len || text[p] != '.')
                return 0;
            ++p;
        }
    }
    if (p < len && (Cod2x_IpAlnum(text[p]) || (text[p] == '.' && p + 1 < len && Cod2x_IpDigit(text[p + 1]))))
        return 0;
    if (!longer)
        return 0;
    if (p + 1 < len && text[p] == ':' && Cod2x_IpDigit(text[p + 1]))
        for (++p; p < len && Cod2x_IpDigit(text[p]); ++p)
            ;
    return p - start;
}

/* Masks text in place; returns whether anything changed. The host of every
   "connect" command on the line is masked too, so a typed name or a partial
   address does not show either. */
static int Cod2x_MaskIpAddresses(char *text, int len)
{
    int changed = 0, p, i;
    char previous = ' ';
    for (p = 0; p < len;) {
        while (p < len && (text[p] == ']' || text[p] == '/' || text[p] == '\\' || text[p] == ' '))
            ++p;
        if (len - p >= 8 && !strncasecmp(text + p, "connect ", 8)) {
            for (p += 8; p < len && text[p] == ' '; ++p)
                ;
            for (; p < len && text[p] != ' ' && text[p] != ';'; ++p)
                if (Cod2x_IpAlnum(text[p])) {
                    text[p] = '*';
                    changed = 1;
                }
        }
        while (p < len && text[p] != ';')
            ++p;
        ++p;
    }
    for (i = 0; i < len;) {
        int length;
        /* A colour code is not part of an address and separates like a space. */
        if ((text[i] == '^' || text[i] == '$' || text[i] == '#' || text[i] == '@') && i + 1 < len) {
            i += 2;
            previous = ' ';
            continue;
        }
        length = !Cod2x_IpAlnum(previous) && previous != '.' && Cod2x_IpDigit(text[i]) ? Cod2x_IpLength(text, len, i) : 0;
        if (!length) {
            previous = text[i++];
            continue;
        }
        previous = text[i + length - 1];
        for (; length > 0; --length, ++i)
            if (Cod2x_IpDigit(text[i]))
                text[i] = '*';
        changed = 1;
    }
    return changed;
}

const char *Cod2x_HideIpText(const char *text, char *buffer, size_t size)
{
    size_t len;
    if (!ipHidden || !text)
        return text;
    len = strlen(text);
    if (len >= size)
        return text;
    memcpy(buffer, text, len + 1);
    return Cod2x_MaskIpAddresses(buffer, (int)len) ? buffer : text;
}

/* Console cells: the low byte is the character; the killfeed's control cells
   (types 10 and up) separate the text around them. */
const short *Cod2x_HideIpCells(const short *cells, int count, short *buffer, int size)
{
    char text[1024];
    int i;
    if (!ipHidden || !cells || count <= 0 || count > size || count > (int)sizeof(text))
        return cells;
    for (i = 0; i < count; ++i) {
        int type = (cells[i] >> 8) & 0xff;
        text[i] = type >= 10 && type < 0x40 ? '\1' : (char)(cells[i] & 0xff);
    }
    if (!Cod2x_MaskIpAddresses(text, count))
        return cells;
    for (i = 0; i < count; ++i) {
        buffer[i] = cells[i];
        if (text[i] == '*' && (char)(cells[i] & 0xff) != '*')
            buffer[i] = (short)((cells[i] & 0xff00) | '*');
    }
    return buffer;
}

/* The line height is whole (it also picks the font); the scale corrects the rest. */
void Cod2x_GameMessageLayout(int *x, int *y, int *charHeight, float *scale)
{
    float height = 12.0f * (messageScale ? messageScale->current.value : 1.0f);
    *charHeight = (int)(height + 0.5f);
    if (*charHeight < 1)
        *charHeight = 1;
    *scale = height / (float)*charHeight;
    if (messageOffsetX)
        *x += (int)floorf(messageOffsetX->current.value + 0.5f);
    if (messageOffsetY)
        *y += (int)floorf(messageOffsetY->current.value + 0.5f);
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
