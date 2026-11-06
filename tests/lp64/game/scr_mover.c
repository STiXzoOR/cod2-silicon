/* Exercise Mac script mover behavior through the production g_scr_mover_mp entry points. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

extern void ScriptEntCmdGetCommandTimes(float *total, float *accel, float *decel);
extern void Reached_ScriptMover(gentity_t *ent);
extern void ScriptEntCmd_MoveTo(scr_entref_t entref);
extern void ScriptEntCmd_Solid(scr_entref_t entref);
extern void ScriptEntCmd_NotSolid(scr_entref_t entref);

level_locals_t level;
gentity_t g_entities[16];
scr_const_t scr_const;

static float floats[4];
static vec3_t vectorArg;
static int params, paramErrors, lastParam, errors, notifies, links;
static unsigned short lastNotify;
static const vec3_t evaluatedGravity = {7, 8, 9}, evaluatedMoving = {10, 0, 0};

float Scr_GetFloat(int index) { return floats[index]; }
void Scr_GetVector(unsigned int index, vec_t *out) { memcpy(out, vectorArg, sizeof(vec3_t)); }
unsigned int Scr_GetNumParam(void) { return params; }
void Scr_ParamError(int index, const char *message) { ++paramErrors; lastParam = index; }
void Scr_Error(const char *message) { ++errors; }
void Scr_ObjectError(const char *message) { ++errors; }
void Scr_Notify(gentity_t *ent, unsigned short name, unsigned int count) { ++notifies; lastNotify = name; }
const char *va(const char *format, ...) { return format; }
void Com_DPrintf(const char *format, ...) {}
void SV_LinkEntity(gentity_t *ent) { ++links; }
const float AngleNormalize180(const float angle) { return angle; }
const float AngleNormalize360(const float angle) { return angle; }
void BG_EvaluateTrajectory(const trajectory_t *tr, int time, vec_t *result)
{
    if (tr->trType == TR_GRAVITY)
        memcpy(result, evaluatedGravity, sizeof(vec3_t));
    else if (tr->trType == TR_LINEAR_STOP && !tr->trDuration)
        memcpy(result, evaluatedMoving, sizeof(vec3_t));
    else
        memcpy(result, tr->trBase, sizeof(vec3_t));
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
const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out)
{
    memcpy(out, v, sizeof(vec3_t));
    return Vec3Normalize(out);
}

_Static_assert(__builtin_offsetof(scr_const_t, movedone) == 0x28, "movedone");
_Static_assert(__builtin_offsetof(scr_const_t, rotatedone) == 0x40, "rotatedone");
_Static_assert(__builtin_offsetof(scr_const_t, script_brushmodel) == 0x42, "script_brushmodel");
_Static_assert(__builtin_offsetof(scr_const_t, script_model) == 0x44, "script_model");
_Static_assert(__builtin_offsetof(scr_const_t, script_origin) == 0x46, "script_origin");

static scr_entref_t make_mover(int classname)
{
    scr_entref_t entref;
    memset(&entref, 0, sizeof(entref));
    entref.entnum = 1;
    memset(&g_entities[1], 0, sizeof(g_entities[1]));
    g_entities[1].s.number = 1;
    g_entities[1].classname = classname;
    return entref;
}

/* Mac 0x1b908d-0x1b90ab keeps the evaluated position and skips the copy at 0x1b8efa. */
static void gravity_end(void)
{
    make_mover(5);
    gentity_t *ent = &g_entities[1];
    ent->s.pos.trType = TR_GRAVITY;
    ent->mover.pos3[0] = 100;
    ent->mover.pos3[1] = 200;
    ent->mover.pos3[2] = 300;
    level.time = 100;
    notifies = 0;
    Reached_ScriptMover(ent);
    assert(!memcmp(ent->s.pos.trBase, evaluatedGravity, sizeof(vec3_t)));
    assert(!memcmp(ent->r.currentOrigin, evaluatedGravity, sizeof(vec3_t)));
    assert(ent->s.pos.trType == TR_STATIONARY && ent->s.pos.trTime == 100);
    assert(notifies == 1 && lastNotify == scr_const.movedone);
}

/* A move that is not gravity ends on its target. */
static void plain_moves(void)
{
    make_mover(5);
    gentity_t *ent = &g_entities[1];
    ent->s.pos.trType = TR_LINEAR_STOP;
    ent->s.pos.trDuration = 1;
    ent->mover.pos3[0] = 100;
    ent->mover.pos3[1] = 200;
    ent->mover.pos3[2] = 300;
    level.time = 100;
    Reached_ScriptMover(ent);
    assert(ent->s.pos.trBase[0] == 100 && ent->s.pos.trBase[2] == 300);
    assert(ent->s.pos.trType == TR_STATIONARY);

    scr_const.script_brushmodel = 3;
    scr_const.script_model = 4;
    scr_const.script_origin = 5;
    scr_entref_t entref = make_mover(4);
    ScriptEntCmd_Solid(entref);
    assert(g_entities[1].r.contents == 0x2080 && !errors);
    entref = make_mover(3);
    g_entities[1].s.eFlags = 3;
    ScriptEntCmd_Solid(entref);
    assert(g_entities[1].r.contents == 1 && g_entities[1].s.eFlags == 2);
    ScriptEntCmd_NotSolid(entref);
    assert(!g_entities[1].r.contents && g_entities[1].s.eFlags == 3);
}

/* Mac 0x1b96ad stores the peak speed vector directly as trDelta while accelerating. */
static void accelerate_delta(void)
{
    static const float accelTimes[] = {0.1234f, 0.0005f};
    scr_const.script_model = 4;
    for (int i = 0; i < 2; ++i) {
        scr_entref_t entref = make_mover(4);
        gentity_t *ent = &g_entities[1];
        vectorArg[0] = 100;
        vectorArg[1] = vectorArg[2] = 0;
        floats[1] = 1;
        floats[2] = accelTimes[i];
        params = 3;
        errors = 0;
        ScriptEntCmd_MoveTo(entref);
        const float expected = 100.0f * 2.0f / (1.0f * 2.0f - accelTimes[i] - 0.0f);
        assert(!errors && ent->s.pos.trType == TR_ACCELERATE);
        assert(ent->s.pos.trDelta[0] == expected);
        assert(ent->s.pos.trDuration == (int)(accelTimes[i] * 1000.0f));
    }
}

/* Mac 0x1b92c0-0x1b92fe takes the move delta before re-evaluating at 0x1b963b. */
static void moving_delta(void)
{
    scr_const.script_model = 4;
    scr_entref_t entref = make_mover(4);
    gentity_t *ent = &g_entities[1];
    ent->s.pos.trType = TR_LINEAR_STOP;
    vectorArg[0] = 100;
    vectorArg[1] = vectorArg[2] = 0;
    floats[1] = 1;
    params = 2;
    errors = 0;
    ScriptEntCmd_MoveTo(entref);
    assert(!errors);
    assert(ent->s.pos.trBase[0] == evaluatedMoving[0]);
    assert(ent->s.pos.trDelta[0] == 100);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    if (!strcmp(argv[1], "gravity")) gravity_end();
    else if (!strcmp(argv[1], "accel")) accelerate_delta();
    else if (!strcmp(argv[1], "moving")) moving_delta();
    else if (!strcmp(argv[1], "plain")) plain_moves();
    else return 2;
    printf("script mover %s: pass\n", argv[1]);
    return 0;
}
