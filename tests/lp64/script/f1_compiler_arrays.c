/* Full production GSC compilation/VM: nested array append/delete and local churn. */
#define main F1_UnusedArrayCallbackTests
#define Scr_GetFunction F1_ArrayCallbackResolver
#define RuntimeError F1_UnusedArrayRuntimeError
#include "../../cod2x/stop_gametype_vm.c"
#undef main
#undef Scr_GetFunction
#undef RuntimeError

extern void Scr_AddStruct(void);
extern Bool IsVarFree(unsigned int id);
static int observations, checkpoints;
static unsigned int firstUsage, maximumUsage;

static unsigned int usage(void)
{
    unsigned int count = 0;
    for (unsigned int id = 1; id <= 65534; ++id)
        count += !IsVarFree(id);
    return count;
}

static void Checkpoint(void)
{
    if (checkpoints++ % 64)
        return;
    unsigned int count = usage();
    if (!firstUsage)
        firstUsage = count;
    if (count > maximumUsage)
        maximumUsage = count;
    if (count > firstUsage + 8) {
        fprintf(stderr, "array/local churn leaked script vars: first=%u now=%u checkpoint=%d\n", firstUsage, count, checkpoints);
        abort();
    }
}

static void IsDefinedArray(void) { Scr_AddBool(Scr_GetType(0) != 0); }

static void ObserveArray(void)
{
    assert(Scr_GetNumParam() == 1);
    int value = Scr_GetInt(0);
    if (value != 99) {
        fprintf(stderr, "array/local behavior returned %d instead of 99\n", value);
        abort();
    }
    ++observations;
}

BuiltinFunction Scr_GetFunction(const char **name, int *type)
{
    *type = 0;
    if (!strcmp(*name, "spawnstruct")) return Scr_AddStruct;
    if (!strcmp(*name, "checkpoint")) return Checkpoint;
    if (!strcmp(*name, "isdefined")) return IsDefinedArray;
    if (!strcmp(*name, "observe")) return ObserveArray;
    return F1_ArrayCallbackResolver(name, type);
}

void RuntimeError(const char *pos, unsigned int index, const char *message, const char *dialog)
{
    (void)pos; (void)dialog;
    fprintf(stderr, "array/local runtime error[%u]: %s\n", index, message);
    abort();
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Scr_Init(); Scr_InitSystem(1); Scr_BeginLoadScripts();
    if (!strcmp(argv[1], "append-delete"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<2000;i++) { a=[]; a[a.size]=i; a[a.size]=i+1; if(a.size!=2) observe(0); a[a.size-1]=undefined; if(a.size!=1) observe(0); a=undefined; checkpoint(); } observe(99); }");
    else if (!strcmp(argv[1], "nested-append-delete"))
        snprintf(scriptText, sizeof(scriptText), "main() { level.pool=spawnstruct(); level.pool.entries=[]; for(i=0;i<2000;i++) { item=spawnstruct(); item.id=i; level.pool.entries[level.pool.entries.size]=item; if(level.pool.entries.size!=1) observe(0); item=undefined; level.pool.entries[0]=undefined; checkpoint(); } observe(99); }");
    else if (!strcmp(argv[1], "local-branches"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<2000;i++) { if(i%%2) { a=[]; b=i; } else { a=[]; b=i+1; } a[a.size]=b; c=a[0]; if(c<0) observe(0); checkpoint(); } observe(99); }");
    else if (!strcmp(argv[1], "array-copy"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<2000;i++) { a=[]; a[0]=7; b=a; b[b.size]=9; if(a.size!=1 || b.size!=2 || a[0]!=7 || b[1]!=9) observe(0); a=undefined; b=undefined; checkpoint(); } observe(99); }");
    else if (!strcmp(argv[1], "indexed-owner"))
        snprintf(scriptText, sizeof(scriptText), "main() { level.icons=[]; for(i=0;i<3;i++) { level.icons[i]=spawnstruct(); level.icons[i].entries=[]; } for(i=0;i<2000;i++) { key=i%%3; item=spawnstruct(); item.id=i; level.icons[key].entries[level.icons[key].entries.size]=item; last=level.icons[key].entries.size-1; level.icons[key].entries[last]=undefined; checkpoint(); } observe(99); }");
    else if (!strcmp(argv[1], "implicit-string-owner"))
        snprintf(scriptText, sizeof(scriptText), "main() { level.icons[\"group\"]=spawnstruct(); level.icons[\"group\"].entries=[]; for(i=0;i<2000;i++) { key=\"group\"; item=spawnstruct(); item.id=i; level.icons[key].entries[level.icons[key].entries.size]=item; if(level.icons[key].entries.size!=1) observe(0); level.icons[key].entries[0]=undefined; checkpoint(); } observe(99); }");
    else if (!strcmp(argv[1], "remove-loop"))
        snprintf(scriptText, sizeof(scriptText), "main() { level.groups=[]; for(i=0;i<3;i++) { level.groups[i]=spawnstruct(); level.groups[i].data=[]; } for(i=0;i<2000;i++) { key=i%%3; item=spawnstruct(); item.id=i; level.groups[key].data[level.groups[key].data.size]=item; erase(i); if(level.groups[key].data.size!=0) observe(0); checkpoint(); } observe(99); } erase(id) { for(g=0;g<3;g++) { found=undefined; for(j=0;j<level.groups[g].data.size;j++) { if(level.groups[g].data[j].id!=id) continue; found=level.groups[g].data[j]; break; } if(isdefined(found)) { last=level.groups[g].data.size-1; for(j=0;j<level.groups[g].data.size;j++) { if(level.groups[g].data[j]!=found) continue; level.groups[g].data[j]=level.groups[g].data[last]; level.groups[g].data[last]=undefined; break; } } } }");
    else if (!strcmp(argv[1], "call-cleanup"))
        snprintf(scriptText, sizeof(scriptText), "main() { level.pool=[]; for(i=0;i<2000;i++) { add(i); checkpoint(); } observe(99); } add(value) { item=spawnstruct(); item.id=value; level.pool[level.pool.size]=item; level.pool[level.pool.size-1]=undefined; }");
    else abort();
    assert(Scr_LoadScript("f1-arrays"));
    scrCompilePub.programLen = emitted;
    scrVarPub.endScriptBuffer = bytecode + emitted;
    scrCompilePub.script_loading = 0;
    Scr_Settings(0, 0, 0);
    scr_func_t handle = Scr_GetFunctionHandle("f1-arrays", "main");
    assert(handle);
    Scr_FreeThread(Scr_ExecThread(handle, 0));
    assert(observations == 1 && checkpoints == 2000);
    assert(scrVmPub.top == scrVmPub.stack && scrVmPub.function_count == 0);
    assert(scrVmPub.inparamcount == 0 && scrVmPub.outparamcount == 0);
    printf("%s: script vars first=%u max=%u final=%u\n", argv[1], firstUsage, maximumUsage, usage());
    return 0;
}
