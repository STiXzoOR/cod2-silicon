/* Locational point and sight traces through production sv_world_mp.c. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void SV_PointTraceToEntity(const pointtrace_t *clip, svEntity_t *check, trace_t *trace);
extern int SV_PointSightTraceToEntity(const sightpointtrace_t *clip, svEntity_t *check);
server_t sv;
static gentity_t entity;
static vec3_t zero;
void *imp_vec3_origin = zero;
static int geomCalls, failures;
static vec3_t seenStart, seenEnd;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

vec3_t actorLocationalMins = { -64.0f, -64.0f, -32.0f };
void Com_Error(int code, const char *fmt, ...) { abort(); }
/* Only the locational model path is exercised; the brush fallbacks must not run. */
clipHandle_t CM_TempBoxModel(const vec_t *mins, const vec_t *maxs, int contents) { abort(); }
int CM_TransformedBoxTrace(trace_t *results, const vec_t *start, const vec_t *end, const vec_t *mins,
    const vec_t *maxs, int clipHandle, int contentmask, const vec_t *origin, const vec_t *angles) { abort(); }
int CM_TransformedBoxSightTrace(int oldHitNum, const vec_t *start, const vec_t *end, const vec_t *mins,
    const vec_t *maxs, int clipHandle, int contentmask, const vec_t *origin, const vec_t *angles) { abort(); }
void DObjTraceline(DObj *obj, vec_t *start, vec_t *end, unsigned char *priorityMap, DObjTrace_s *objTrace) { abort(); }
gentity_t *SV_GentityNum(int num) { assert(num == 5); return &entity; }
DObj *Com_GetServerDObj(int handle) { assert(handle == 5); return (DObj *)&entity; }
int DObjHasContents(DObj *obj, int contentmask) { return 1; }
void DObjGetBounds(const DObj *obj, vec_t *mins, vec_t *maxs)
{
    for (int i = 0; i < 3; i++) { mins[i] = -16; maxs[i] = 16; }
}
int CM_TraceBox(const TraceExtents *extents, const vec_t *mins, const vec_t *maxs, float fraction) { return 0; }
void CM_CalcTraceEntents(TraceExtents *extents) {}
void G_DObjCalcPose(gentity_t *ent) { assert(ent == &entity); }
void DObjGeomTraceline(DObj *obj, vec_t *start, vec_t *end, int contentmask, DObjTrace *trace)
{
    memcpy(seenStart, start, sizeof(seenStart));
    memcpy(seenEnd, end, sizeof(seenEnd));
    ++geomCalls;
}

int main(void)
{
    entity.s.number = 5;
    entity.r.contents = 1;
    entity.r.svFlags = 4;
    entity.r.currentOrigin[0] = 100; entity.r.currentOrigin[1] = 200; entity.r.currentOrigin[2] = 300;

    /* The model-space ray subtracts the entity origin: Linux 1.3
       sv_world_mp.cpp stores currentOrigin in entAxis[3] for the 4x3 transform. */
    pointtrace_t clip;
    memset(&clip, 0, sizeof(clip));
    clip.extents.start[0] = 110; clip.extents.start[1] = 200; clip.extents.start[2] = 300;
    clip.extents.end[0] = 90; clip.extents.end[1] = 200; clip.extents.end[2] = 300;
    clip.passEntityNum = 1023;
    clip.contentmask = 1;
    clip.bLocational = 1;
    trace_t trace;
    memset(&trace, 0, sizeof(trace));
    trace.fraction = 1.0f;
    SV_PointTraceToEntity(&clip, &sv.svEntities[5], &trace);
    CHECK(geomCalls == 1);
    CHECK(seenStart[0] == 10 && seenStart[1] == 0 && seenStart[2] == 0);
    CHECK(seenEnd[0] == -10 && seenEnd[1] == 0 && seenEnd[2] == 0);

    sightpointtrace_t sight;
    memset(&sight, 0, sizeof(sight));
    memcpy(sight.start, clip.extents.start, sizeof(sight.start));
    memcpy(sight.end, clip.extents.end, sizeof(sight.end));
    sight.passEntityNum[0] = sight.passEntityNum[1] = 1023;
    sight.contentmask = 1;
    sight.locational = 1;
    memset(seenStart, 0x7f, sizeof(seenStart));
    SV_PointSightTraceToEntity(&sight, &sv.svEntities[5]);
    CHECK(geomCalls == 2);
    CHECK(seenStart[0] == 10 && seenStart[1] == 0 && seenStart[2] == 0);
    CHECK(seenEnd[0] == -10 && seenEnd[1] == 0 && seenEnd[2] == 0);

    if (failures) return 1;
    puts("point traces: locational model rays are relative to the entity origin");
    return 0;
}
