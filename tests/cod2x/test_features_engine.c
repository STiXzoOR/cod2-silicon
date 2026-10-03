#include "common_types.h"
#include "cod2x_features.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static dvar_t variables[32];
static int variableCount, argc, setterCalls;
static char *argv[3];
static DvarSetSource lastSource;
static struct { const char *name; void (*function)(void); } commands[3];
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

void Dvar_SetIntFromSource(const dvar_t *var, int value, DvarSetSource source)
{
    ++setterCalls;
    lastSource = source;
    ((dvar_t *)var)->current.integer = value;
}

void Dvar_SetFloatFromSource(const dvar_t *var, float value, DvarSetSource source)
{
    ++setterCalls;
    lastSource = source;
    ((dvar_t *)var)->current.value = value;
}

void Dvar_SetString(const dvar_t *var, const char *value)
{
    ((dvar_t *)var)->current.string = value;
}

void Cmd_AddCommand(const char *name, void (*function)(void))
{
    assert(commandCount < 3);
    commands[commandCount].name = name;
    commands[commandCount++].function = function;
}

int Cmd_Argc(void) { return argc; }
char *Cmd_Argv(int i) { return i < argc ? argv[i] : ""; }
void Com_Printf(const char *format, ...) { (void)format; }
int Cod2x_GameVersion(void) { return 6; }
void Cod2x_PoseInit(void) {}

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
    assert(integer->current.integer == 8 && lastSource == DVAR_SOURCE_EXTERNAL);
    execute("decrease", "test_float", NULL);
    assert(number->current.value == -.75f && lastSource == DVAR_SOURCE_EXTERNAL);
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
    execute("match", "login", "match-player-id");
    assert(Dvar_FindVar("match_login")->flags == 0x12);
    Cod2x_FeaturesFrame(0, 0);
    assert(!strcmp(Dvar_FindVar("match_login")->current.string, "match-player-id"));
    Cod2x_FeaturesFrame(1, 1);
    assert(Cod2x_FeaturesDemo());
    Cod2x_FeaturesFrame(0, 0);
    assert(!Cod2x_FeaturesDemo());
    puts("cod2x client feature command integration: passed");
    return 0;
}
