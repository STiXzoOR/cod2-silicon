#include "common_types.h"
#include "stop_vm_features.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

scrVarPub_t scrVarPub;
scrCompilePub_t scrCompilePub;
scrParserPub_t scrParserPub;
scrAnimPub_t scrAnimPub;
scrMemTreePub_t scrMemTreePub;
scrVmPub_t scrVmPub;
unsigned char scrVarGlob[1048608] __attribute__((aligned(8)));
unsigned char scrMemTreeGlob[sizeof(scrMemTreeGlob_t)] __attribute__((aligned(8)));
unsigned char scrStringGlob[sizeof(scrStringGlob_t)] __attribute__((aligned(8)));
unsigned char scrVmGlob[sizeof(scrVmGlob_t)] __attribute__((aligned(8)));
unsigned char scrCompileGlob[sizeof(scrCompileGlob_t)] __attribute__((aligned(8)));
void *const imp_scrVarPub = &scrVarPub;
void *imp_scrVmPub = &scrVmPub;
void *imp_scrCompilePub = &scrCompilePub, *imp_scrParserPub = &scrParserPub;
void *imp_scrAnimPub = &scrAnimPub, *imp_scrCompileGlob = scrCompileGlob;
scr_classStruct_t g_classMap[5];
jmp_buf g_script_error[33];
int g_script_error_level, yy_init = 1;
char g_EndPos, ch_buf[0x4002];
level_locals_t level;
scr_data_t g_scr_data;
static dvar_t gametype = { .current.string = "dm" };
const dvar_t *g_gametype = &gametype;
static char bytecode[65536] __attribute__((aligned(8)));
int currentPos;
#define emitted currentPos
static char scriptText[4096];
static void *nodes[4096];
static size_t nodeCount;
static void *permanent[32];
static size_t permanentCount;
static int expectedScript, expectedComplete, expectedShutdown, observedStops, queuedTransitions, matchEnds;
static const char *expectedSource;
static char queued[128];
static cmd_t fixtureCommandBuffer;
cmd_t *cmd_text = &fixtureCommandBuffer;
extern void Scr_Init(void);
extern void Scr_InitSystem(int inst);
extern void Scr_BeginLoadScripts(void);
extern unsigned int Scr_LoadScript(const char *filename);
extern scr_func_t Scr_GetFunctionHandle(const char *filename, const char *name);
extern int Scr_IsSystemActive(int inst);
extern void Scr_AddString(const char *value);
extern void Scr_AddBool(int value);
extern void Scr_AddExecThread(scr_func_t handle, unsigned int count);
extern int Scr_GetType(unsigned int index);
extern int Scr_GetInt(unsigned int index);
extern unsigned int Scr_GetNumParam(void);
extern unsigned int Scr_GetConstString(unsigned int index);
extern const char *Scr_GetString(unsigned int index);
extern void SL_AddRefToString(unsigned int value);
extern void SL_RemoveRefToString(unsigned int value);
extern scr_thread_t Scr_ExecThread(scr_func_t handle, unsigned int count);
extern void Scr_FreeThread(int handle);
void GScr_MapRestart(void);
void GScr_LoadMap(void);
void GScr_ExitLevel(void);
void Scr_RandomInt(void);
extern void Scr_Error(const char *text);
extern void Scr_AddInt(int value);
extern void Scr_Abort(void);
extern void Scr_Settings(int developer, int developerScript, int abortOnError);
extern void RemoveRefToValue(int type, VariableUnion value);
static char printed[4096];
void Com_Printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vsnprintf(printed, sizeof(printed), fmt, ap); va_end(ap); }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(errorParm_t code, const char *fmt, ...);
static errorParm_t errorcode;
static char com_errorMessage[4096];
const void *com_errorCaller;
qboolean com_errorEntered, com_fixedConsolePosition;
static jmp_buf recoverableDrop;
static clientStatic_t fixtureCls;
void *imp_cls = &fixtureCls;
void *Sys_GetValue(int slot) { assert(slot == 2); return &recoverableDrop; }
void Sys_Error(const char *fmt, ...) { (void)fmt; abort(); }
void CL_ConsoleFixPosition(void) {}
qboolean UI_AnyFullScreenMenuVisible(void) { return 0; }
qboolean UI_SetActiveMenu(int menu) { (void)menu; return 1; }
void Com_SetErrorMessage(const char *message) { (void)message; }
void I_strncpyz(char *out, const char *in, int size) { snprintf(out, size, "%s", in); }
int irand(int min, int max) { (void)max; return min; }
void Com_PrintMessage(print_msg_type_t type, const char *message) { (void)type; (void)message; }
int Com_sprintf(char *dest, int size, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int n = vsnprintf(dest, size, fmt, ap); va_end(ap); return n;
}
char *va(const char *fmt, ...)
{
    static char text[4096]; va_list ap; va_start(ap, fmt); vsnprintf(text, sizeof(text), fmt, ap); va_end(ap); return text;
}
void CompileError(unsigned int pos, const char *fmt, ...)
{
    fprintf(stderr, "compile error at %u: ", pos); va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); abort();
}
void CompileError2(const char *pos, const char *fmt, ...) { (void)pos; fprintf(stderr, "%s\n", fmt); abort(); }
void *Z_MallocInternal(int size) { return calloc(1, size); }
#if defined(COD2_X64)
void *Z_TryMallocInternal(int size) { return calloc(1, size); }
#endif
void Z_FreeInternal(void *p) { free(p); }
void *Hunk_AllocateTempMemoryHighInternal(int size)
{
    assert(nodeCount < 4096); return nodes[nodeCount++] = calloc(1, size);
}
void Hunk_ClearTempMemoryHigh(void) { for (size_t i = 0; i < nodeCount; ++i) free(nodes[i]); nodeCount = 0; }
void *Hunk_ReallocateTempMemoryInternal(int size) { assert(size >= 0 && size < sizeof(bytecode)); return bytecode; }
void *Hunk_AllocLowInternal(int size) { assert(size == 0); return bytecode; }
void *Hunk_AllocInternal(int size) { assert(permanentCount < 32); return permanent[permanentCount++] = calloc(1, size); }
int Hunk_SetMark(void) { return 0; }
void Scr_InitOpcodeLookup(void) {}
void AddOpcodePos(unsigned int pos, int type) { (void)pos; (void)type; }
void RemoveOpcodePos(void) {}
void AddThreadStartOpcodePos(unsigned int pos) { (void)pos; }
#if defined(COD2_CODX) && COD2_CODX && defined(DEBUG) && DEBUG
static const char debugTests[] =
    "callback_test_onStartGameType(greeting) { probe_debug(1, greeting); } "
    "callback_test_onStartGameType2(greeting) { probe_debug(2, greeting); return 1337; } "
    "callback_test_onStartGameType3() { probe_debug(3); } "
    "callback_test_onPlayerConnect() { probe_debug(5); } "
    "callback_test_onPlayerConnect2(greeting) { probe_debug(6, greeting); return 1338; } ";
static const char debugMatch[] =
    "callback_match_onStartGameType() { probe_debug(4); } "
    "callback_match_onPlayerConnect() { probe_debug(7); } "
    "callback_match_onStopGameType(a,b,c,d) { probe_debug_stop(a,b,c,d); return 0; } ";
#endif
byte *Scr_AddSourceBuffer(const char *filename, const char *ext, byte *code, int flag)
{
    (void)filename; (void)ext; (void)code; (void)flag;
#if defined(COD2_CODX) && COD2_CODX && defined(DEBUG) && DEBUG
    if (strstr(filename, "_callback_tests")) { scrParserPub.sourceBuf = (char *)debugTests; return (byte *)debugTests; }
    if (strstr(filename, "_callback_match")) { scrParserPub.sourceBuf = (char *)debugMatch; return (byte *)debugMatch; }
#endif
    scrParserPub.sourceBuf = scriptText; return (byte *)scriptText;
}
static void ProbeStop(void)
{
    assert(Scr_GetNumParam() == 4);
    assert(Scr_GetType(0) == 6 && Scr_GetInt(0) == expectedScript);
    assert(Scr_GetType(1) == 6 && Scr_GetInt(1) == expectedComplete);
    assert(Scr_GetType(2) == 6 && Scr_GetInt(2) == expectedShutdown);
    assert(Scr_GetType(3) == 2 && !strcmp(Scr_GetString(3), expectedSource));
    ++observedStops;
}
#if defined(COD2_CODX) && COD2_CODX && defined(DEBUG) && DEBUG
extern float Scr_GetFloat(unsigned int index);
extern void Scr_GetVector(unsigned int index, float *value);
extern const char *Scr_GetIString(unsigned int index);
extern int Scr_GetFunc(unsigned int index);
extern const char *Scr_GetTypeName(unsigned int index);
extern void Scr_AddFloat(float value);
extern void Scr_AddVector(const float *value);
extern void Scr_AddString(const char *value);
extern void Scr_AddUndefined(void);
extern void Scr_MakeArray(void);
extern void Scr_AddArray(void);
extern void Scr_AddExecEntThreadNum(int, int, scr_func_t, unsigned int);
extern scr_thread_t Scr_ExecEntThreadNum(int, int, scr_func_t, unsigned int);
extern void Scr_SetClassMap(int classnum);
extern void Scr_RemoveClassMap(int classnum);
static client_t debugClients[64];
serverStatic_t svs = { .clients = debugClients };
static int debugTrace[64], debugTraceCount, debugStops, debugValues, debugPassed;
static void (*debugCommand)(void);
static const char *debugArgs[3];
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int Cmd_Argc(void) { return debugArgs[1] ? (debugArgs[2] ? 3 : 2) : 1; }
const char *Cmd_Argv(int n) { return n < 3 && debugArgs[n] ? debugArgs[n] : ""; }
void Cmd_AddCommand(const char *name, void (*call)(void)) { assert(!strcmp(name, "ws")); debugCommand = call; }
#include "stop_vm_debug.h"
static void ProbeDebug(void)
{
    int stage = Scr_GetInt(0);
    if (stage == 1 || stage == 2 || stage == 6)
        assert(Scr_GetNumParam() == 2 && !strcmp(Scr_GetString(1), "hello from CoD2x"));
    else
        assert(Scr_GetNumParam() == 1);
    if (debugTraceCount < 64) debugTrace[debugTraceCount++] = stage;
}
static void ProbeDebugStop(void)
{
    assert(Scr_GetNumParam() == 4);
    assert(Scr_GetType(0) == 6 && Scr_GetType(1) == 6 && Scr_GetType(2) == 6 && Scr_GetType(3) == 2);
    ++debugStops;
}
static void ProbeDebugValues(void)
{
    vec3_t vector;
    assert(Scr_GetNumParam() == 10);
    assert(Scr_GetType(0) == 0);
    /* Windows Scr_AddBool (0x483530) stores the integer unchanged. */
    assert(Scr_GetInt(1) == 2 && Scr_GetInt(2) == -701);
    assert(Scr_GetFloat(3) == 1.25f && !strcmp(Scr_GetString(4), "return string"));
    Scr_GetVector(5, vector); assert(vector[0] == 9 && vector[1] == 8 && vector[2] == 7);
    assert(Scr_GetInt(6) == 10 && Scr_GetInt(7) == 20 && Scr_GetInt(8) == 30);
    assert(!strcmp(Scr_GetString(9), "test player"));
    ++debugValues;
}
static void ProbeDebugPassed(void) { ++debugPassed; }
static void GScr_Cod2xTestReturn(const char *, const char *, int);
void SV_Cod2xMatchStartGameType(void) { debugTrace[debugTraceCount++] = 40; }
scr_thread_t Scr_ExecEntThread(gentity_t *self, scr_func_t handle, unsigned int count)
{ return Scr_ExecEntThreadNum(self->s.number, 0, handle, count); }
#define COD2_DEBUG_ENV(name) 0
#endif
BuiltinFunction Scr_GetFunction(const char **name, int *type)
{
    *type = 0;
    if (!strcmp(*name, "probe_stop")) return ProbeStop;
    if (!strcmp(*name, "map")) return GScr_LoadMap;
    if (!strcmp(*name, "map_restart")) return GScr_MapRestart;
    if (!strcmp(*name, "exitlevel")) return GScr_ExitLevel;
    if (!strcmp(*name, "randomint")) return Scr_RandomInt;
#if defined(COD2_CODX) && COD2_CODX && defined(DEBUG) && DEBUG
    if (!strcmp(*name, "probe_debug")) return ProbeDebug;
    if (!strcmp(*name, "probe_debug_stop")) return ProbeDebugStop;
    if (!strcmp(*name, "probe_debug_values")) return ProbeDebugValues;
    if (!strcmp(*name, "probe_debug_passed")) return ProbeDebugPassed;
    return (BuiltinFunction)GScr_Cod2xGetFunction(name, type);
#endif
    return NULL;
}
BuiltinMethod Scr_GetMethod(const char **name, int *type) {
#if defined(COD2_CODX) && COD2_CODX && defined(DEBUG) && DEBUG
    if (!strcasecmp(*name, "test_playerGetName")) { *name = "test_playerGetName"; *type = 0; return (BuiltinMethod)GScr_Cod2xTestPlayerGetName; }
#endif
    (void)name; (void)type; return NULL;
}
#if defined(COD2_X64)
void Scr_ResetAnimRefs(void) {}
#endif
void Scr_UsingTree(const char *filename, unsigned int pos) { (void)filename; (void)pos; abort(); }
void Scr_EmitAnimation(char *pos, unsigned int animName, unsigned int source) { (void)pos; (void)animName; (void)source; abort(); }
void Scr_PrintPrevCodePos(print_msg_type_t type, const char *pos, unsigned int index) { (void)type; (void)pos; (void)index; }
void dbg_end_probe(int probe) { (void)probe; }
static const char *typeNames[24] = { "undefined", "object", "string", "localized string", "vector", "float", "int" };
void *imp_var_typename = typeNames;
int cod2_native_trace_enabled;
void *Z_VirtualAllocInternal(int size) { return calloc(1, size); }
void Z_VirtualFreeInternal(void *p) { free(p); }
int Scr_GetSourceBuffer(const char *pos) { (void)pos; return 0; }
struct XAnim_s *Scr_GetAnims(unsigned int tree) { (void)tree; return NULL; }
const char *XAnimGetAnimDebugName(const struct XAnim_s *anims, unsigned int index) { (void)anims; (void)index; return ""; }
void Scr_GetObjectField(unsigned int classnum, int entnum, int offset) { (void)classnum; (void)entnum; (void)offset; abort(); }
int Scr_SetObjectField(unsigned int classnum, int entnum, int offset) { (void)classnum; (void)entnum; (void)offset; abort(); }
qboolean SV_MapExists(const char *map) { assert(!strcmp(map, "fixture_next")); return 1; }
void Cbuf_ExecuteText(int when, const char *text) { assert(when == 2); ++queuedTransitions; cmd_text->cmdsize += strlen(text); snprintf(queued, sizeof(queued), "%s", text); }
void SV_MatchEnd(void) { ++matchEnds; }
void ExitLevel(void);
#include "stop_vm_game.h"
void ExitLevel(void) {
    Cbuf_ExecuteText(2, "map_rotate\n");
    level.teamScores[0] = level.teamScores[1] = 0;
}
void Hunk_ClearToMark(int mark) { (void)mark; for (size_t i = 0; i < permanentCount; ++i) free(permanent[i]); permanentCount = 0; }
extern void Scr_EndLoadScripts(void);
extern void Scr_EndLoadAnimTrees(void);
#include "stop_vm_errors.h"

static void ClearRequest(void)
{
    level.finished = level.savepersist = 0;
    observedStops = queuedTransitions = matchEnds = 0;
    queued[0] = 0;
}

int main(void)
{
    Scr_Init(); Scr_InitSystem(1);
#if defined(COD2_CODX) && COD2_CODX && defined(DEBUG) && DEBUG
    GScr_Cod2xDebugInit(); assert(debugCommand);
    debugArgs[0] = "ws"; debugArgs[1] = NULL; printed[0] = 0; debugCommand(); assert(!printed[0]);
    debugArgs[1] = "send"; debugCommand(); assert(strstr(printed, "Please provide a message"));
    debugArgs[2] = "message"; debugCommand(); assert(strstr(printed, "not connected"));
    debugArgs[1] = "reconnect"; debugCommand(); assert(strstr(printed, "reconnecting"));
    debugArgs[1] = "send"; debugCommand(); assert(strstr(printed, "not connected"));
    debugArgs[1] = "close"; debugCommand(); assert(strstr(printed, "connection closed"));
    debugArgs[1] = "other"; debugCommand(); assert(strstr(printed, "Unknown match command: other"));
    Scr_BeginLoadScripts();
    snprintf(scriptText, sizeof(scriptText),
        "main() {} CodeCallback_StartGameType() { probe_debug(9); } CodeCallback_PlayerConnect() { probe_debug(10); } "
        "CodeCallback_PlayerDisconnect() {} CodeCallback_PlayerDamage() {} CodeCallback_PlayerKilled() {} "
        "CodeCallback_StopGameType(a,b,c,d) { return 1; } "
        "passed() { probe_debug_passed(); } "
        "values() { a = test_returnArray(); probe_debug_values(test_returnUndefined(), test_returnBool(2), "
        "test_returnInt(-701), test_returnFloat(1.25), test_returnString(\"return string\"), "
        "test_returnVector((9,8,7)), a[0], a[1], a[2], self test_playerGetName()); "
        "test_getAll(true,1,2.222,\"variable test string\", &\"Localized text string\",(1,2,3),::passed); "
        "test_allOk(\"all ok\"); return 1338; }");
    GScr_LoadGameTypeScript();
    scrCompilePub.programLen = emitted; scrVarPub.endScriptBuffer = bytecode + emitted;
    scrCompilePub.script_loading = 0;
    Scr_SetClassMap(0);
    Scr_StartupGameType();
    const int startOrder[] = {1, 2, 3, 40, 4, 9};
    assert(debugTraceCount == 6 && !memcmp(debugTrace, startOrder, sizeof(startOrder)));
    debugTraceCount = 0;
    gentity_t player = {0}; player.s.number = 7;
    Scr_PlayerConnect(&player);
    const int connectOrder[] = {5, 6, 7, 10};
    assert(debugTraceCount == 4 && !memcmp(debugTrace, connectOrder, sizeof(connectOrder)));
    strcpy(debugClients[7].name, "test player");
    scr_func_t values = Scr_GetFunctionHandle("maps/mp/gametypes/_callbacksetup", "values");
    Scr_AddExecEntThreadNum(7, 0, values, 0);
    assert(Scr_GetType(0) == 6 && Scr_GetInt(0) == 1338);
    assert(debugValues == 1 && debugPassed == 1 && strstr(printed, "GSC test OK"));
    while (scrVmPub.outparamcount) { RemoveRefToValue(scrVmPub.top->type, scrVmPub.top->u); --scrVmPub.top; --scrVmPub.outparamcount; }
    assert(scrVmPub.top == scrVmPub.stack && !scrVmPub.inparamcount);
    debugStops = 0;
    assert(GScr_Cod2xBeforeMapChange(0, "map") && debugStops == 1);
    assert(scrVmPub.top == scrVmPub.stack && !scrVmPub.inparamcount && !scrVmPub.outparamcount);
    Scr_RemoveClassMap(0);
    Scr_EndLoadAnimTrees(); Scr_EndLoadScripts();
    puts("DEBUG GSC: nine typed functions, entity name, eight required callbacks, start/connect order, entity return 1338 and unpolled ws command passed");
#endif
    const char *returns[] = { "0", "1", "-7", "undefined", "0.0", "\"allow\"" };
    for (int i = 0; i < 6; ++i) {
        Scr_BeginLoadScripts();
        snprintf(scriptText, sizeof(scriptText),
                 "main() {} CodeCallback_StartGameType() {} CodeCallback_PlayerConnect() {} "
                 "CodeCallback_PlayerDisconnect() {} CodeCallback_PlayerDamage() {} CodeCallback_PlayerKilled() {} "
                 "CodeCallback_StopGameType(fromScript, complete, shutdown, source) { probe_stop(fromScript, complete, shutdown, source); return %s; } "
                 "requestMap() { map(\"fixture_next\", 1); } requestRestart() { map_restart(1); } requestExit() { exitlevel(1); }", returns[i]);
        GScr_LoadGameTypeScript();
        scrCompilePub.programLen = emitted; scrVarPub.endScriptBuffer = bytecode + emitted;
        scrCompilePub.script_loading = 0;
        assert(Scr_GetFunctionHandle("maps/mp/gametypes/_callbacksetup", "CodeCallback_StopGameType"));
#if defined(COD2_CODX) && COD2_CODX && STOP_VM_CALLBACK_IMPLEMENTED
        /* Only the integer 0 blocks; a shutdown is never blocked. */
        ClearRequest();
        expectedScript = 0; expectedComplete = 1; expectedShutdown = 0; expectedSource = "map";
        assert(GScr_Cod2xBeforeMapChange(0, "map") == (i != 0));
        assert(observedStops == 1);
        assert(scrVmPub.outparamcount == 0 && scrVmPub.inparamcount == 0 && scrVmPub.top == scrVmPub.stack);
        expectedShutdown = 1; expectedSource = "shutdown";
        assert(GScr_Cod2xBeforeMapChange(1, "shutdown"));
        assert(scrVmPub.outparamcount == 0 && scrVmPub.inparamcount == 0 && scrVmPub.top == scrVmPub.stack);
        for (int finished = 1; finished <= 3; ++finished) {
            level.finished = finished;
            expectedScript = 1; expectedShutdown = 0; expectedSource = "map_restart";
            assert(GScr_Cod2xBeforeMapChange(0, "map_restart") == (i != 0));
            assert(scrVmPub.outparamcount == 0 && scrVmPub.inparamcount == 0 && scrVmPub.top == scrVmPub.stack);
        }
        /* The match-cancel bypass lets exactly one blocked change through. */
        ClearRequest();
        expectedScript = 0; expectedComplete = 1; expectedShutdown = 0; expectedSource = "map";
        GScr_Cod2xAllowOneMapChange();
        assert(GScr_Cod2xBeforeMapChange(0, "map"));
        assert(GScr_Cod2xBeforeMapChange(0, "map") == (i != 0));
        assert(observedStops == 2);
#endif
        /* map(), map_restart() and exitLevel() only queue their command, as in
           the stock game; the callback runs when that command does. */
        static const char *requestFunctions[] = { "requestMap", "requestRestart", "requestExit" };
        static const char *requestSources[] = { "map", "fast_restart", "map_rotate" };
        static const char *requestCommands[] = { "map fixture_next\n", "fast_restart\n", "map_rotate\n" };
        for (int request = 0; request < 3; ++request) {
            ClearRequest();
            level.teamScores[0] = level.teamScores[1] = 7;
            scr_func_t handle = Scr_GetFunctionHandle("maps/mp/gametypes/_callbacksetup", requestFunctions[request]);
            assert(handle);
            Scr_FreeThread(Scr_ExecThread(handle, 0));
            assert(observedStops == 0 && queuedTransitions == 1 && !strcmp(queued, requestCommands[request]));
            assert(level.finished == (request == 0 ? 2 : request == 1 ? 1 : 3) && level.savepersist == 1);
            assert(matchEnds == (request == 2));
            assert(level.teamScores[0] == (request == 2 ? 0 : 7));
            assert(scrVmPub.outparamcount == 0 && scrVmPub.inparamcount == 0 && scrVmPub.top == scrVmPub.stack);
#if defined(COD2_CODX) && COD2_CODX && STOP_VM_CALLBACK_IMPLEMENTED
            expectedScript = 1; expectedComplete = 0; expectedShutdown = 0; expectedSource = requestSources[request];
            assert(GScr_Cod2xBeforeMapChange(0, requestSources[request]) == (i != 0));
            assert(observedStops == 1);
            /* A blocked change leaves the level finished and exitLevel's scores reset. */
            assert(level.finished == (request == 0 ? 2 : request == 1 ? 1 : 3) && level.savepersist == 1);
            assert(level.teamScores[0] == (request == 2 ? 0 : 7));
#else
            (void)requestSources;
#endif
        }
        ClearRequest();
        Scr_EndLoadAnimTrees(); Scr_EndLoadScripts();
    }
#if defined(COD2_CODX) && COD2_CODX && STOP_VM_CALLBACK_IMPLEMENTED
    Scr_BeginLoadScripts();
    snprintf(scriptText, sizeof(scriptText),
             "main() {} CodeCallback_StartGameType() {} CodeCallback_PlayerConnect() {} "
             "CodeCallback_PlayerDisconnect() {} CodeCallback_PlayerDamage() {} CodeCallback_PlayerKilled() {} "
             "CodeCallback_StopGameType(a,b,c,d) { randomint(0); }");
    GScr_LoadGameTypeScript();
    scrCompilePub.programLen = emitted; scrVarPub.endScriptBuffer = bytecode + emitted;
    scrCompilePub.script_loading = 0;
    Scr_Settings(1, 1, 1);
    if (!setjmp(recoverableDrop)) { GScr_Cod2xBeforeMapChange(0, "map"); assert(0); }
    assert(errorcode == ERR_DROP && com_errorEntered);
    Scr_Abort();
    assert(GScr_Cod2xBeforeMapChange(1, "shutdown"));
    com_errorEntered = com_fixedConsolePosition = 0;
    Scr_EndLoadAnimTrees(); Scr_EndLoadScripts();
    Scr_BeginLoadScripts();
    snprintf(scriptText, sizeof(scriptText),
             "main() {} CodeCallback_StartGameType() {} CodeCallback_PlayerConnect() {} "
             "CodeCallback_PlayerDisconnect() {} CodeCallback_PlayerDamage() {} CodeCallback_PlayerKilled() {}");
    GScr_LoadGameTypeScript();
    assert(Scr_GetFunctionHandle("maps/mp/gametypes/_callbacksetup", "CodeCallback_StopGameType") == 0);
    observedStops = 0;
    assert(GScr_Cod2xBeforeMapChange(0, "map") && GScr_Cod2xBeforeMapChange(1, "shutdown"));
    assert(observedStops == 0);
    Scr_EndLoadAnimTrees(); Scr_EndLoadScripts();
#endif
    puts("real synthetic GSC loading, callback lookup and VM return interpretation passed");
    return 0;
}
