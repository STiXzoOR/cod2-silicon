/* Ordered Mac 1.3 cold/restart initializer effects, through extracted production C. */
#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static level_locals_t level;
static bgs_t level_bgs, *bgs;
static gentity_t g_entities[1024];
static gclient_t g_clients[64];
#define native_clients g_clients
static scr_data_t scriptData;
static void *imp_bgs = &bgs, *imp_g_scr_data = &scriptData;
static dvar_t maxclients, logDvar, syncDvar, gameType, dedicated, password;
static const dvar_t *g_maxclients = &maxclients, *g_log = &logDvar,
    *g_logSync = &syncDvar, *g_gametype = &gameType,
    *g_dedicated = &dedicated, *g_password = &password;
static int events[256], eventCount, treeCount, restartCase, saveCase, failedLog;
static int bannerCount, nameCount, dateCount;
static char treeStorage[72][16];
static XAnim *animSentinel = (XAnim *)(uintptr_t)0x1000;
enum { SETUP=1, REGISTER, BG_REGISTER, BANS, LOG_OPEN, LOG_INFO, LOG_SEPARATOR,
    LOG_INIT, MANTLE, SCRIPT_BEGIN, SCRIPTS, SCRIPT_POST, SCRIPT_END, LOAD_ANIM,
    TREE, CONSTS, GET_WINNER, SET_WINNER, LOCATE, HITLOC, TURRETS, SPAWN,
    FOG, OBJECTIVES, FREE_LIST, SEPARATOR, INIT_SYSTEM, LOADING, GAME_VAR, STRUCTS,
    GAME_TYPE, LEVEL_SCRIPT, STARTUP, SAVE_WEAPONS, SAVE_ITEMS, UNEXPECTED };
static void Record(int event) { assert(eventCount < 256); events[eventCount++] = event; }
static void Com_Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (!strcmp(fmt, "------- Game Initialization -------\n")) ++bannerCount;
    else if (!strcmp(fmt, "gamename: %s\n")) { assert(!strcmp(va_arg(args, const char *), "Call of Duty 2")); ++nameCount; }
    else if (!strcmp(fmt, "gamedate: %s\n")) { assert(!strcmp(va_arg(args, const char *), "Aug 30 2006")); ++dateCount; }
    else if (!strcmp(fmt, "-----------------------------------\n")) Record(SEPARATOR);
    else if (!strcmp(fmt, "WARNING: Couldn't open logfile: %s\n")) assert(!strcmp(va_arg(args, const char *), "fixture.log"));
    else assert(!strcmp(fmt, "Not logging to disk.\n"));
    va_end(args);
}
static void G_InitDbgPrint(const char *fmt, ...) { (void)fmt; }
static void Swap_Init(void) {}
int Sys_DiffSeed(int seed) { assert(seed == 37); return seed; }
static void Rand_Init(int seed) { assert(seed == 37); }
static void G_SetupWeaponDef(void) { Record(SETUP); }
static void G_RegisterDvars_impl(void) { Record(REGISTER); }
static void BG_RegisterDvars(void) { Record(BG_REGISTER); }
static void G_ProcessIPBans(void) { Record(BANS); }
static XModel *GetModel(const char *name) { (void)name; return NULL; }
static void FreeDObj(int handle) { (void)handle; }
static void *imp_SV_XModelGet = (void *)GetModel, *imp_Com_SafeServerDObjFree = (void *)FreeDObj;
static void G_CreateDObjCallback(void *a, unsigned short b, void *c, int d, void *e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
static void *Hunk_AllocXAnimServer(int size) { (void)size; return NULL; }
static void Mantle_CreateAnims(MantleAnimAlloc alloc)
{
    assert(alloc == (MantleAnimAlloc)Hunk_AllocXAnimServer);
    assert(level.openScriptIOFileHandles[0] == -1);
    Record(MANTLE);
}
static int FS_FOpenFileByMode(const char *name, int *handle, int mode)
{
    assert(!strcmp(name, "fixture.log")); assert(mode == (syncDvar.current.enabled ? 3 : 2));
    *handle = failedLog ? 0 : 9; Record(LOG_OPEN); return 0;
}
static void SV_GetServerinfo(char *info, int size) { assert(size == 1024); strcpy(info,"server"); Record(LOG_INFO); }
static void G_LogPrintf(const char *fmt, ...)
{
    if (!strcmp(fmt,"------------------------------------------------------------\n")) Record(LOG_SEPARATOR);
    else if (!strcmp(fmt,"InitGame: %s\n")) Record(LOG_INIT);
    else Record(UNEXPECTED);
}
static void Scr_BeginLoadScripts(void) { assert(bgs == &level_bgs); Record(SCRIPT_BEGIN); }
static void GScr_LoadScripts(int inst) { assert(inst == 1); Record(SCRIPTS); Record(SCRIPT_POST); }
static void Scr_PostCompileScripts(void) { Record(SCRIPT_POST); }
static void Scr_EndLoadScripts(void) { Record(SCRIPT_END); }
static snd_alias_list_t *Com_FindSoundAlias(const char *name) { (void)name; return NULL; }
static int G_AnimScriptSound(int client, snd_alias_list_t *alias) { (void)client; (void)alias; return 0; }
static void BG_LoadAnim(void)
{
    assert(bgs == &level_bgs && level_bgs.animScriptData.numScriptItems == 0);
    assert(level_bgs.animScriptData.soundAlias == Com_FindSoundAlias);
    assert(level_bgs.animScriptData.playSoundAlias == G_AnimScriptSound);
    level_bgs.generic_human.tree.anims = animSentinel;
    level_bgs.animScriptData.animTree.anims = animSentinel;
    Record(LOAD_ANIM);
}
static XAnimTree *XAnimCreateTree(XAnim *anims, Alloc_t alloc)
{
    assert(anims == animSentinel && alloc == (Alloc_t)Hunk_AllocXAnimServer);
    assert(treeCount < 72); Record(TREE); return (XAnimTree *)treeStorage[treeCount++];
}
static void GScr_LoadConsts(void)
{
    assert(treeCount == (restartCase ? 0 : 72));
    for (int i=0; i<64; ++i) assert(level_bgs.clientinfo[i].pXAnimTree == (XAnimTree *)treeStorage[i]);
    for (int i=0; i<8; ++i) assert(scriptData.playerCorpseInfo[i].tree == (XAnimTree *)treeStorage[64+i]);
    Record(CONSTS);
}
static void SV_GetConfigstring(int index, char *text, int size) { assert(index == 22 && size == 1024); strcpy(text,"\\winner\\7\\other\\keep"); Record(GET_WINNER); }
static void Info_SetValueForKey(char *text, const char *key, const char *value) { assert(!strcmp(key,"winner") && !strcmp(value,"0")); strcpy(text,"\\winner\\0\\other\\keep"); }
static void SV_SetConfigstring(int index, const char *text) { assert(index == 22 && !strcmp(text,"\\winner\\0\\other\\keep")); Record(SET_WINNER); }
static void SV_LocateGameData(gentity_t *entities, int count, int entitySize, playerState_t *clients, int clientSize)
{
    assert(entities == g_entities && clients == (playerState_t *)g_clients);
    assert(count == 72 && entitySize == sizeof(gentity_t) && clientSize == sizeof(gclient_t));
    assert(level.clients == g_clients && level.gentities == g_entities && level.maxclients == 2);
    assert(level.num_entities == 72 && !level.firstFreeEnt && !level.lastFreeEnt);
    for (int i=0; i<1024; ++i) { assert(!g_entities[i].r.inuse); assert(g_entities[i].client == (i<2 ? &g_clients[i] : NULL)); }
    for (int i=0; i<64; ++i) assert(!g_clients[i].sess.connected);
    Record(LOCATE);
}
static void G_ParseHitLocDmgTable(void) { Record(HITLOC); }
static void G_InitTurrets(void) { Record(TURRETS); }
static void G_SpawnEntitiesFromString(void) { assert(level.initializing == 1 && bgs == &level_bgs); Record(SPAWN); }
static void G_setfog(const char *fog) { assert(!strcmp(fog,"0")); Record(FOG); }
static void G_InitObjectives(void) { Record(OBJECTIVES); }
static void Scr_FreeEntityList(void) { Record(FREE_LIST); }
static void Scr_InitSystem(int inst) { assert(inst == 1); Record(INIT_SYSTEM); }
static void Scr_SetLoading(int loading) { assert(loading == 1); Record(LOADING); }
static void Scr_AllocGameVariable(void) { Record(GAME_VAR); }
static void G_LoadStructs(void) { assert(level.initializing == 1); Record(STRUCTS); }
static void Scr_LoadGameType(void) { Record(GAME_TYPE); }
static void Scr_LoadLevel(void) { Record(LEVEL_SCRIPT); }
static void Scr_StartupGameType(void) { Record(STARTUP); }
static void SaveRegisteredWeapons(void) { assert(!bgs && !level.initializing); Record(SAVE_WEAPONS); }
static void SaveRegisteredItems(void) { Record(SAVE_ITEMS); }
static int SV_GetBrushModelCount(void) { Record(UNEXPECTED); return 0; }
static void G_SpawnTriggerHurt(int count) { (void)count; Record(UNEXPECTED); }
static void GScr_PostResetTimeout(void) { Record(UNEXPECTED); }
static void Scr_BeginLoadAnimScripts(void) { Record(UNEXPECTED); }
static void GScr_LoadAnimScripts(void) { Record(UNEXPECTED); }
static void Scr_EndLoadAnimScripts(void) { Record(UNEXPECTED); }
static void RestoreBody(void) { Record(UNEXPECTED); }
static void ClientUserinfoChanged(int client) { (void)client; Record(UNEXPECTED); }
static int Com_sprintf(char *dest, int size, const char *fmt, ...) { (void)dest;(void)size;(void)fmt; Record(UNEXPECTED);return 0; }
static void CalculateRanks(void) { Record(UNEXPECTED); }
#include "init_body.inc"
int main(int argc, char **argv)
{
    assert(argc == 2);
    restartCase = !strncmp(argv[1],"restart",7);
    saveCase = strstr(argv[1],"save") != NULL;
    int logging = !strncmp(argv[1],"log",3);
    failedLog = !strcmp(argv[1],"log-failed");
    maxclients.current.integer = 2;
    logDvar.current.string = logging ? "fixture.log" : "";
    syncDvar.current.enabled = !strcmp(argv[1],"log-sync");
    gameType.current.string = "dm"; password.current.string = "";
    memset(&level,0xa5,sizeof(level)); memset(g_entities,0xa5,sizeof(g_entities)); memset(g_clients,0xa5,sizeof(g_clients));
    level_bgs.animScriptData.numScriptItems = 17;
    for (int i=0; i<64; ++i) level_bgs.clientinfo[i].pXAnimTree = (XAnimTree *)treeStorage[i];
    for (int i=0; i<8; ++i) {scriptData.playerCorpseInfo[i].tree = (XAnimTree *)treeStorage[64+i]; scriptData.playerCorpseInfo[i].entnum = 100+i;}
    G_InitGame(1234,37,restartCase,saveCase);
    int expected[256], n=0;
#define EXPECT(event) (expected[n++] = event)
    EXPECT(SETUP);
    if (!restartCase || !saveCase) { EXPECT(REGISTER); EXPECT(BG_REGISTER); }
    EXPECT(BANS);
    if (logging) { EXPECT(LOG_OPEN); if (!failedLog) {EXPECT(LOG_INFO);EXPECT(LOG_SEPARATOR);EXPECT(LOG_INIT);} }
    EXPECT(MANTLE);
    if (!restartCase) { EXPECT(SCRIPT_BEGIN); EXPECT(SCRIPTS); EXPECT(SCRIPT_POST); EXPECT(SCRIPT_END); EXPECT(LOAD_ANIM); for(int i=0;i<72;++i) EXPECT(TREE); }
    EXPECT(CONSTS); EXPECT(GET_WINNER); EXPECT(SET_WINNER); EXPECT(LOCATE); EXPECT(HITLOC); EXPECT(TURRETS); EXPECT(SPAWN); EXPECT(FOG); EXPECT(OBJECTIVES); EXPECT(FREE_LIST); EXPECT(SEPARATOR); EXPECT(INIT_SYSTEM); EXPECT(LOADING); EXPECT(GAME_VAR); EXPECT(STRUCTS); EXPECT(GAME_TYPE); EXPECT(LEVEL_SCRIPT); EXPECT(STARTUP); EXPECT(SAVE_WEAPONS); EXPECT(SAVE_ITEMS);
    assert(bannerCount == 1);
#if defined(COD2_CODX) && COD2_CODX
    assert(!nameCount && !dateCount);
#else
    assert(nameCount == 1 && dateCount == 1);
#endif
    assert(eventCount == n);
    for (int i=0;i<n;++i) { if(events[i] != expected[i]) fprintf(stderr,"event %d: got %d expected %d\n",i,events[i],expected[i]); assert(events[i] == expected[i]); }
    assert(level.time == 1234 && level.startTime == 1234 && level.currentEntityThink == -1);
    assert(level.openScriptIOFileHandles[0] == -1 && !level.openScriptIOFileBuffers[0]);
    for(int i=0;i<8;++i) assert(scriptData.playerCorpseInfo[i].entnum == -1);
    puts("game init contract passes"); return 0;
}
