/* Exercise production corpse landing orientation and movement controls. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void G_RunCorpseMove(gentity_t *ent);
scr_data_t g_scr_data;
level_locals_t level;
static gentity_t fixtureCorpse;
static vec3_t rootDelta;
static vec3_t tracedEnd;
static int traces, links, thinks, anglesSet;
static float traceFraction = 1;
static vec3_t traceNormal, landingAngles;
static int failures;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

int G_GetPlayerCorpseIndex(gentity_t *ent) { assert(ent == &fixtureCorpse); return 0; }
void XAnimCalcDelta(XAnimTree *tree, unsigned int index, vec_t *rotation,
                    vec_t *translation, int useGoal)
{
    assert(index == 0 && useGoal == 1);
    rotation[0] = rotation[1] = 0;
    memcpy(translation, rootDelta, sizeof(vec3_t));
}
void BG_EvaluateTrajectory(const trajectory_t *tr, int time, vec_t *out)
{
    assert(tr == &fixtureCorpse.s.pos && time == level.time);
    memcpy(out, tr->trBase, sizeof(vec3_t));
}
void G_TraceCapsule(trace_t *trace, const vec_t *start, const vec_t *mins,
                    const vec_t *maxs, const vec_t *end, int owner, int mask)
{
    assert(owner == 23 && mask == 0x491);
    memcpy(tracedEnd, end, sizeof(tracedEnd));
    memset(trace, 0, sizeof(*trace));
    trace->fraction = traceFraction;
    memcpy(trace->normal, traceNormal, sizeof(vec3_t));
    trace->entityNum = 77;
    ++traces;
}
void SV_LinkEntity(gentity_t *ent) { assert(ent == &fixtureCorpse); ++links; }
int G_RunThink(gentity_t *ent) { assert(ent == &fixtureCorpse); ++thinks; return 1; }
qboolean G_TraceCapsuleComplete(const vec_t *start, const vec_t *mins,
                               const vec_t *maxs, const vec_t *end, int owner, int mask)
{ assert(owner == 23 && mask == 0x491); return 0; }
int SV_PointContents(const vec_t *point, int pass, int mask) { return 0; }
void G_FreeEntity(gentity_t *ent) { abort(); }
void G_SetAngle(gentity_t *ent, const vec_t *angle)
{ assert(ent == &fixtureCorpse); memcpy(landingAngles, angle, sizeof(vec3_t)); ++anglesSet; }

static void Reset(void)
{
    memset(&fixtureCorpse, 0, sizeof(fixtureCorpse));
    memset(&g_scr_data, 0, sizeof(g_scr_data));
    memcpy(fixtureCorpse.s.pos.trBase, (vec3_t){10, 20, 30}, sizeof(vec3_t));
    memcpy(fixtureCorpse.r.currentOrigin, fixtureCorpse.s.pos.trBase, sizeof(vec3_t));
    fixtureCorpse.r.ownerNum = 23;
    fixtureCorpse.clipmask = 0x491;
    fixtureCorpse.active = 1;
    traces = links = thinks = anglesSet = 0;
    traceFraction = 1;
    memset(traceNormal, 0, sizeof(traceNormal));
    level.time = 1000;
}
static void CheckVector(const vec_t *actual, const vec_t *expected)
{
    for (int i = 0; i < 3; ++i)
        CHECK(fabsf(actual[i] - expected[i]) < 0.00001f);
}
int main(void)
{
    const vec3_t angles[] = {{0,0,0}, {0,90,0}, {30,0,0}};
    const vec3_t expected[] = {{14,23,30}, {7,24,30}, {13.4641016f,23,28}};
    for (int i = 0; i < 3; ++i) {
        Reset();
        memcpy(rootDelta, (vec3_t){4,3,0}, sizeof(rootDelta));
        memcpy(fixtureCorpse.r.currentAngles, angles[i], sizeof(vec3_t));
        G_RunCorpseMove(&fixtureCorpse);
        CHECK(traces == 1 && links == 1 && thinks == 1);
        CheckVector(tracedEnd, expected[i]);
        CheckVector(fixtureCorpse.r.currentOrigin, expected[i]);
        CheckVector(fixtureCorpse.s.pos.trBase, expected[i]);
        CHECK(!g_scr_data.playerCorpseInfo[0].falling);
    }
    Reset();
    memcpy(rootDelta, (vec3_t){1,0,0}, sizeof(rootDelta));
    G_RunCorpseMove(&fixtureCorpse);
    CHECK(traces == 0 && links == 0 && thinks == 0);
    CheckVector(fixtureCorpse.r.currentOrigin, (vec3_t){10,20,30});
    Reset();
    g_scr_data.playerCorpseInfo[0].falling = 1;
    memcpy(rootDelta, (vec3_t){4,3,0}, sizeof(rootDelta));
    G_RunCorpseMove(&fixtureCorpse);
    CHECK(traces == 1 && links == 1 && thinks == 1);
    CheckVector(tracedEnd, (vec3_t){10,20,30});
    CheckVector(fixtureCorpse.r.currentOrigin, (vec3_t){10,20,30});
    CHECK(g_scr_data.playerCorpseInfo[0].falling);
    for (int slope = 0; slope < 2; ++slope) {
        Reset();
        g_scr_data.playerCorpseInfo[0].falling = 1;
        memcpy(fixtureCorpse.s.pos.trBase, (vec3_t){14,24,26}, sizeof(vec3_t));
        traceFraction = 0.5f;
        traceNormal[1] = slope ? 0.707106781f : 0;
        traceNormal[2] = slope ? 0.707106781f : 1;
        G_RunCorpseMove(&fixtureCorpse);
        CHECK(traces == 1 && links == 2 && thinks == 1 && anglesSet == 1);
        CheckVector(fixtureCorpse.r.currentOrigin, (vec3_t){12,22,28});
        CheckVector(fixtureCorpse.s.pos.trBase, (vec3_t){12,22,28});
        CheckVector(landingAngles, (vec3_t){0,0,slope ? -45 : 0});
        CHECK(!g_scr_data.playerCorpseInfo[0].falling);
        CHECK(fixtureCorpse.s.groundEntityNum == 77);
    }
    if (!failures)
        puts("F1 items: corpse landing angles and movement controls pass");
    return failures != 0;
}
