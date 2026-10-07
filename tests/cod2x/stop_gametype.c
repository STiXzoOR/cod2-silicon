#include "../fidelity/dvar_services.h"
#include "cod2x.h"
#include "stop_globals.h"
#include "stop_features.h"
#include <assert.h>
#include <setjmp.h>

const dvar_t *sv_punkbuster, *sv_wwwDownload, *sv_wwwBaseURL, *sv_wwwDlDisconnected;
const dvar_t *net_lanauthorize, *g_gametype, *g_maxclients;
server_t sv;
serverStatic_t svs;
level_locals_t level;
bgs_t level_bgs;
scr_data_t g_scr_data;
gentity_t g_entities[1024];
void *imp_sv = &sv, *imp_svs = &svs, *imp_g_scr_data = &g_scr_data;
static void *bgs;
void *imp_bgs = &bgs;
static LegacyHacks legacy;
static LegacyHacks *legacyPointer = &legacy;
void *imp_legacyHacks = &legacyPointer;
int sv_serverId_value, com_frameTime = 1000, com_errorEntered;
void *imp_com_frameTime = &com_frameTime, *imp_com_errorEntered = &com_errorEntered;
void *imp_dvar_modifiedFlags = &dvar_modifiedFlags;
void *imp_com_sv_running = &com_sv_running, *imp_sv_gametype = &sv_gametype;
void *imp_sv_maxclients = &sv_maxclients, *imp_sv_serverid = &sv_serverid;
void *imp_sv_serverId_value = &sv_serverId_value;
void *imp_sv_mapRotation = &sv_mapRotation, *imp_sv_mapRotationCurrent = &sv_mapRotationCurrent;
unsigned char mtlLoadGlob[4];
static client_t hostClients[64];
static gclient_t gameClients[64];
static int destroyedEntities, destroyedScripts, stopped, clearedReturns, matchEnds, persisted, gameplayStarts;
static int freedPersistentClients, scriptShutdownComplete;
static int scriptActive = 1, optionalCallback = 1, optionalLookups;
static char cmdName[32] = "map", cmdMap[64] = "fixture", cmdMarker[64], spawnedMap[64];
int cmd_wait;
cmd_t cmd_texts[1], *cmd_text = cmd_texts;
static cmd_text_buf_t cmd_text_buf;
#define queued ((char *)cmd_text->data)
static jmp_buf transitionError;
static int callbackError, startupError, missingMap, missingProfile, retainedStrings;
static int scriptArgCount, scriptPersist, returnedType = 6, returnedInt = 1, nestedRequest;
static int callbackArgs[3], callbackDestroys;
static char callbackSource[32];
static VariableValue values[32];
scrVmPub_t scrVmPub;

void SV_SpawnServer(const char *server);
void SV_InitDvar(void);
void SV_InitArchivedSnapshot(void);
void G_ShutdownGame(qboolean freeScripts);
void GScr_FreeScripts(void);
void GScr_MapRestart(void);
void GScr_LoadMap(void);
void GScr_ExitLevel(void);
void GScr_Cod2xQueueExitLevel(void);
void ExitLevel(void);
int G_GetSavePersist(void);
void G_SetSavePersist(qboolean persist);
void Cbuf_ExecuteText(int when, const char *text);
void Cbuf_Execute(void);
void Cmd_ExecuteString(const char *text);
static inline __attribute__((always_inline)) void Cbuf_InsertText(const char *text);
static void Cbuf_ExecuteInternal(void);
static void SV_Map_f(void);
static void SV_MapRestart(qboolean fast_restart);
static void SV_MapRotate_f(void);
static void SV_MapRestart_f(void);
static void SV_FastRestart_f(void);
#if STOP_CALLBACK_IMPLEMENTED
qboolean GScr_Cod2xBeforeMapChange(qboolean shutdown, const char *source);
qboolean SV_Cod2xBeginMapChange(qboolean shutdown, const char *source);
void SV_Cod2xEndMapChange(qboolean shutdown);
#endif

extern void Dvar_SetInt(const dvar_t *, int);
extern void Dvar_SetString(const dvar_t *, const char *);
extern void Dvar_SetStringByName(const char *, const char *);
extern void Dvar_SetFromStringByName(const char *, const char *);
extern Bool Dvar_GetBool(const char *);
extern void Dvar_SetCheatState(void);
extern void Dvar_ResetScriptInfo(void);
extern void Dvar_ClearModified(const dvar_t *);
extern const char *Dvar_GetString(const char *);
void Cod2x_ResetAnimation(void) {}
void Cod2x_ResetClientAnimation(void) {}
int Cod2x_ReadMachineHwid(char id[33]) { memset(id, 'a', 32); id[32] = 0; return 1; }
int Cod2x_LimitedFPS(int fps, int limited) { return limited && (fps < 125 || fps > 250) ? 250 : fps; }
void Cod2x_FeaturesInit(void) {}
void Cod2x_FeaturesFrame(int active, int demo) { (void)active; (void)demo; }
void Cod2x_IwdSystemInfo(const char *names) { (void)names; }
#if !defined(COD2_CODX) || !COD2_CODX
int Cod2x_GameVersion(void) { return 0; }
void Cod2x_Disconnect(void) {}
#endif
void SV_Cod2xClearIdentities(void) {}
void Com_DPrintf(const char *format, ...) { (void)format; }
void Com_Memset(void *p, int value, int size) { memset(p, value, size); }
void SV_AddOperatorCommands(void) {}
void SV_RemoveOperatorCommands(void) {}
void SV_MasterShutdown(void) {}
void SV_ShutdownGameProgs(void) { if (level.num_entities) G_ShutdownGame(1); }
void SV_FreeClients(void) { svs.clients = NULL; }
void SV_DropClient(client_t *client, const char *reason) { (void)reason; client->state = 0; }
void SV_SendClientSnapshot(client_t *client) { (void)client; }
void SV_SendServerCommand(client_t *client, svscmd_type type, const char *format, ...) { (void)client; (void)type; (void)format; }
void SV_ClientEnterWorld(client_t *client, const dvar_t *(*cmd)[4]) { (void)client; (void)cmd; }
gentity_t *SV_GentityNum(int num) { return &g_entities[num]; }
void SV_RunFrame(void) {}
void SV_Heartbeat_f(void) {}
void SV_SetConfig(int start, int max, int count) { (void)start; (void)max; (void)count; }
void SV_SetGametype(void) {}
void SV_InitGameProgs(int savepersist)
{
    persisted = savepersist;
    ++gameplayStarts;
    level.num_entities = 1;
    g_entities[0].r.inuse = 1;
    level.finished = level.savepersist = 0;
}
void SV_RestartGameProgs(qboolean savepersist) { G_ShutdownGame(0); SV_InitGameProgs(savepersist); }
void SV_AddServerCommand(client_t *client, svscmd_type type, const char *cmd) { (void)client; (void)type; (void)cmd; }
void FS_ConvertPath(char *path) { (void)path; }
void CL_InitLoad(const char *map, const char *game) { (void)map; (void)game; }
void CL_MapLoading(const char *map) { (void)map; }
void CL_ShutdownAll(void) {}
void CL_StartLoading(const char *map, const char *game) { (void)map; (void)game; }
void CL_Disconnect(void) {}
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
void Scr_Abort(void) { scriptActive = 0; }
void Scr_ParseGameTypeList(void) { if (startupError) longjmp(transitionError, 1); }
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
const char *ClientConnect(int num, unsigned short id) { (void)num; (void)id; return NULL; }
void DBG_Hunk_PrintUsage(const char *text) { (void)text; }
void *Hunk_AllocInternal(int size) { (void)size; return NULL; }
void SV_Startup(void) { svs.clients = hostClients; Dvar_SetBool(com_sv_running, 1); }
void SV_ChangeMaxClients(void) {}
void SV_SetExpectedHunkUsage(char *map) { snprintf(spawnedMap, sizeof(spawnedMap), "%s", map); }
char *Dvar_InfoString_Big(int flags) { (void)flags; return ""; }
char *Dvar_InfoString(int flags) { (void)flags; return ""; }
void SV_SetConfigstring(int index, const char *text) { (void)index; (void)text; }
void G_LogPrintf(const char *fmt, ...) { (void)fmt; }
void FS_FCloseFile(fileHandle_t f) { (void)f; }
void G_FreeEntity(gentity_t *ent) { ent->r.inuse = 0; ++destroyedEntities; }
void HudElem_DestroyAll(void) {}
void SV_FreeClientScriptPers(void) { ++freedPersistentClients; }
int Scr_IsSystemActive(int inst) { CHECK(inst == 1); return scriptActive; }
void Scr_ShutdownSystem(int inst, int complete) { CHECK(inst == 1); scriptShutdownComplete = complete; ++destroyedScripts; scriptActive = 0; }
void Mantle_ShutdownAnims(void) {}
void Scr_FreeScripts(int inst) { (void)inst; }
void Scr_RemoveClassMap(int classnum) { (void)classnum; }
void XAnimFreeTree(struct XAnimTree_s *tree, int user) { (void)tree; (void)user; }
void Hunk_ClearToMarkLow(int mark) { (void)mark; }
unsigned int Scr_LoadScript(const char *name) { (void)name; return 1; }
scr_func_t Scr_GetFunctionHandle(const char *name, const char *label)
{
    (void)name;
    if (!strcmp(label, "CodeCallback_StopGameType")) { ++optionalLookups; return optionalCallback ? 100 : 0; }
    return 1;
}
char *SV_Cmd_Argv(int arg) { return arg == 0 ? cmdName : arg == 1 ? cmdMap : arg == 2 ? cmdMarker : ""; }
int FS_ReadFile(const char *name, void **buf) { (void)name; (void)buf; return missingMap ? -1 : 1; }
const char *FS_GetMapBaseName(const char *name) { return name; }
char *I_strlwr(char *s) { for (char *p = s; *p; ++p) *p = tolower(*p); return s; }
unsigned char Com_HasPlayerProfile(void) { return !missingProfile; }
const char *Com_Parse(const char **text)
{
    static char token[64]; size_t n = 0;
    while (**text == ' ') ++*text;
    while (**text && **text != ' ' && n < sizeof(token) - 1) token[n++] = *(*text)++;
    token[n] = 0;
    while (**text == ' ') ++*text;
    return token;
}
void Cmd_ExecuteString(const char *text)
{
    cmdName[0] = cmdMap[0] = cmdMarker[0] = 0;
    sscanf(text, "%31s %63s %63s", cmdName, cmdMap, cmdMarker);
    if (!strcmp(cmdName, "wait")) cmd_wait = atoi(cmdMap);
    else if (!strcmp(cmdName, "map") || !strcmp(cmdName, "devmap")) SV_Map_f();
    else if (!strcmp(cmdName, "map_rotate")) SV_MapRotate_f();
    else if (!strcmp(cmdName, "map_restart")) SV_MapRestart_f();
    else if (!strcmp(cmdName, "fast_restart")) SV_FastRestart_f();
    else abort();
}
void SV_MatchEnd(void) { ++matchEnds; }
qboolean SV_MapExists(const char *name) { (void)name; return 1; }
unsigned int Scr_GetNumParam(void) { return scriptArgCount; }
unsigned int Scr_GetConstString(unsigned int arg) { (void)arg; return 1; }
void SL_AddRefToString(unsigned int value) { (void)value; ++retainedStrings; }
void SL_RemoveRefToString(unsigned int value) { (void)value; --retainedStrings; CHECK(retainedStrings >= 0); }
const char *Scr_GetString(unsigned int arg) { (void)arg; return "fixture_next"; }
void Scr_Error(const char *text) { fprintf(stderr, "%s\n", text); abort(); }
void Scr_AddString(const char *s) { snprintf(callbackSource, sizeof(callbackSource), "%s", s); ++scrVmPub.top; ++scrVmPub.inparamcount; scrVmPub.top->type = 2; }
void Scr_AddBool(int b) { ++scrVmPub.top; ++scrVmPub.inparamcount; scrVmPub.top->type = 6; scrVmPub.top->u.intValue = b; }
void RemoveRefToValue(int type, VariableUnion u) { (void)type; (void)u; ++clearedReturns; }
int Scr_GetType(unsigned int arg) { CHECK(arg == 0 && scrVmPub.outparamcount == 1); return scrVmPub.top->type; }
int Scr_GetInt(unsigned int arg)
{
    if (!scrVmPub.outparamcount) return scriptPersist;
    CHECK(arg == 0 && scrVmPub.top->type == 6);
    return scrVmPub.top->u.intValue;
}
void Scr_AddExecThread(scr_func_t handle, unsigned int paramcount)
{
    CHECK(handle == 100 && paramcount == 4 && scrVmPub.inparamcount == 4);
    ++stopped;
    callbackArgs[0] = scrVmPub.top[0].u.intValue;
    callbackArgs[1] = scrVmPub.top[-1].u.intValue;
    callbackArgs[2] = scrVmPub.top[-2].u.intValue;
    callbackDestroys = destroyedEntities + destroyedScripts;
    if (callbackError) longjmp(transitionError, 1);
    scrVmPub.top -= 4; scrVmPub.inparamcount = 0;
    if (nestedRequest) { scriptArgCount = 1; GScr_MapRestart(); }
    ++scrVmPub.top; scrVmPub.top->type = returnedType; scrVmPub.top->u.intValue = returnedInt;
    scrVmPub.outparamcount = 1;
}
#include "stop_main_source.h"
#include "stop_game_source.h"
#include "stop_commands_source.h"
#include "stop_server_source.h"
#include "stop_buffer_source.h"

static void resetCase(int present, int type, int value, int fromScript, int persist)
{
    memset(&level, 0, sizeof(level)); memset(&sv, 0, sizeof(sv));
    level.num_entities = 1; level.openScriptIOFileHandles[0] = -1;
    level.clients = gameClients;
    memset(gameClients, 0, sizeof(gameClients));
    gameClients[0].sess.connected = CON_CONNECTED;
    level.finished = fromScript; level.savepersist = persist;
    level.teamScores[TEAM_AXIS] = 10; level.teamScores[TEAM_ALLIES] = 20;
    g_entities[0].r.inuse = 1; sv.state = SS_GAME;
    strcpy(sv.gametype, "dm"); svs.clients = hostClients;
    memset(hostClients, 0, sizeof(hostClients));
    Dvar_SetString(sv_gametype, "dm");
    Dvar_SetBool(com_sv_running, 1); Dvar_ClearModified(sv_maxclients);
    Dvar_SetBool(sv_cheats, 0);
    scriptActive = 1; optionalCallback = present;
    returnedType = type; returnedInt = value;
    stopped = optionalLookups = destroyedEntities = destroyedScripts = gameplayStarts = 0;
    matchEnds = clearedReturns = nestedRequest = callbackError = startupError = 0;
    missingMap = missingProfile = 0;
    Cbuf_Init(); memset(cmd_text_buf, 0, sizeof(cmd_text_buf)); cmd_wait = 0;
    cmdMarker[0] = spawnedMap[0] = 0;
    freedPersistentClients = scriptShutdownComplete = 0;
    scrVmPub.top = values; scrVmPub.outparamcount = scrVmPub.inparamcount = 0;
    ++com_frameTime; GScr_LoadGameTypeScript();
}
static void transition(int source)
{
    strcpy(cmdName, source == 5 ? "devmap" : "map"); strcpy(cmdMap, "fixture_next");
    switch (source) {
    case 0: case 5: SV_Map_f(); break;
    case 1: SV_FastRestart_f(); break;
    case 2: SV_MapRestart_f(); break;
    case 3: Dvar_SetString(sv_mapRotationCurrent, "gametype tdm map fixture_next"); SV_MapRotate_f(); break;
    case 4: SV_Shutdown("synthetic shutdown"); break;
    }
}
int main(void)
{
    Dvar_Init();
    com_dedicated = Dvar_RegisterInt("dedicated", 1, 0, 2, 0);
    com_sv_running = Dvar_RegisterBool("sv_running", 0, 0);
    cl_paused = Dvar_RegisterInt("cl_paused", 0, 0, 1, 0);
    SV_Init(); g_gametype = sv_gametype; g_maxclients = sv_maxclients;
    static const char *sources[] = { "map", "fast_restart", "map_restart", "map_rotate", "shutdown", "map" };
    int codx = 0;
#if defined(COD2_CODX) && COD2_CODX
    codx = 1;
#endif
    for (int source = 0; source < 6; ++source) {
        for (int present = 0; present <= 1; ++present) {
            for (int origin = 0; origin <= 1; ++origin) {
                for (int persist = 0; persist <= 1; ++persist) {
                    for (int result = 0; result < 5; ++result) {
                        int types[] = { 6, 6, 6, 0, 5 }, vals[] = { 0, 1, -7, 0, 0 };
                        resetCase(present, types[result], vals[result], origin, persist);
                        transition(source);
                        int veto = codx && present && result == 0 && source != 4;
                        CHECK(optionalLookups == codx);
                        CHECK(stopped == (codx && present));
                        if (codx && present) {
                            CHECK(!strcmp(callbackSource, sources[source]));
                            CHECK(callbackArgs[0] == origin && callbackArgs[1] == !persist && callbackArgs[2] == (source == 4));
                            CHECK(callbackDestroys == 0);
                            CHECK(scrVmPub.outparamcount == 0 && scrVmPub.inparamcount == 0 && scrVmPub.top == values);
                        }
                        CHECK((destroyedEntities != 0) == !veto);
                        CHECK((destroyedScripts != 0) == !veto);
                        if (source == 1 && !veto) {
                            CHECK(scriptShutdownComplete == !persist && freedPersistentClients == !persist);
                            CHECK(persisted == persist);
                        }
                        CHECK(!veto || (com_sv_running->current.enabled && level.num_entities == 1));
                        if (source == 5) CHECK(sv_cheats->current.enabled == !veto);
                        if (source == 3 && veto) CHECK(!strcmp(sv_mapRotationCurrent->current.string, "gametype tdm map fixture_next"));
                        if (source == 4) {
                            transition(source);
                            CHECK(stopped == (codx && present) && destroyedEntities == 1 && destroyedScripts == 1);
                        }
                    }
                }
            }
        }
    }
    if (codx) {
        resetCase(1, 6, 0, 0, 0); transition(0); transition(0);
        CHECK(stopped == 2 && !destroyedEntities);
        returnedInt = 1; transition(0);
        CHECK(stopped == 3 && destroyedEntities == 1 && destroyedScripts == 1);
        for (int source = 0; source < 3; ++source) {
            resetCase(1, 6, 0, 0, 1); scriptArgCount = source == 1 ? 2 : 1; scriptPersist = 0;
            if (source == 0) GScr_MapRestart(); else if (source == 1) GScr_LoadMap(); else GScr_ExitLevel();
            CHECK(stopped == 1 && level.finished == 0 && level.savepersist == 1 && !queued[0]);
            CHECK(callbackArgs[0] == 1 && callbackArgs[1] == 1);
            CHECK(level.teamScores[TEAM_AXIS] == 10 && level.teamScores[TEAM_ALLIES] == 20 && !matchEnds);
            CHECK(gameClients[0].sess.connected == CON_CONNECTED);
        }
        resetCase(1, 6, 1, 0, 0); scriptArgCount = 1; scriptPersist = 1;
        GScr_ExitLevel(); CHECK(stopped == 1 && matchEnds == 1 && !strncmp(queued, "map_rotate ", 11));
        Dvar_SetString(sv_mapRotationCurrent, "map fixture_next");
        sscanf(queued, "%31s %63s", cmdName, cmdMap); SV_MapRotate_f();
        CHECK(stopped == 1 && destroyedEntities == 1 && persisted == 1);
        resetCase(1, 6, 0, 0, 0); nestedRequest = 1; transition(0);
        CHECK(stopped == 1 && !queued[0] && level.finished == 0 && !destroyedEntities);
        returnedInt = 1; nestedRequest = 0; transition(0);
        CHECK(stopped == 2 && destroyedEntities == 1);
        resetCase(1, 6, 0, 0, 0); SV_SpawnServer("fixture_direct");
        CHECK(stopped == 1 && !destroyedEntities && !destroyedScripts && !gameplayStarts);
        returnedInt = 1; SV_SpawnServer("fixture_direct");
        CHECK(stopped == 2 && destroyedEntities == 1 && destroyedScripts == 1 && gameplayStarts == 1);
        resetCase(1, 6, 0, 0, 0); scriptActive = 0; transition(0);
        CHECK(!stopped && destroyedEntities == 1 && destroyedScripts == 1 && gameplayStarts == 1);
        /* A recoverable callback error unwinds through Scr_Abort and shutdown. */
        resetCase(1, 6, 1, 0, 0); callbackError = 1;
        if (!setjmp(transitionError)) { transition(0); CHECK(0); }
        callbackError = 0; Scr_Abort(); SV_Shutdown("callback drop");
        CHECK(!com_sv_running->current.enabled && !svs.clients);
        CHECK(destroyedEntities == 1 && destroyedScripts == 1);
        resetCase(1, 6, 0, 0, 0); transition(0);
        CHECK(stopped == 1 && !destroyedEntities);
        /* Failure before the initial server runs must not leave a depth. */
        resetCase(1, 6, 1, 0, 0); Dvar_SetBool(com_sv_running, 0); startupError = 1;
        if (!setjmp(transitionError)) { SV_SpawnServer("failed_start"); CHECK(0); }
        startupError = 0; Scr_Abort(); SV_Shutdown("startup drop");
        resetCase(1, 6, 0, 0, 0); transition(0);
        CHECK(stopped == 1 && !destroyedEntities);
#if STOP_CALLBACK_IMPLEMENTED
        SV_Cod2xEndMapChange(1);
#endif
        /* Real FIFO and wait: an earlier B cannot consume script A's approval. */
        resetCase(1, 6, 1, 0, 0); Cbuf_ExecuteText(2, "wait 1; map fixture_before\n");
        scriptArgCount = 2; scriptPersist = 0; GScr_LoadMap(); CHECK(stopped == 1);
        returnedInt = 0; Cbuf_Execute(); CHECK(!destroyedEntities);
        Cbuf_Execute();
        CHECK(stopped == 2 && destroyedEntities == 1);
        CHECK(strstr(spawnedMap, "fixture_next") != NULL);
        /* Inserted earlier commands, as well as appended commands, stay distinct. */
        resetCase(1, 6, 1, 0, 0); scriptArgCount = 2; GScr_LoadMap();
        Cbuf_ExecuteText(1, "map fixture_inserted"); returnedInt = 0; Cbuf_Execute();
        CHECK(stopped == 2 && destroyedEntities == 1 && strstr(spawnedMap, "fixture_next"));
        /* A matching marker expires before failed BSP/profile validation. */
        for (int failure = 0; failure < 2; ++failure) {
            char tagged[256];
            resetCase(1, 6, 1, 0, 0); scriptArgCount = 2; GScr_LoadMap();
            strcpy(tagged, queued); missingMap = !failure; missingProfile = failure;
            returnedInt = 0; Cbuf_Execute();
            CHECK(stopped == 1 && !destroyedEntities);
            missingMap = missingProfile = 0; Cmd_ExecuteString(tagged);
            CHECK(stopped == 2 && !destroyedEntities);
        }
        /* Altered command form/target consumes the marker without granting it. */
        for (int form = 0; form < 2; ++form) {
            char marker[64], name[32], target[64], altered[256];
            resetCase(1, 6, 1, 0, 0); scriptArgCount = 2; GScr_LoadMap();
            sscanf(queued, "%31s %63s %63s", name, target, marker);
            snprintf(altered, sizeof(altered), "%s %s %s", form ? "devmap" : "map", form ? target : "fixture_wrong", marker);
            returnedInt = 0; Cmd_ExecuteString(altered); Cbuf_Execute();
            CHECK(stopped == 3 && !destroyedEntities && !sv_cheats->current.enabled);
        }
        /* Successfully consumed markers cannot grant a later command. */
        {
            char tagged[256];
            resetCase(1, 6, 1, 0, 0); scriptArgCount = 2; GScr_LoadMap(); strcpy(tagged, queued);
            returnedInt = 0; Cbuf_Execute(); CHECK(stopped == 1 && destroyedEntities == 1);
            scriptActive = 1; optionalCallback = 1; GScr_LoadGameTypeScript(); Cmd_ExecuteString(tagged);
            CHECK(stopped == 2 && destroyedEntities == 1);
        }
        /* A command-buffer overflow leaves no permission for an unqueued marker. */
        resetCase(1, 6, 1, 0, 0); cmd_text->cmdsize = cmd_text->maxsize - 1;
        scriptArgCount = 2; GScr_LoadMap();
#if STOP_CALLBACK_IMPLEMENTED
        CHECK(!cod2xStopQueuedSource && !cod2xStopQueuedMarker[0]);
#endif
        /* Map's temporary string ref is also released after callback longjmp. */
        resetCase(1, 6, 1, 0, 0); callbackError = 1; scriptArgCount = 2;
        if (!setjmp(transitionError)) { GScr_LoadMap(); CHECK(0); }
        CHECK(retainedStrings == 1);
        callbackError = 0; Scr_Abort(); SV_Shutdown("script map drop");
        CHECK(retainedStrings == 0 && !com_sv_running->current.enabled && !svs.clients);
    }
    Dvar_Shutdown(); FINISH("StopGameType commands, script preflight and teardown");
}
