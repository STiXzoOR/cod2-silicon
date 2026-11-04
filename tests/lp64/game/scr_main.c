/* Exercise Mac builtin behavior through the production g_scr_main_mp entry points. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern BuiltinFunction Scr_GetFunction(const char **pName, int *type);
extern void GScr_EndXboxLiveLobby(void);
extern unsigned int Scr_IsSplitscreen(void);
extern void GScr_sin(void);
extern void GScr_cos(void);
extern void GScr_asin(void);
extern void GScr_acos(void);
extern void GScr_atan(void);
extern void Scr_PrecacheShader(void);
extern void GScr_PlaceSpawnPoint(scr_entref_t entref);
extern void Scr_PlayFX(void);

level_locals_t level;
gentity_t g_entities[1024];
vec3_t playerMins, playerMaxs;

static float floatArg, addedFloat;
static const char *stringArg = "", *paramError;
static int errors, prints, traces, numParams = 4;
static int solidOnLastTrace, allOnLastTrace;
static vec3_t vectors[4];
static vec3_t capturedAxis[3];
static gentity_t tempEntity;

static float indexedFloats[4];
static int useIndexedFloats;
float Scr_GetFloat(unsigned int index) { return useIndexedFloats ? indexedFloats[index] : floatArg; }
void Scr_AddFloat(float value) { addedFloat = value; }
void Scr_Error(const char *message) { ++errors; }
void Scr_ParamError(int index, const char *message) { paramError = message; }
const char *Scr_GetString(unsigned int index) { return stringArg; }
int G_ShaderIndex(const char *name) { return 1; }
int Scr_GetNumParam(void) { return numParams; }
int Scr_GetInt(unsigned int index) { return 1; }
void Scr_GetVector(unsigned int index, vec_t *out) { memcpy(out, vectors[index], sizeof(vec3_t)); }
gentity_t *G_TempEntity(const vec_t *origin, int event) { return &tempEntity; }
void G_SetOrigin(gentity_t *ent, const vec_t *origin) {}
void Com_Printf(const char *format, ...) { ++prints; }
void SV_GetConfigstring(int index, char *buffer, int size) { buffer[0] = 0; }
void vectoangles(const vec_t *vec, vec_t *angles) {}
const char *va(const char *format, ...)
{
    static char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}
void G_TraceCapsule(trace_t *tr, const vec_t *start, const vec_t *mins, const vec_t *maxs,
                    const vec_t *end, int pass, int mask)
{
    memset(tr, 0, sizeof(*tr));
    tr->fraction = 1;
    if (++traces == 3) {
        tr->startsolid = solidOnLastTrace;
        tr->allsolid = allOnLastTrace;
    }
}
const vec_t Vec3Normalize(vec_t *v)
{
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len != 0) {
        float scale = 1.0f / len;
        v[0] *= scale; v[1] *= scale; v[2] *= scale;
    }
    return len;
}
void Vec3Cross(const vec_t *a, const vec_t *b, vec_t *out)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}
void AxisToAngles(vec3_t *axis, vec_t *angles) { memcpy(capturedAxis, axis, sizeof(capturedAxis)); }

/* Mac 0x19e631-0x19e643 resolves the alias before touching the entity. */
extern void ScrCmd_PlayLoopSound(scr_entref_t entref);
int G_SoundAliasIndex(const char *name)
{
    assert(g_entities[0].r.broadcastTime == 0);
    return 7;
}

/* Mac stores these at fixed entity and script-constant offsets; the names must agree. */
_Static_assert(__builtin_offsetof(gentity_t, s.origin2) == 0x5c, "looped fx forward");
_Static_assert(__builtin_offsetof(gentity_t, s.angles2) == 0x68, "looped fx cull and repeat");
_Static_assert(__builtin_offsetof(gentity_t, r.clientMask) == 0xf4, "show and hide");
_Static_assert(__builtin_offsetof(scr_const_t, allies) == 0x2, "allies");
_Static_assert(__builtin_offsetof(scr_const_t, axis) == 0x4, "axis");
_Static_assert(__builtin_offsetof(scr_const_t, current) == 0x6, "current");
_Static_assert(__builtin_offsetof(scr_const_t, empty) == 0x10, "empty");
_Static_assert(__builtin_offsetof(scr_const_t, invisible) == 0x1e, "invisible");
_Static_assert(__builtin_offsetof(scr_const_t, trigger_use) == 0x56, "trigger_use");
_Static_assert(__builtin_offsetof(scr_const_t, trigger_use_touch) == 0x58, "trigger_use_touch");
_Static_assert(__builtin_offsetof(scr_const_t, trigger_damage) == 0x5a, "trigger_damage");
_Static_assert(__builtin_offsetof(scr_const_t, none) == 0x74, "none");

/* The builtin names of the Mac table at _functions, in order. */
static const char *const builtinNames[] = {
    "print", "println", "iprintln", "iprintlnbold", "print3d", "line", "getent", "getentarray",
    "spawn", "spawnturret", "precacheturret", "spawnstruct", "assert", "assertex", "assertmsg",
    "isdefined", "isstring", "isalive", "getcvar", "getcvarint", "getcvarfloat", "setcvar",
    "gettime", "getentbynum", "getweaponmodel", "getanimlength", "animhasnotetrack",
    "getbrushmodelcenter", "objective_add", "objective_delete", "objective_state",
    "objective_icon", "objective_position", "objective_onentity", "objective_current",
    "bullettrace", "bullettracepassed", "sighttracepassed", "physicstrace", "getmovedelta",
    "getangledelta", "getnorthyaw", "randomint", "randomfloat", "randomintrange",
    "randomfloatrange", "sin", "cos", "tan", "asin", "acos", "atan", "int", "distance",
    "distancesquared", "length", "lengthsquared", "closer", "vectordot", "vectornormalize",
    "vectortoangles", "anglestoup", "anglestoright", "anglestoforward", "issubstr", "getsubstr",
    "tolower", "strtok", "musicplay", "musicstop", "soundfade", "ambientplay", "ambientstop",
    "precachemodel", "precacheshellshock", "precacheitem", "precacheshader", "precachestring",
    "precacherumble", "loadfx", "playfx", "playfxontag", "playloopedfx", "setcullfog",
    "setexpfog", "grenadeexplosioneffect", "radiusdamage", "setplayerignoreradiusdamage",
    "getnumparts", "getpartname", "earthquake", "newhudelem", "newclienthudelem",
    "newteamhudelem", "resettimeout", "isplayer", "isplayernumber", "setwinningplayer",
    "setwinningteam", "announcement", "clientannouncement", "getteamscore", "setteamscore",
    "setclientnamemode", "updateclientnames", "getteamplayersalive", "objective_team",
    "logprint", "worldentnumber", "obituary", "positionwouldtelefrag", "getstarttime",
    "precachemenu", "precachestatusicon", "precacheheadicon", "map_restart", "exitlevel",
    "addtestclient", "makecvarserverinfo", "setarchive", "allclientsprint", "clientprint",
    "mapexists", "isvalidgametype", "matchend", "setplayerteamrank", "sendranks", "endparty",
    "setvotestring", "setvotetime", "setvoteyescount", "setvotenocount", "openfile",
    "closefile", "fprintln", "freadln", "fgetarg", "kick", "ban", "map", "playrumbleonpos",
    "playlooprumbleonpos", "stopallrumbles", "soundexists", "issplitscreen",
};

/* Mac table row 127 and the strcmp at 0x19b16d. */
static void builtin_table(void)
{
    const char *name = "endparty";
    int type = 1;
    assert(Scr_GetFunction(&name, &type) == (BuiltinFunction)GScr_EndXboxLiveLobby);
    assert(type == 0);
    name = "issplitscreen";
    assert(Scr_GetFunction(&name, &type) == (BuiltinFunction)Scr_IsSplitscreen);
    name = "IsSplitScreen";
    assert(!Scr_GetFunction(&name, &type));
    name = "EndParty";
    assert(!Scr_GetFunction(&name, &type));
    name = "no_such_builtin";
    assert(!Scr_GetFunction(&name, &type));
    assert(sizeof(builtinNames) / sizeof(builtinNames[0]) == 145);
    /* Mac marks exactly these rows as developer builtins, type 1. */
    static const char *const developerNames[] = {
        "print", "println", "print3d", "line", "assert", "assertex", "assertmsg", "getentbynum",
    };
    int developerRows = 0;
    for (unsigned int i = 0; i < 145; ++i) {
        const char *lookup = builtinNames[i];
        int expected = 0;
        for (unsigned int d = 0; d < 8; ++d)
            expected |= !strcmp(lookup, developerNames[d]);
        assert(Scr_GetFunction(&lookup, &type));
        assert(!strcmp(lookup, builtinNames[i]) && type == expected);
        developerRows += expected;
    }
    assert(developerRows == 8);
}

static const double DEG_TO_RAD = 0.017453292519943295, RAD_TO_DEG = 57.29577951308232;

/* Mac multiplies in double and narrows only the result: 0x1989dd, 0x198a1d, 0x198b39,
   0x198bb5 and 0x198bef. */
static void trig_precision(void)
{
    for (int i = -20000; i <= 20000; ++i) {
        floatArg = i * 0.0173f;
        GScr_sin();
        assert(addedFloat == (float)sin((double)floatArg * DEG_TO_RAD));
        GScr_cos();
        assert(addedFloat == (float)cos((double)floatArg * DEG_TO_RAD));
        GScr_atan();
        assert(addedFloat == (float)((double)atanf(floatArg) * RAD_TO_DEG));
        floatArg = i / 20000.0f;
        GScr_asin();
        assert(addedFloat == (float)((double)asinf(floatArg) * RAD_TO_DEG));
        GScr_acos();
        assert(addedFloat == (float)((double)acosf(floatArg) * RAD_TO_DEG));
    }
}

/* Mac text at 0x2dd4b4. */
static void shader_message(void)
{
    level.initializing = 1;
    stringArg = "";
    paramError = NULL;
    Scr_PrecacheShader();
    assert(paramError && !strcmp(paramError, "Shader name string is empty"));
}

/* Mac 0x19f695 tests the byte at trace offset 0x22, allsolid. */
static void spawn_point_warning(void)
{
    static const struct { int start, all, expected; } cases[] = {
        {0, 0, 0}, {1, 0, 0}, {0, 1, 1}, {1, 1, 1},
    };
    scr_entref_t entref;
    memset(&entref, 0, sizeof(entref));
    for (int i = 0; i < 4; ++i) {
        traces = prints = 0;
        solidOnLastTrace = cases[i].start;
        allOnLastTrace = cases[i].all;
        GScr_PlaceSpawnPoint(entref);
        assert(traces == 3);
        assert(prints == cases[i].expected);
    }
}

static float length(const vec_t *v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

/* Mac 0x19fd41-0x19fe2e: the cross product is rescaled, and up rebuilt, only below 0.999. */
static void play_fx_axes(void)
{
    numParams = 4;
    errors = 0;
    memset(vectors, 0, sizeof(vectors));
    vectors[2][0] = 1;

    /* Length 0.9995: stays as computed. */
    vectors[3][0] = 0.03161f;
    vectors[3][2] = 0.9995f;
    Scr_PlayFX();
    const float kept = length(capturedAxis[1]);
    assert(kept < 0.9999f && kept >= 0.999f);
    assert(fabsf(capturedAxis[2][0] - 0.03161f) < 0.0001f);

    /* Length 0.6: rescaled, and up rebuilt perpendicular to forward. */
    vectors[3][0] = 0.8f;
    vectors[3][2] = 0.6f;
    Scr_PlayFX();
    assert(fabsf(length(capturedAxis[1]) - 1) < 0.00001f);
    assert(fabsf(capturedAxis[2][0]) < 0.00001f);
    assert(!errors);

    /* Parallel: reported. */
    vectors[3][0] = 1;
    vectors[3][2] = 0;
    Scr_PlayFX();
    assert(errors == 1);
}

/* Mac 0x1994d1-0x199514: inflictor NULL, attacker the world entity. G_RadiusDamage returns at
   once on a NULL attacker (0x1c45ca), so the reverse order makes the builtin do nothing. */
extern void GScr_RadiusDamage(void);
static const vec_t *damageOrigin;
static gentity_t *damageInflictor, *damageAttacker, *damageIgnore;
static float damageInner, damageOuter, damageRadius;
static int damageMod, damageCalls;
qboolean G_RadiusDamage(const vec_t *origin, gentity_t *inflictor, gentity_t *attacker, float inner,
                        float outer, float radius, gentity_t *ignore, int mod)
{
    damageOrigin = origin;
    damageInflictor = inflictor;
    damageAttacker = attacker;
    damageInner = inner;
    damageOuter = outer;
    damageRadius = radius;
    damageIgnore = ignore;
    damageMod = mod;
    ++damageCalls;
    return 1;
}

static void radius_damage(void)
{
    useIndexedFloats = 1;
    indexedFloats[1] = 300;
    indexedFloats[2] = 200;
    indexedFloats[3] = 50;
    level.bPlayerIgnoreRadiusDamageLatched = 1;
    GScr_RadiusDamage();
    useIndexedFloats = 0;
    assert(damageCalls == 1 && damageOrigin);
    assert(!damageInflictor && damageAttacker == &g_entities[1022] && !damageIgnore);
    assert(damageInner == 200 && damageOuter == 50 && damageRadius == 300 && damageMod == 0xe);
    assert(!level.bPlayerIgnoreRadiusDamage);
}

static void loop_sound_order(void)
{
    scr_entref_t entref;
    memset(&entref, 0, sizeof(entref));
    memset(&g_entities[0], 0, sizeof(g_entities[0]));
    ScrCmd_PlayLoopSound(entref);
    assert(g_entities[0].r.broadcastTime == -1 && g_entities[0].s.loopSound == 7);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    if (!strcmp(argv[1], "loopsound")) { loop_sound_order(); puts("script main loopsound: pass"); return 0; }
    if (!strcmp(argv[1], "radius")) { radius_damage(); puts("script main radius: pass"); return 0; }
    if (!strcmp(argv[1], "table")) builtin_table();
    else if (!strcmp(argv[1], "trig")) trig_precision();
    else if (!strcmp(argv[1], "shader")) shader_message();
    else if (!strcmp(argv[1], "spawn")) spawn_point_warning();
    else if (!strcmp(argv[1], "playfx")) play_fx_axes();
    else return 2;
    printf("script main %s: pass\n", argv[1]);
    return 0;
}
