#include "common_types.h"
#include "cod2x.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static dvar_t vars[32];
static int count;
static int animationResets;

void Cod2x_ResetAnimation(void)
{
    ++animationResets;
}

const dvar_t *Dvar_FindVar(const char *name)
{
    int i;
    for (i = 0; i < count; ++i)
        if (!strcmp(vars[i].name, name))
            return &vars[i];
    return NULL;
}

static dvar_t *add(const char *name, unsigned short flags, byte type)
{
    dvar_t *var = (dvar_t *)Dvar_FindVar(name);
    if (!var) {
        assert(count < 32);
        var = &vars[count++];
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
    var->current.integer = value;
    return var;
}

const dvar_t *Dvar_RegisterString(const char *name, const char *value, unsigned short flags)
{
    dvar_t *var = add(name, flags, 7);
    var->current.string = value;
    return var;
}

void Dvar_SetInt(const dvar_t *var, int value)
{
    assert(var->type == 5);
    assert(value >= var->domain.integer.min && value <= var->domain.integer.max);
    ((dvar_t *)var)->current.integer = value;
}

void Dvar_SetBool(const dvar_t *var, unsigned char value)
{
    assert(var->type == 0);
    ((dvar_t *)var)->current.integer = value;
}

void Dvar_SetString(const dvar_t *var, const char *value)
{
    assert(var->type == 7);
    ((dvar_t *)var)->current.string = value;
}

void Com_Printf(const char *format, ...)
{
    (void)format;
}

static int integer(const char *name)
{
    return Dvar_FindVar(name)->current.integer;
}

int main(void)
{
    const dvar_t *fps = Dvar_RegisterInt("com_maxfps", 333, 0, 1000, 1);
    Dvar_RegisterInt("rate", 5000, 1000, 25000, 3);
    Dvar_RegisterInt("snaps", 20, 1, 40, 3);
    Dvar_RegisterInt("cl_maxpackets", 30, 15, 125, 1);
    Dvar_RegisterBool("sc_enable", 1, 1);
    Dvar_RegisterBool("fx_sort", 0, 1);
    Dvar_RegisterBool("mss_q3fs", 0, 1);
    Cod2x_Init();
    assert(Cod2x_HwidValid(Cod2x_Hwid()));
    assert(Dvar_FindVar("cl_hwid2")->flags == 0x42);
    assert(integer("protocol_cod2x") == 6);
    Dvar_SetString(Dvar_FindVar("cl_hwid2"), "server-supplied-identity");
    Cod2x_PrepareConnect();
    assert(!strcmp(Dvar_FindVar("cl_hwid2")->current.string, Cod2x_Hwid()));

    /* Stock servers keep original settings, even if they send unrelated policy. */
    Dvar_SetBool(Dvar_FindVar("g_competitive"), 1);
    Cod2x_Frame(1, 0);
    assert(!Cod2x_Competitive() && integer("com_maxfps") == 333);
    assert(Cod2x_FrameFPS(0) == 0);

    /* Connecting is exempt; active CoD2x play enforces all seven settings. */
    Dvar_SetInt(Dvar_FindVar("g_cod2x"), 6);
    Cod2x_Frame(0, 0);
    assert(!Cod2x_Competitive());
    Cod2x_Frame(1, 0);
    assert(Cod2x_Competitive());
    assert(integer("com_maxfps") == 250 && fps->domain.integer.max == 250);
    assert(integer("rate") == 25000 && integer("snaps") == 40);
    assert(integer("cl_maxpackets") == 125);
    assert(integer("sc_enable") == 0 && integer("fx_sort") == 1 && integer("mss_q3fs") == 1);
    assert(Cod2x_FrameFPS(0) == 250);
    Dvar_SetInt(fps, 125);
    Cod2x_Frame(1, 0);
    assert(integer("com_maxfps") == 125);

    /* Demo and disable restore original domains/types. */
    Cod2x_Frame(1, 1);
    assert(!Cod2x_Competitive());
    assert(fps->domain.integer.min == 0 && fps->domain.integer.max == 1000);
    assert(Dvar_FindVar("sc_enable")->type == 0);
    assert(Dvar_FindVar("snaps")->domain.integer.min == 1);
    assert(Cod2x_FrameFPS(0) == 0);
    Dvar_SetBool(Dvar_FindVar("g_competitive"), 0);
    Cod2x_Frame(1, 0);
    assert(fps->domain.integer.max == 1000);

    /* Legacy maxfps policy constrains FPS without forcing network/render policy. */
    Dvar_SetInt(fps, 333);
    Dvar_SetBool(Dvar_FindVar("com_maxfps_limit"), 1);
    Cod2x_Frame(1, 0);
    assert(integer("com_maxfps") == 250 && fps->domain.integer.max == 250);
    assert(Dvar_FindVar("rate")->domain.integer.min == 1000);
    Cod2x_Disconnect();
    assert(animationResets == 1);
    assert(Cod2x_GameVersion() == 0 && !integer("g_competitive") && !integer("com_maxfps_limit"));
    assert(fps->domain.integer.max == 1000 && Cod2x_FrameFPS(333) == 333);
    puts("cod2x: runtime policy, demo, reconnect, immutable identity tests passed");
    return 0;
}
