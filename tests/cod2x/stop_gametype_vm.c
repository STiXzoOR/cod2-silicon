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
static char scriptText[2048];
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
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(errorParm_t code, const char *fmt, ...);
static errorParm_t errorcode;
static char com_errorMessage[4096];
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
byte *Scr_AddSourceBuffer(const char *filename, const char *ext, byte *code, int flag)
{
    (void)filename; (void)ext; (void)code; (void)flag;
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
BuiltinFunction Scr_GetFunction(const char **name, int *type)
{
    *type = 0;
    if (!strcmp(*name, "probe_stop")) return ProbeStop;
    if (!strcmp(*name, "map")) return GScr_LoadMap;
    if (!strcmp(*name, "map_restart")) return GScr_MapRestart;
    if (!strcmp(*name, "exitlevel")) return GScr_ExitLevel;
    if (!strcmp(*name, "randomint")) return Scr_RandomInt;
    return NULL;
}
BuiltinMethod Scr_GetMethod(const char **name, int *type) { (void)name; (void)type; return NULL; }
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
void GScr_Cod2xQueueExitLevel(void);
void ExitLevel(void) {
#if defined(COD2_CODX) && COD2_CODX && STOP_VM_CALLBACK_IMPLEMENTED
    GScr_Cod2xQueueExitLevel();
#else
    Cbuf_ExecuteText(2, "map_rotate\n");
#endif
    level.teamScores[0] = level.teamScores[1] = 0;
}
void Hunk_ClearToMark(int mark) { (void)mark; for (size_t i = 0; i < permanentCount; ++i) free(permanent[i]); permanentCount = 0; }
extern void Scr_EndLoadScripts(void);
extern void Scr_EndLoadAnimTrees(void);
#include "stop_vm_game.h"
#include "stop_vm_errors.h"

int main(void)
{
    Scr_Init(); Scr_InitSystem(1);
    const char *returns[] = { "0", "1", "-7", "undefined", "0.0", "\"allow\"" };
    for (int i = 0; i < 6; ++i) {
        Scr_BeginLoadScripts();
        snprintf(scriptText, sizeof(scriptText),
                 "main() {} CodeCallback_StartGameType() {} CodeCallback_PlayerConnect() {} "
                 "CodeCallback_PlayerDisconnect() {} CodeCallback_PlayerDamage() {} CodeCallback_PlayerKilled() {} "
                 "CodeCallback_StopGameType(fromScript, complete, shutdown, source) { probe_stop(fromScript, complete, shutdown, source); map_restart(1); map(\"fixture_next\", false); exitlevel(false); return %s; } "
                 "requestMap() { map(\"fixture_next\", 1); } requestRestart() { map_restart(1); } requestExit() { exitlevel(1); }", returns[i]);
        GScr_LoadGameTypeScript();
        scrCompilePub.programLen = emitted; scrVarPub.endScriptBuffer = bytecode + emitted;
        scrCompilePub.script_loading = 0;
        assert(Scr_GetFunctionHandle("maps/mp/gametypes/_callbacksetup", "CodeCallback_StopGameType"));
#if defined(COD2_CODX) && COD2_CODX
#if STOP_VM_CALLBACK_IMPLEMENTED
        expectedScript = 0; expectedComplete = 1; expectedShutdown = 0; expectedSource = "map";
        assert(GScr_Cod2xBeforeMapChange(0, "map") == (i != 0));
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
#endif
        static const char *requestFunctions[] = { "requestMap", "requestRestart", "requestExit" };
        static const char *requestSources[] = { "map", "fast_restart", "map_rotate" };
        for (int request = 0; request < 3; ++request) {
            level.finished = 0; level.savepersist = 0; level.teamScores[0] = level.teamScores[1] = 7;
            expectedScript = 1; expectedComplete = 0; expectedShutdown = 0; expectedSource = requestSources[request];
            observedStops = queuedTransitions = matchEnds = 0;
            scr_func_t handle = Scr_GetFunctionHandle("maps/mp/gametypes/_callbacksetup", requestFunctions[request]);
            assert(handle);
            Scr_FreeThread(Scr_ExecThread(handle, 0));
            assert(observedStops == 1 && queuedTransitions == (i != 0));
            assert(level.finished == (i == 0 ? 0 : request == 0 ? 2 : request == 1 ? 1 : 3));
            assert(level.savepersist == (i != 0));
            if (request == 0 && i != 0) assert(!strncmp(queued, "map fixture_next ", 17));
            assert(scrVmPub.outparamcount == 0 && scrVmPub.inparamcount == 0 && scrVmPub.top == scrVmPub.stack);
            if (i != 0) {
#if STOP_VM_CALLBACK_IMPLEMENTED
                char command[32], target[64], marker[64];
                if (request == 0) sscanf(queued, "%31s %63s %63s", command, target, marker);
                else { sscanf(queued, "%31s %63s", command, marker); target[0] = 0; }
                assert(GScr_Cod2xConsumeMapChange(requestSources[request], command, target, marker));
                assert(!GScr_Cod2xConsumeMapChange(requestSources[request], command, target, marker));
                assert(observedStops == 1);
#endif
            }
        }
#endif
        level.finished = level.savepersist = 0;
        Scr_EndLoadAnimTrees(); Scr_EndLoadScripts();
    }
#if STOP_VM_CALLBACK_IMPLEMENTED
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
