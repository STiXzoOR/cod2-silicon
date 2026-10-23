/* Full production compiler/VM host seams from the existing callback fixture. */
#define main F1_UnusedCallbackMain
#define Scr_GetFunction F1_CallbackFunctionResolver
#define RuntimeError F1_UnusedRuntimeError
#include "../../cod2x/stop_gametype_vm.c"
#undef main
#undef Scr_GetFunction
#undef RuntimeError

static unsigned int probes;
static unsigned int LiveVariables(void)
{
    const VariableValueInternal *list = (const VariableValueInternal *)scrVarGlob;
    unsigned int count = 0, id;
    for (id = 1; id <= 65533; id++)
        count += (list[id].w.status & 0x60) != 0;
    return count;
}
static void Probe(void)
{
    unsigned int iteration = (unsigned int)Scr_GetInt(0);
    fprintf(stderr, "iteration=%u live=%u\n", iteration, LiveVariables());
    probes++;
}
static void Fail(void) { assert(!"incorrect array size"); }
static void SpawnStruct(void)
{
    extern void Scr_AddStruct(void);
    Scr_AddStruct();
}
BuiltinFunction Scr_GetFunction(const char **name, int *type)
{
    *type = 0;
    if (!strcmp(*name, "probe")) return Probe;
    if (!strcmp(*name, "fail")) return Fail;
    if (!strcmp(*name, "spawnstruct")) return SpawnStruct;
    return F1_CallbackFunctionResolver(name, type);
}
void RuntimeError(const char *pos, unsigned int index, const char *message, const char *dialog)
{
    (void)pos; (void)dialog;
    fprintf(stderr, "runtime error[%u]: %s\n", index, message);
    abort();
}
int main(int argc, char **argv)
{
    unsigned int baseline;
    assert(argc == 2);
    Scr_Init(); Scr_InitSystem(1); Scr_BeginLoadScripts();
    if (!strcmp(argv[1], "nested_append")) {
        snprintf(scriptText, sizeof(scriptText),
                 "main() { level.icons=[]; level.icons[\"spectator\"]=spawnstruct(); "
                 "level.icons[\"spectator\"].array=[]; "
                 "for(i=0;i<2000;i++) { add(i); if(level.icons[\"spectator\"].array.size!=i+1) fail(); "
                 "if(i%%200==0) probe(i); } level.icons=undefined; } "
                 "add(i) { a=spawnstruct(); a.flag=i; "
                 "level.icons[\"spectator\"].array[level.icons[\"spectator\"].array.size]=a; }");
    } else {
        assert(!"unknown case");
    }
    assert(Scr_LoadScript("f1_variable_vm"));
    scrCompilePub.programLen = emitted;
    scrVarPub.endScriptBuffer = bytecode + emitted;
    scrCompilePub.script_loading = 0;
    Scr_Settings(0, 0, 0);
    scr_func_t handle = Scr_GetFunctionHandle("f1_variable_vm", "main");
    assert(handle);
    baseline = LiveVariables();
    Scr_FreeThread(Scr_ExecThread(handle, 0));
    fprintf(stderr, "baseline=%u final=%u\n", baseline, LiveVariables());
    assert(probes == 10 && LiveVariables() == baseline);
    puts(argv[1]);
    return 0;
}
