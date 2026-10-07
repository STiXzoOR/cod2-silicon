#include "../fidelity/dvar_services.h"
#include "cod2x.h"
#include <assert.h>

#include "host_globals.h"
const dvar_t *sv_punkbuster, *sv_wwwDownload, *sv_wwwBaseURL, *sv_wwwDlDisconnected;
const dvar_t *net_lanauthorize;
server_t sv;
serverStatic_t svs;
void *imp_sv = &sv, *imp_svs = &svs;
static LegacyHacks legacy;
static LegacyHacks *legacyPointer = &legacy;
void *imp_legacyHacks = &legacyPointer;
int sv_serverId_value, com_frameTime = 1000, com_errorEntered;
void *imp_com_frameTime = &com_frameTime, *imp_com_errorEntered = &com_errorEntered;
void *imp_dvar_modifiedFlags = &dvar_modifiedFlags;
void *imp_com_sv_running = &com_sv_running, *imp_sv_gametype = &sv_gametype;
void *imp_sv_maxclients = &sv_maxclients, *imp_sv_serverid = &sv_serverid;
void *imp_sv_serverId_value = &sv_serverId_value;
unsigned char mtlLoadGlob[4];
static clientActive_t client;
void *cl = &client;
static clientConnection_t connection;
static byte *connectionPointer = (byte *)&connection;
byte **clc_ptr = &connectionPointer;
int cl_connectedToPureServer;
static cg_t clientGame;
cg_t *cg = &clientGame;
static gentity_t entities[2];
static client_t hostClients[64];
static int begins, publications, clientResets, allResets, identityReads, gameplayStarts;
static char published[128], serialized[128];
static int valueindex;
static char value1[2][8192];

extern void Dvar_SetInt(const dvar_t *, int);
extern void Dvar_SetString(const dvar_t *, const char *);
extern void Dvar_SetStringByName(const char *, const char *);
extern void Dvar_SetFromStringByName(const char *, const char *);
extern Bool Dvar_GetBool(const char *);
extern void Dvar_SetCheatState(void);
extern void Dvar_ResetScriptInfo(void);
extern void Dvar_ClearModified(const dvar_t *);
extern const char *Dvar_GetString(const char *);

void Cod2x_ResetAnimation(void) { ++allResets; }
void Cod2x_ResetClientAnimation(void) { ++clientResets; }
int Cod2x_ReadMachineHwid(char id[33]) { ++identityReads; memset(id, 'a', 32); id[32] = 0; return 1; }
int Cod2x_LimitedFPS(int fps, int limited) { return limited && (fps < 125 || fps > 250) ? 250 : fps; }
void Cod2x_FeaturesInit(void) {}
void Cod2x_FeaturesFrame(int active, int demo) { (void)active; (void)demo; }
void Cod2x_IwdSystemInfo(const char *names) { (void)names; }
#if !defined(COD2_CODX) || !COD2_CODX
int Cod2x_GameVersion(void) { return 0; }
void Cod2x_Disconnect(void) {}
#endif

void Com_DPrintf(const char *format, ...) { (void)format; }
void Com_Memset(void *p, int value, int size) { memset(p, value, size); }
void SV_AddOperatorCommands(void) {}
void SV_RemoveOperatorCommands(void) {}
void SV_MasterShutdown(void) {}
void SV_ShutdownGameProgs(void) {}
void SV_FreeClients(void) { svs.clients = NULL; }
void SV_DropClient(client_t *client_, const char *reason) { (void)reason; client_->state = 0; }
void SV_SendClientSnapshot(client_t *client_) { (void)client_; }
void SV_SendServerCommand(client_t *client_, svscmd_type type, const char *format, ...)
{
    va_list args;
    CHECK(client_ == &svs.clients[0]);
    CHECK(type == SV_CMD_RELIABLE);
    va_start(args, format);
    vsnprintf(published, sizeof(published), format, args);
    va_end(args);
    ++publications;
    CHECK(begins == publications); /* Original gameplay begin precedes publication. */
}
void ClientBegin(int num) { CHECK(num == 0); ++begins; }
gentity_t *SV_GentityNum(int num) { assert(num >= 0 && num < 2); return &entities[num]; }
void SV_RunFrame(void) { CHECK(Cod2x_GameVersion() == 0); }
void SV_Heartbeat_f(void) {}
void SV_SetConfig(int start, int max, int count) { (void)start; (void)max; (void)count; }
void SV_SetGametype(void) {}
void SV_InitGameProgs(int savepersist)
{
    (void)savepersist;
    ++gameplayStarts;
    CHECK(Cod2x_GameVersion() == 0);
}
void SV_RestartGameProgs(qboolean savepersist) { SV_InitGameProgs(savepersist); }
void SV_AddServerCommand(client_t *client_, svscmd_type type, const char *cmd)
{
    CHECK(client_ == &svs.clients[0] && type == SV_CMD_RELIABLE);
    CHECK(!strcmp(cmd, "B"));
}
void G_SetSavePersist(qboolean savepersist) { (void)savepersist; }
void FS_ConvertPath(char *path) { (void)path; }
void CL_InitLoad(const char *map, const char *game) { (void)map; (void)game; }
void CL_MapLoading(const char *map) { (void)map; Cod2x_Disconnect(); }
void CL_ShutdownAll(void) {}
void CL_StartLoading(const char *map, const char *game) { (void)map; (void)game; }
void CL_Disconnect(void) { Cod2x_Disconnect(); }
unsigned char CL_GetLocalClientActive(int num) { (void)num; return 0; }
void Com_Restart(void) {}
void Material_PreLoadAllShaderText(void) {}
void Com_LoadBsp(const char *path) { (void)path; }
void CM_LoadMap(const char *path, int *checksum) { (void)path; *checksum = 1; }
void Com_UnloadBsp(void) {}
void CM_LinkWorld(void) {}
void Com_LoadSoundAliases(const char *path, const char *name, snd_alias_system_t type) { (void)path; (void)name; (void)type; }
void Com_AbortDObj(void) {}
void DObjAbort(void) {}
void XAnimAbort(void) {}
void Scr_Abort(void) {}
void Scr_ParseGameTypeList(void) {}
void FX_FreeSystem(void) {}
int FX_InitSystem(int num) { (void)num; return 0; }
void FX_CreateDefaultEffect(void) {}
void FS_Shutdown(int close) { (void)close; }
void FS_ClearIwdReferences(void) {}
void FS_Restart(int feed) { (void)feed; }
const char *FS_LoadedIwdChecksums(void) { return ""; }
const char *FS_LoadedIwdNames(void) { return ""; }
const char *FS_ReferencedIwdChecksums(void) { return ""; }
const char *FS_ReferencedIwdNames(void) { return ""; }
const char *GetBspExtension(void) { return "d3dbsp"; }
int Sys_Milliseconds(void) { return 1000; }
int Sys_MillisecondsRaw(void) { return 1000; }
int rand(void) { return 1; }
void srand(unsigned int seed) { (void)seed; }
void NET_Sleep(int ms) { (void)ms; }
Bool NET_OutOfBandPrint(int source, netadr_t address, const char *text) { (void)source; (void)address; (void)text; return 1; }
int G_GetSavePersist(void) { return 0; }
const char *ClientConnect(int num, unsigned short id) { (void)num; (void)id; return NULL; }
void DBG_Hunk_PrintUsage(const char *text) { (void)text; }
void *Hunk_AllocInternal(int size) { (void)size; return NULL; }
void SV_Startup(void) { svs.clients = hostClients; Dvar_SetBool(com_sv_running, 1); }
void SV_ChangeMaxClients(void) {}
void SV_SetExpectedHunkUsage(char *map) { (void)map; }
char *Dvar_InfoString_Big(int flags)
{
    CHECK(flags == 8);
    const dvar_t *version = Dvar_FindVar("g_cod2x");
    snprintf(serialized, sizeof(serialized), "\\g_cod2x\\%d", version ? version->current.integer : 0);
    return serialized;
}
char *Dvar_InfoString(int flags) { (void)flags; return ""; }
void SV_SetConfigstring(int index, const char *text)
{
    if (index == 1)
        CHECK(strstr(text, "\\g_cod2x\\0") != NULL);
}
void LargeLocal_LargeLocal(const LargeLocal *local, int size) { (void)local; CHECK(size == 0x2000); }
void *LargeLocal_GetBuf(const LargeLocal *local)
{
    static char buffers[2][8192];
    static int next;
    (void)local;
    return buffers[(next++) & 1];
}
void ZN10LargeLocalD1Ev(const LargeLocal *local) { (void)local; }
void FS_PureServerSetLoadedIwds(const char *sums, const char *names) { (void)sums; (void)names; }
void FS_PureServerSetReferencedIwds(const char *sums, const char *names) { (void)sums; (void)names; }
int NetHardening_ServerMaySetCvar(const char *name) { (void)name; return 1; }

#include "host_info_source.h"
#include "host_server_source.h"
#include "host_begin_source.h"
#include "host_restart_source.h"
#include "host_systeminfo_source.h"
#include "host_reliable_source.h"

static void systemInfo(const char *text, int demo)
{
    client.gameState.stringOffsets[1] = 0;
    strcpy(client.gameState.stringData, text);
    connection.demoplaying = demo;
    CL_SystemInfoChanged();
}

static void checkHost(void)
{
    CHECK(Cod2x_GameVersion() == 0);
    CHECK(Dvar_FindVar("g_cod2x")->current.integer == 0);
#ifdef COD2X_HOST_REVISION
    CHECK(Cod2x_ServerActive());
    CHECK(Cod2x_ServerGameVersion() == 0);
#endif
}

int main(void)
{
    Dvar_Init();
#ifdef DEDICATED
    com_dedicated = Dvar_RegisterInt("dedicated", 1, 0, 2, 0);
#else
    com_dedicated = Dvar_RegisterInt("dedicated", 0, 0, 2, 0);
#endif
    com_sv_running = Dvar_RegisterBool("sv_running", 0, 0);
    cl_paused = Dvar_RegisterInt("cl_paused", 0, 0, 1, 0);
    SV_Init();
    CHECK(Dvar_FindVar("protocol")->current.integer == 118);
    CHECK(Dvar_FindVar("protocol")->domain.integer.min == 118);
    CHECK(Dvar_FindVar("protocol")->domain.integer.max == 118);
#if !defined(COD2_CODX) || !COD2_CODX
    CHECK(Dvar_FindVar("g_cod2x") == NULL);
    SV_SpawnServer("stock_fixture");
    static client_t stockClient;
    svs.clients = &stockClient;
    Dvar_SetInt(sv_maxclients, 1);
    usercmd_t stockCmd = {0};
    SV_ClientEnterWorld(&stockClient, (const dvar_t *(*)[4])&stockCmd);
    CHECK(begins == 1 && publications == 0);
    stockClient.state = 0;
    svs.clients = NULL;
    SV_Shutdown("stock fixture shutdown");
    CHECK(identityReads == 0 && clientResets == 0 && allResets == 0);
    Dvar_Shutdown();
    FINISH("stock native lifecycle control");
#else
    CHECK(Dvar_FindVar("g_cod2x") != NULL); /* Dedicated registration is not client-owned. */
    CHECK(identityReads == 0 && Dvar_FindVar("cl_hwid2") == NULL);
    if (!Dvar_FindVar("g_cod2x"))
        FINISH("host lifecycle: missing dedicated registration");
    const dvar_t *shared = Dvar_FindVar("g_cod2x");
#ifdef DEDICATED
    CHECK(Dvar_FindVar("g_competitive")->flags == 0x1008);
#else
    CHECK(Dvar_FindVar("g_competitive")->flags == 0x18);
#endif
    SV_SpawnServer("fixture");
    checkHost();
    CHECK(gameplayStarts == 1);
    CHECK(identityReads == 0);
    CHECK(shared->domain.integer.min == 0 && shared->domain.integer.max == 0);
    Dvar_SetInt(shared, 6);
    checkHost();

#ifndef DEDICATED
    Dvar_SetInt(com_dedicated, 0);
    Cod2x_Init();
    CHECK(Dvar_FindVar("g_cod2x") == shared && identityReads == 1);
    CHECK(Cod2x_GameVersion() == 0);

    /* Explicit defaults and the generic systeminfo loop must respect ownership. */
    systemInfo("\\g_cod2x\\6\\g_competitive\\1\\com_maxfps_limit\\1\\sv_serverid\\17", 0);
    checkHost();
    CHECK(!Dvar_FindVar("g_competitive")->current.enabled);
    CHECK(!Dvar_FindVar("com_maxfps_limit")->current.enabled);
    CG_SetClientDvarFromServer("G_COD2X", "6");
    checkHost();
    systemInfo("\\sv_serverid\\18", 0);
    checkHost();
    Dvar_SetBool(Dvar_FindVar("g_competitive"), 1);
    systemInfo("\\g_cod2x\\6\\g_competitive\\0", 0);
    CHECK(Dvar_FindVar("g_competitive")->current.enabled);
    Cod2x_Disconnect();
    checkHost();
    CHECK(Dvar_FindVar("g_competitive")->current.enabled);
    Dvar_SetBool(Dvar_FindVar("g_competitive"), 0);
    CHECK(clientResets > 0);
#endif

    /* Soft restart reasserts before gameplay; full spawn republishes systeminfo. */
    ((dvar_t *)shared)->current.integer = 6;
    SV_InitDvar();
    checkHost();
    SV_SpawnServer("fixture_restart");
    checkHost();
    CHECK(gameplayStarts == 2);

    static client_t hostClient;
    svs.clients = &hostClient;
    Dvar_SetInt(sv_maxclients, 1);
    usercmd_t cmd = {0};
    SV_ClientEnterWorld(&hostClient, (const dvar_t *(*)[4])&cmd);
    CHECK(begins == 1 && publications == 1);
    CHECK(!strcmp(published, "v g_cod2x \"0\""));
    Dvar_ClearModified(sv_maxclients);
    ++com_frameTime;
    ((dvar_t *)shared)->current.integer = 6;
    SV_MapRestart(1);
    checkHost();
    CHECK(begins == 2 && publications == 2);
    CHECK(!strcmp(published, "v g_cod2x \"0\""));
    CHECK(gameplayStarts == 3);
    SV_MapRestart(0);
    checkHost();
    CHECK(gameplayStarts == 4);
    SV_ClientEnterWorld(&hostClient, (const dvar_t *(*)[4])&cmd);
    CHECK(begins == 3 && publications == 3);
    hostClient.state = 0;
    svs.clients = NULL;
    SV_Shutdown("fixture shutdown");
    CHECK(Cod2x_GameVersion() == 0);
#ifdef COD2X_HOST_REVISION
    CHECK(!Cod2x_ServerActive() && Cod2x_ServerGameVersion() == 0);
    CHECK(shared->domain.integer.min == 0 && shared->domain.integer.max == 6);
#endif

#ifndef DEDICATED
    /* Actual systeminfo/reliable dispatch drives remote 0 -> 6 -> 0 transitions. */
    const dvar_t *fps = Dvar_RegisterInt("com_maxfps", 333, 0, 1000, 1);
    systemInfo("\\sv_serverid\\19", 0);
    CHECK(Cod2x_GameVersion() == 0);
    systemInfo("\\g_cod2x\\6\\g_competitive\\1", 0);
    CHECK(Cod2x_GameVersion() == 6);
    Cod2x_Frame(1, 0);
    CHECK(Cod2x_Competitive() && fps->domain.integer.min == 125);
    Cod2x_Frame(1, 1);
    CHECK(!Cod2x_Competitive() && fps->domain.integer.min == 0);
    CG_SetClientDvarFromServer("g_cod2x", "3");
    CHECK(Cod2x_GameVersion() == 3);
    systemInfo("\\sv_serverid\\20", 0);
    Cod2x_Frame(1, 0);
    CHECK(Cod2x_GameVersion() == 0 && !Cod2x_Competitive());
    CHECK(fps->domain.integer.max == 1000);
    systemInfo("\\g_cod2x\\6\\g_competitive\\1", 1);
    Cod2x_Frame(1, 1);
    CHECK(Cod2x_GameVersion() == 6 && !Cod2x_Competitive());
    Cod2x_Disconnect();
    CHECK(Cod2x_GameVersion() == 0 && !Cod2x_Competitive());
    systemInfo("\\g_cod2x\\6\\g_competitive\\1", 0);
    SV_SpawnServer("fixture_after_remote");
    checkHost();
    CHECK(!Dvar_FindVar("g_competitive")->current.enabled);
    CHECK(!Dvar_FindVar("com_maxfps_limit")->current.enabled);
    SV_Shutdown("fixture final shutdown");
#endif
    Dvar_Shutdown();
    FINISH("native host lifecycle, revision ownership, begin, restart and remote policy");
#endif
}
