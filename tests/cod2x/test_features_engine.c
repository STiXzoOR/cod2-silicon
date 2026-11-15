#include "common_types.h"
#include "cod2x.h"
#include "cod2x_features.h"
#include "cod2x_http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static dvar_t variables[32];
static char stringValues[32][2048];
static uint64_t serviceTicks;
static int failHttpStart, httpStarts, udpSends, resolveUpdate;
static int variableCount, argc, setterCalls;
static int stateLogs, stateDebugLogs, legacyNotices, versionNotices;
static char stateMessage[128];
static char *argv[3];
static struct { const char *name; void (*function)(void); } commands[4];
static int commandCount;

const dvar_t *Dvar_FindVar(const char *name)
{
    int i;
    for (i = 0; i < variableCount; ++i)
        if (!strcmp(variables[i].name, name))
            return &variables[i];
    return NULL;
}

const char *Dvar_GetString(const char *name)
{
    const dvar_t *var = Dvar_FindVar(name);
    return var && var->current.string ? var->current.string : "";
}

static dvar_t *add(const char *name, unsigned short flags, byte type)
{
    dvar_t *var = (dvar_t *)Dvar_FindVar(name);
    if (!var) {
        assert(variableCount < 32);
        var = &variables[variableCount++];
    }
    var->name = name;
    var->flags = flags;
    var->type = type;
    return var;
}

const dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, unsigned short flags)
{
    dvar_t *var = add(name, flags, 5);
    var->current.integer = value;
    var->domain.integer.min = min;
    var->domain.integer.max = max;
    return var;
}

const dvar_t *Dvar_RegisterBool(const char *name, unsigned char value, unsigned short flags)
{
    dvar_t *var = add(name, flags, 0);
    var->current.enabled = value;
    return var;
}

const dvar_t *Dvar_RegisterFloat(const char *name, float value, float min, float max, unsigned short flags)
{
    dvar_t *var = add(name, flags, 1);
    (void)min;
    (void)max;
    var->current.value = value;
    return var;
}

const dvar_t *Dvar_RegisterString(const char *name, const char *value, unsigned short flags)
{
    dvar_t *var = add(name, flags, 7);
    var->current.string = value;
    return var;
}

/* CoD2x's increase/decrease set the value internally, without the console's source checks. */
void Dvar_SetInt(const dvar_t *var, int value)
{
    ++setterCalls;
    if (((dvar_t *)var)->current.integer != value)
        ((dvar_t *)var)->modified = 1;
    ((dvar_t *)var)->current.integer = value;
}

void Dvar_SetFloat(const dvar_t *var, float value)
{
    ++setterCalls;
    ((dvar_t *)var)->current.value = value;
}

void Dvar_SetBool(const dvar_t *var, unsigned char value)
{
    ((dvar_t *)var)->current.enabled = value;
}

void Dvar_SetString(const dvar_t *var, const char *value)
{
    size_t index = (size_t)(var - variables);
    assert(index < 32);
    snprintf(stringValues[index], sizeof(stringValues[index]), "%s", value);
    ((dvar_t *)var)->current.string = stringValues[index];
}

void Cmd_AddCommand(const char *name, void (*function)(void))
{
    assert(commandCount < 4);
    commands[commandCount].name = name;
    commands[commandCount++].function = function;
}

int Cmd_Argc(void) { return argc; }
char *Cmd_Argv(int i) { return i < argc ? argv[i] : ""; }
void Com_Printf(const char *format, ...)
{
    if (strstr(format, "Changes turned off")) ++legacyNotices;
    if (strstr(format, "Changes turned on")) ++versionNotices;
}
static int errorCount;
static char lastError[1024], openedURL[256];
void Com_Error(int code, const char *format, ...)
{
    va_list args;
    (void)code;
    ++errorCount;
    va_start(args, format);
    vsnprintf(lastError, sizeof(lastError), format, args);
    va_end(args);
}
void Sys_OpenURL(const char *url, int activate)
{
    assert(activate);
    snprintf(openedURL, sizeof(openedURL), "%s", url);
}
char *Info_ValueForKey(const char *s, const char *key)
{
    return !strcmp(key, "sv_hideServerIp") && !strcmp(s, "\\sv_hideServerIp\\1") ? (char *)"1" : (char *)"";
}
static int serverRevision = 6;
int Cod2x_GameVersion(void) { return serverRevision; }
/* The revision this build runs as. The text codes are tested as the build that has them (7);
   one block below checks that a build running as 1.4.6.8 reads none. */
static int buildRevision = 7;
int Cod2x_HostRevision(void) { return buildRevision; }
void Cod2x_PoseInit(void) {}
static int radarInitializations, radarFrames;
void CG_Cod2xRadarInit(void) { ++radarInitializations; }
void CG_Cod2xRadarFrame(void) { ++radarFrames; }
void CG_Cod2xSpectatorInit(void) {}
void CG_Cod2xSpectatorReset(void) {}
/* vMix and the update check's links to the client, the platform and the network: none here. */
void Cod2x_VmixInit(void) {}
void Cod2x_VmixFrame(int clientState) { (void)clientState; }
cg_t cgArray[1];
void CL_Cod2xCDKeyHash(char hash[34]) { hash[0] = '\0'; }
void Sys_Cod2xMachineHistory(Cod2xMachineHistory *out) { memset(out, 0, sizeof(*out)); }
void Sys_Cod2xMachineHistoryClear(void) {}
void Cod2x_Log(const char *format, ...)
{
    if (strncmp(format, "Client state changed", 20)) return;
    va_list args; va_start(args, format);
    vsnprintf(stateMessage, sizeof(stateMessage), format, args); va_end(args);
    ++stateLogs;
}
void Com_DPrintf(const char *format, ...)
{ if (!strncmp(format, "Client state changed", 20)) ++stateDebugLogs; }
int Sys_Milliseconds(void) { return (int)(uint32_t)serviceTicks; }
uint64_t Cod2x_Milliseconds64(void) { return serviceTicks; }
void Cod2x_HttpFrameFor(Cod2xHttpDone done) { (void)done; }
const char *NET_AdrToString(netadr_t a) { (void)a; return ""; }
qboolean NET_StringToAdr(const char *s, netadr_t *a) { (void)s; memset(a, 0, sizeof(*a)); a->type = NA_IP; return resolveUpdate; }
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b) { (void)a, (void)b; return 0; }
Bool NET_OutOfBandPrint(netsrc_t sock, netadr_t adr, const char *data) { (void)sock, (void)adr, (void)data; ++udpSends; return 1; }
static Cod2xHttpDone updateDone;
static void *updateContext;
int Cod2x_HttpStart(const Cod2xHttpRequest *request, Cod2xHttpDone done, void *context)
{
    (void)request;
    ++httpStarts;
    updateDone = done;
    updateContext = context;
    return !failHttpStart;
}

static void execute(const char *cmd, const char *a, const char *b)
{
    int i;
    argv[0] = (char *)cmd; argv[1] = (char *)a; argv[2] = (char *)b;
    argc = b ? 3 : a ? 2 : 1;
    for (i = 0; i < commandCount; ++i)
        if (!strcmp(commands[i].name, cmd)) {
            commands[i].function();
            return;
        }
    assert(0);
}

int main(void)
{
    const dvar_t *integer = Dvar_RegisterInt("test_int", 7, -100, 100, 0);
    const dvar_t *number = Dvar_RegisterFloat("test_float", .25f, -100, 100, 0);
    dvar_t *cheats = (dvar_t *)Dvar_RegisterBool("sv_cheats", 0, 0);
    dvar_t *debug;
    Cod2x_FeaturesInit();
    assert(radarInitializations == 1);
    execute("increase", "test_int", NULL);
    assert(integer->current.integer == 8);
    execute("decrease", "test_float", NULL);
    assert(number->current.value == -.75f);
    execute("increase", "sv_cheats", NULL);
    assert(setterCalls == 2 && !cheats->current.enabled);
    execute("increase", "missing_dvar", NULL);
    execute("increase", NULL, NULL);
    assert(setterCalls == 2);
    debug = (dvar_t *)Dvar_FindVar("cg_debugBullets");
    assert(debug->flags & 0x80);
    debug->current.enabled = 1;
    assert(!Cod2x_DebugBullets());
    cheats->current.enabled = 1;
    assert(Cod2x_DebugBullets());
    cheats->current.enabled = 0;
    assert(!Cod2x_DebugBullets());
    /* "match login" is the match service's (cod2x_match.c); the client registers the dvar. */
    Dvar_SetString(Dvar_FindVar("match_login"), "match-player-id");
    assert(Dvar_FindVar("match_login")->flags == 0x12);
    assert(Dvar_FindVar("com_autoaffinity")->flags == 0x1001 && Dvar_FindVar("com_autoaffinity")->current.enabled);
    Cod2x_FeaturesFrame(0, 0);
    assert(!strcmp(Dvar_FindVar("match_login")->current.string, "match-player-id"));
    Cod2x_FeaturesFrame(1, 1);
    assert(Cod2x_FeaturesDemo());
    Cod2x_FeaturesFrame(0, 0);
    assert(!Cod2x_FeaturesDemo());
    assert(radarFrames == 0); /* Radar follows the shared service phase. */
    {
        /* Revision notices occur on changes, and stock is announced at active. */
        dvar_t *game = (dvar_t *)Dvar_RegisterInt("g_cod2x", 6, 0, 6, 0x18);
        game->modified = 1;
        Cod2x_FeaturesClientState(CA_ACTIVE);
        assert(stateLogs == 1 && stateDebugLogs == 1 && versionNotices == 1);
        assert(!strcmp(stateMessage, "Client state changed from -1:UNKNOWN to 8:ACTIVE"));
        Cod2x_FeaturesClientState(CA_ACTIVE);
        assert(stateLogs == 1 && versionNotices == 1);
        Cod2x_FeaturesClientState(CA_DISCONNECTED);
        assert(game->current.integer == 0 && !game->modified);
        assert(stateLogs == 2 && stateDebugLogs == 2 && legacyNotices == 1);
        for (int state = 2; state <= CA_PRIMED; ++state) {
            Cod2x_FeaturesClientState(state);
            assert(game->current.integer == 0 && !game->modified && legacyNotices == 1);
        }
        Cod2x_FeaturesClientState(CA_ACTIVE);
        assert(legacyNotices == 2);
        Cod2x_FeaturesClientState(CA_ACTIVE);
        assert(legacyNotices == 2);
        game->current.integer = 6; game->modified = 1;
        Cod2x_FeaturesClientState(CA_ACTIVE);
        assert(versionNotices == 2 && !game->modified);
        for (int state = CA_DISCONNECTED; state <= CA_CONNECTED; ++state) {
            game->current.integer = 6;
            Cod2x_FeaturesClientState(state);
            assert(game->current.integer == 0 && !game->modified);
        }
        dvar_t *join = (dvar_t *)Dvar_RegisterInt("ui_joinGametype", 3, 0, 10, 0);
        int priorSets = setterCalls;
        Cod2x_IwdFrame(CA_DISCONNECTED); /* First observed disconnected frame counts. */
        assert(join->current.integer == 0 && setterCalls == priorSets + 1);
        join->current.integer = 3;
        Cod2x_IwdFrame(CA_DISCONNECTED);
        assert(join->current.integer == 3 && setterCalls == priorSets + 1);
        for (int state = CA_CONNECTING; state <= CA_ACTIVE; ++state)
            Cod2x_IwdFrame(state);
        assert(join->current.integer == 3 && setterCalls == priorSets + 1);
        Cod2x_IwdFrame(CA_DISCONNECTED);
        assert(join->current.integer == 0 && setterCalls == priorSets + 2);
        Cod2x_IwdFrame(CA_ACTIVE);
        Cod2x_IwdFrame(CA_DISCONNECTED); /* Already zero must not call the setter. */
        assert(setterCalls == priorSets + 2);
        puts("CoD2x IWD phase: initial/changed disconnected reset, stable and zero-valued frames untouched");
        puts("CoD2x client states: transition logs, revision clears, stock-active notice and stable frames pass");
    }
    {
        char code, line[80], buffer[80];
        int revision;
        const char *doubleName = "^^11Alice";
        for (revision = 0; revision <= 7; revision += revision ? 1 : 6) {
            Cod2x_FeaturesClientState(CA_ACTIVE);
            serverRevision = revision;
            assert(Cod2x_TextCode("#1", &code) == (revision >= 7 ? 2 : 0));
            assert(Cod2x_TextCode("$5", &code) == (revision >= 7 ? 2 : 0));
            assert(Cod2x_TextCode("^1", &code) == 2);
            strcpy(line, "^2^1Alice^7: Rank #1");
            Cod2x_StripChatName(line);
            assert(!strcmp(line, revision >= 7 ? "^2Alice^7: Rank #1" : "^2^1Alice^7: Rank #1"));
            assert(!strcmp(Cod2x_ResolveDoubleColors(doubleName, buffer, sizeof(buffer)),
                           revision >= 7 ? "^1Alice" : doubleName));
        }
        serverRevision = 0;
        Cod2x_FeaturesClientState(CA_DISCONNECTED);
        assert(Cod2x_TextCode("#1", &code) == 2);
        /* Running as 1.4.6.8, which has no text codes of its own: none in the menus, none in a game. */
        buildRevision = 6;
        assert(!Cod2x_TextEnabled() && Cod2x_TextCode("#1", &code) == 0 && Cod2x_TextCode("^1", &code) == 2);
        Cod2x_FeaturesClientState(CA_ACTIVE); serverRevision = 7;
        assert(!Cod2x_TextEnabled() && Cod2x_TextCode("$5", &code) == 0);
        serverRevision = 0; Cod2x_FeaturesClientState(CA_DISCONNECTED);
        buildRevision = 7;
    }
    {
        const char *address = "^1127.0.0.1:28960";
        char hidden[1024], longAddress[1025];
        short cells[16], masked[16];
        int i;
        Dvar_SetBool(Dvar_FindVar("cl_hideServerIp"), 0);
        Cod2x_HideServerIpFrame(NULL);
        assert(!Cod2x_HideServerIp());
        assert(Cod2x_HideIpText(address, hidden, sizeof(hidden)) == address);
        Cod2x_HideServerIpFrame("\\sv_hideServerIp\\1");
        assert(Cod2x_HideServerIp());
        assert(!strcmp(Cod2x_HideIpText(address, hidden, sizeof(hidden)), "^1***.*.*.*:*****"));
        Cod2x_HideServerIpFrame(NULL);
        assert(!Cod2x_HideServerIp());
        Dvar_SetBool(Dvar_FindVar("cl_hideServerIp"), 1);
        Cod2x_HideServerIpFrame(NULL);
        assert(Cod2x_HideServerIp());
        assert(!strcmp(Cod2x_HideIpText("connect example.invalid:28960", hidden, sizeof(hidden)),
                       "connect *******.*******:*****"));
        memset(longAddress, ' ', sizeof(longAddress));
        memcpy(longAddress, "127.0.0.1", 9);
        longAddress[1023] = 0;
        assert(Cod2x_HideIpText(longAddress, hidden, sizeof(hidden)) == hidden);
        assert(!memcmp(hidden, "***.*.*.*", 9));
        longAddress[1023] = ' ';
        longAddress[1024] = 0;
        assert(Cod2x_HideIpText(longAddress, hidden, sizeof(hidden)) == longAddress);
        for (i = 0; i < 9; ++i) cells[i] = (short)(0x4100 | "127.0.0.1"[i]);
        cells[9] = 0x0e31; /* A killfeed control cell remains byte-for-byte unchanged. */
        assert(Cod2x_HideIpCells(cells, 10, masked, 16) == masked);
        for (i = 0; i < 9; ++i) assert(masked[i] == (short)(0x4100 | "***.*.*.*"[i]));
        assert(masked[9] == cells[9]);
        Dvar_SetBool(Dvar_FindVar("cl_hideServerIp"), 0);
        Cod2x_HideServerIpFrame(NULL);
        assert(Cod2x_HideIpCells(cells, 10, masked, 16) == cells);
    }
    {
        char remote[80], url[1200];
        int i;
        assert(!Cod2x_ZpamDirectURL("main/zpam1.iwd", 0, url, sizeof(url)));
        assert(!Cod2x_ZpamDirectURL("mod/zpam1.iwd", 1, url, sizeof(url)));
        assert(!Cod2x_ZpamDirectURL("main/zpam1.tmp", 1, url, sizeof(url)));
        for (i = 0; i < 70; ++i) {
            snprintf(remote, sizeof(remote), "main/zpam%d.iwd", i);
            int started = Cod2x_ZpamDirectURL(remote, 1, url, sizeof(url));
            if (!started) { printf("zPAM direct unique attempt %d refused\n", i + 1); fflush(stdout); }
            assert(started);
            assert(Cod2x_ZpamDirectActive() && !strcmp(Cod2x_ZpamDirectRemote(), remote));
            assert(strstr(url, remote));
            Cod2x_ZpamDirectEnd(0);
            assert(!Cod2x_ZpamDirectActive());
        }
        for (i = 0; i < 70; ++i) {
            snprintf(remote, sizeof(remote), "main/zpam%d.iwd", i);
            assert(!Cod2x_ZpamDirectURL(remote, 1, url, sizeof(url)));
        }
        assert(Cod2x_ZpamDirectURL("main/zpam_maps_v999.iwd", 1, url, sizeof(url)));
        Cod2x_ZpamDirectEnd(1);
        assert(!Cod2x_ZpamDirectActive());
        assert(Cod2x_ZpamDirectURL("main/zpam_maps_v999.iwd", 1, url, sizeof(url)));
        Cod2x_ZpamDirectEnd(0);
        assert(!Cod2x_ZpamDirectURL("main/zpam_maps_v999.iwd", 1, url, sizeof(url)));
    }
    {
        Cod2xHttpResponse response = {0};
        response.status = 200;
        response.body = "{\"update\":{\"available\":2,\"url\":\"https://example.invalid/upstream.dll\",\"version\":\"9.0\"}}";
        Cod2x_FeaturesUpdateFrame();
        assert(updateDone);
        updateDone(updateContext, &response);
        errorCount = 0;
        Cod2x_FeaturesClientState(CA_CONNECTING);
        assert(errorCount == 0);
        Cod2x_UpdateConfirmed();
        assert(strstr(openedURL, "github.com/STiXzoOR/cod2-silicon/releases"));
        assert(strstr(lastError, "CoD2 Silicon") && strstr(lastError, "launcher"));
    }
    {
        char version[601], body[800];
        Cod2xHttpResponse response = {0};
        memset(version, 'v', sizeof(version) - 1); version[sizeof(version) - 1] = 0;
        snprintf(body, sizeof(body), "{\"update\":{\"available\":1,\"url\":\"https://example.invalid/update\",\"version\":\"%s\"}}", version);
        response.status = 200; response.body = body;
        updateDone(updateContext, &response);
        assert(!strcmp(Dvar_FindVar("cl_updateversion")->current.string, version));
        puts("updater preserves600-byte version text");
        serviceTicks = (UINT64_C(1) << 32) - 1000;
        resolveUpdate = 1; failHttpStart = 1;
        int initial = httpStarts;
        Cod2x_FeaturesRenderer();
        Cod2x_FeaturesClientState(CA_DISCONNECTED);
        Cod2x_FeaturesUpdateFrame();
        assert(httpStarts == initial + 1 && udpSends == 1);
        serviceTicks += 5000; Cod2x_FeaturesUpdateFrame();
        assert(httpStarts == initial + 1);
        ++serviceTicks; Cod2x_FeaturesUpdateFrame();
        assert(httpStarts == initial + 2 && udpSends == 2);
        serviceTicks += (UINT64_C(1) << 32) + 1000; Cod2x_FeaturesUpdateFrame();
        assert(httpStarts == initial + 2 && udpSends == 2); /* Upstream GetTickCount is modulo32 bits. */
        serviceTicks += 4001; Cod2x_FeaturesUpdateFrame();
        assert(httpStarts == initial + 3 && udpSends == 3);
        puts("updater start failure falls back UDP;5000/5001ms across32bit wrap and49-day modulo behavior pass");
    }
    puts("cod2x client feature command integration: passed");
    return 0;
}
