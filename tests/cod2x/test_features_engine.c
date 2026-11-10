#include "common_types.h"
#include "cod2x.h"
#include "cod2x_features.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static dvar_t variables[32];
static int variableCount, argc, setterCalls;
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
    ((dvar_t *)var)->current.string = value;
}

void Cmd_AddCommand(const char *name, void (*function)(void))
{
    assert(commandCount < 4);
    commands[commandCount].name = name;
    commands[commandCount++].function = function;
}

int Cmd_Argc(void) { return argc; }
char *Cmd_Argv(int i) { return i < argc ? argv[i] : ""; }
void Com_Printf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; }
char *Info_ValueForKey(const char *s, const char *key) { (void)s; (void)key; return (char *)""; }
int Cod2x_GameVersion(void) { return 6; }
void Cod2x_PoseInit(void) {}
const char *Cod2x_IwdRequiredName(void) { return ""; }
/* vMix and the update check's links to the client, the platform and the network: none here. */
void Cod2x_VmixInit(void) {}
void Cod2x_VmixFrame(int clientState) { (void)clientState; }
cg_t cgArray[1];
void CL_Cod2xCDKeyHash(char hash[34]) { hash[0] = '\0'; }
void Sys_Cod2xMachineHistory(Cod2xMachineHistory *out) { memset(out, 0, sizeof(*out)); }
void Sys_Cod2xMachineHistoryClear(void) {}
void Cod2x_Log(const char *format, ...) { (void)format; }
void Cod2x_PostJson(const char *url, const char *json, int timeout, Cod2xPostDone done, void *user)
{
    (void)url, (void)json, (void)timeout, (void)done, (void)user;
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
    execute("match", "LOGIN", "match-player-id");
    assert(Dvar_FindVar("match_login")->flags == 0x12);
    assert(Dvar_FindVar("com_autoaffinity")->flags == 0x1001 && Dvar_FindVar("com_autoaffinity")->current.enabled);
    Cod2x_FeaturesFrame(0, 0);
    assert(!strcmp(Dvar_FindVar("match_login")->current.string, "match-player-id"));
    Cod2x_FeaturesFrame(1, 1);
    assert(Cod2x_FeaturesDemo());
    Cod2x_FeaturesFrame(0, 0);
    assert(!Cod2x_FeaturesDemo());
    {
        /* The connect report: a disconnect clears the server revision. */
        dvar_t *game = (dvar_t *)Dvar_RegisterInt("g_cod2x", 6, 0, 6, 0x18);
        Cod2x_FeaturesClientState(CA_ACTIVE);
        Cod2x_FeaturesClientState(CA_DISCONNECTED);
        assert(game->current.integer == 0 && !game->modified);
    }
    puts("cod2x client feature command integration: passed");
    return 0;
}
