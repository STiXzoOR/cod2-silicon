/* Real compiler/VM host services; the synthetic builtins exercise error unwind. */
#define main Recovery_UnusedCallbackTests
#define Scr_GetFunction Recovery_CallbackResolver
#define Scr_GetMethod Recovery_MethodResolver
#define RuntimeError Recovery_UnusedRuntimeError
#define Scr_SetObjectField Recovery_UnusedSetObjectField
#define Scr_GetObjectField Recovery_UnusedGetObjectField
#include "../../cod2x/stop_gametype_vm.c"
#undef main
#undef Scr_GetFunction
#undef Scr_GetMethod
#undef RuntimeError
#undef Scr_SetObjectField
#undef Scr_GetObjectField

static unsigned int errors, observed, calls;
static int expectedIndex;
static int partialResult;
static int fieldCase;
static scr_func_t nestedHandle;
extern void Scr_Error(const char *error);
extern void Scr_ParamError(unsigned int index, const char *error);
extern void Scr_AddInt(int value);
extern void Scr_AddEntityNum(int entnum, int classnum);
extern void Scr_SetClassMap(int classnum);
extern unsigned int Scr_GetEntityId(int entnum, int classnum);
extern void AddRefToObject(unsigned int id);
extern void GScr_AddFieldsForEntity(void);
gentity_t g_entities[1024];
void *const imp_g_entities = g_entities;
/* These unrelated bindings must never be reached by the readonly tests. */
void GScr_AddFieldsForClient(void) {}
void Scr_SetClientField(gclient_t *client, int offset) { abort(); }
void Scr_GetClientField(gclient_t *client, int offset) { abort(); }
void Scr_SetHudElemField(int entnum, int offset) { abort(); }
void Scr_GetHudElemField(int entnum, int offset) { abort(); }
void Scr_SetOrigin(gentity_t *ent, int offset) { abort(); }
void Scr_SetAngles(gentity_t *ent, int offset) { abort(); }
void Scr_SetHealth(gentity_t *ent, int offset) { abort(); }
const char *SV_GetConfigstringConst(int index) { return "original model"; }
const char *G_ModelName(int index) { return "original model"; }

static void Fail(void)
{
    assert(Scr_GetNumParam() == 3 || Scr_GetNumParam() == 6);
    ++calls;
    if (partialResult) {
        Scr_MakeArray();
        Scr_AddString("partial result");
        Scr_AddArray();
    }
    Scr_ParamError(1, "synthetic builtin failure");
    abort();
}

static void Observe(void)
{
    assert(Scr_GetNumParam() == 1 && Scr_GetInt(0) == 99);
    assert(scrVmPub.top < scrVmPub.stack + 20);
    ++observed;
}

static void Nested(void)
{
    Scr_FreeThread(Scr_ExecThread(nestedHandle, 0));
    Scr_AddInt(42);
}

static void IsDefined(void) { Scr_AddBool(Scr_GetType(0) != 0); }
static void Entity(void) { Scr_AddEntityNum(17, 0); }

static unsigned int AllocatedValues(void)
{
    const VariableValueInternal *values = (const VariableValueInternal *)scrVarGlob;
    unsigned int count = 0;
    for (unsigned int i = 1; i < 65534; ++i)
        count += (values[i].w.status & 0x60) != 0;
    return count;
}

BuiltinFunction Scr_GetFunction(const char **name, int *type)
{
    *type = 0;
    if (!strcmp(*name, "fail")) return Fail;
    if (!strcmp(*name, "observe")) return Observe;
    if (!strcmp(*name, "nested")) return Nested;
    if (!strcmp(*name, "isdefined")) return IsDefined;
    if (!strcmp(*name, "entity")) return Entity;
    return Recovery_CallbackResolver(name, type);
}

static void FailMethod(scr_entref_t entref)
{
    assert(entref.entnum == 17 && entref.classnum == 0);
    Fail();
}
BuiltinMethod Scr_GetMethod(const char **name, int *type)
{
    if (!strcmp(*name, "failmethod")) { *type = 0; return (BuiltinMethod)FailMethod; }
    return Recovery_MethodResolver(name, type);
}

void RuntimeError(const char *pos, unsigned int index, const char *message, const char *dialog)
{
    assert(pos > bytecode && pos < bytecode + emitted);
    assert(index == (unsigned int)expectedIndex);
    assert(message && *message);
    if (fieldCase && strcmp(message, "Tried to set a read only entity field")) {
        fprintf(stderr, "unexpected field error: %s\n", message);
        abort();
    }
    (void)dialog;
    ++errors;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Scr_Init(); Scr_InitSystem(1); Scr_BeginLoadScripts();
    Scr_SetClassMap(0);
    GScr_AddFieldsForEntity();
    g_entities[17].spawnflags = 7;
    /* Keep the synthetic engine entity alive after script locals release it. */
    AddRefToObject(Scr_GetEntityId(17, 0));
    expectedIndex = 2;
    partialResult = !strcmp(argv[1], "partial") || !strcmp(argv[1], "partial-method") || !strcmp(argv[1], "partial-nested");
    fieldCase = !strncmp(argv[1], "field-", 6);
    if (fieldCase) expectedIndex = 0;
    if (!strcmp(argv[1], "builtin") || !strcmp(argv[1], "partial"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<3000;i++) { a=fail(1,\"two\",[]); if(isdefined(a)) observe(0); } observe(99); } child() {}");
    else if (!strcmp(argv[1], "builtin-many"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<3000;i++) { a=fail(1,\"two\",[],1,\"two\",[]); if(isdefined(a)) observe(0); } observe(99); } child() {}");
    else if (!strcmp(argv[1], "entity-method") || !strcmp(argv[1], "partial-method"))
        snprintf(scriptText, sizeof(scriptText), "main() { ent=entity(); for(i=0;i<3000;i++) { a=ent failmethod(1,\"two\",[]); if(isdefined(a)) observe(0); } observe(99); } child() {}");
    else if (!strcmp(argv[1], "entity-method-many"))
        snprintf(scriptText, sizeof(scriptText), "main() { ent=entity(); for(i=0;i<3000;i++) { a=ent failmethod(1,\"two\",[],1,\"two\",[]); if(isdefined(a)) observe(0); } observe(99); } child() {}");
    else if (!strcmp(argv[1], "method")) {
        expectedIndex = -1;
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<3000;i++) { a=1; a failmethod(1,\"two\",[]); } observe(99); } child() {}");
    } else if ((!strcmp(argv[1], "nested") || !strcmp(argv[1], "partial-nested")))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<100;i++) { if(nested()!=42) observe(0); } observe(99); } child() { a=fail(1,\"two\",[]); if(isdefined(a)) observe(0); }");
    else if (!strcmp(argv[1], "field-generic"))
        snprintf(scriptText, sizeof(scriptText), "main() { ent=entity(); for(i=0;i<1000;i++) { ent.spawnflags=1; ent.classname=\"bad\"; ent.model=\"bad\"; } observe(99); } child() {}");
    else if (!strcmp(argv[1], "field-self"))
        snprintf(scriptText, sizeof(scriptText), "main() { ent=entity(); ent child(); observe(99); } child() { for(i=0;i<1000;i++) { self.spawnflags=1; self.classname=\"bad\"; self.model=\"bad\"; } }");
    else if (!strcmp(argv[1], "field-compound"))
        snprintf(scriptText, sizeof(scriptText), "main() { ent=entity(); for(i=0;i<1000;i++) { ent.spawnflags+=1; ent.spawnflags++; ent.spawnflags--; } observe(99); } child() {}");
    else if (!strcmp(argv[1], "field-self-compound"))
        snprintf(scriptText, sizeof(scriptText), "main() { ent=entity(); ent child(); observe(99); } child() { for(i=0;i<1000;i++) { self.spawnflags+=1; self.spawnflags++; self.spawnflags--; } }");
    else abort();
    assert(Scr_LoadScript("recovery"));
    scrCompilePub.programLen = emitted;
    scrVarPub.endScriptBuffer = bytecode + emitted;
    scrCompilePub.script_loading = 0;
    Scr_Settings(0, 0, 0);
    scr_func_t handle = Scr_GetFunctionHandle("recovery", "main");
    nestedHandle = Scr_GetFunctionHandle("recovery", "child");
    assert(handle && nestedHandle);
    unsigned int valuesBefore = AllocatedValues();
    Scr_FreeThread(Scr_ExecThread(handle, 0));
    assert(errors == ((!strcmp(argv[1], "nested") || !strcmp(argv[1], "partial-nested")) ? 100 : 3000));
    assert(observed == 1);
    assert(calls == (!strcmp(argv[1], "method") || fieldCase ? 0 : errors));
    assert(g_entities[17].spawnflags == 7);
    assert(!g_entities[17].classname && !g_entities[17].model);
    if (AllocatedValues() != valuesBefore)
        fprintf(stderr, "allocated values: before=%u after=%u\n", valuesBefore, AllocatedValues());
    assert(AllocatedValues() == valuesBefore);
    assert(scrVmPub.top == scrVmPub.stack && scrVmPub.function_count == 0);
    assert(scrVmPub.inparamcount == 0 && scrVmPub.outparamcount == 0);
    assert(g_script_error_level == -1);
    puts(argv[1]);
    return 0;
}
