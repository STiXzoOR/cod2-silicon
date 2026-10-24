/* Production contact and friendly-visibility queries with synthetic geometry. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern qboolean SV_EntityContact(const vec_t *, const vec_t *, const gentity_t *);
extern qboolean SV_inSnapshot(const vec_t *, int);
server_t sv;
static gentity_t entity;
static vec3_t zero;
void *imp_vec3_origin = zero;
static int brushCalls, brushSolid, failures, fogCalls;
static byte pvs[8];
static float fogDistance = FLT_MAX;
static int beyondFog;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
void Com_Error(int code, const char *fmt, ...) { abort(); }
int SV_ClipHandleForEntity(const gentity_t *ent) { assert(ent == &entity); return 17; }
int CM_TransformedBoxTraceExternal(trace_t *trace, const vec_t *start, const vec_t *end,
    const vec_t *mins, const vec_t *maxs, clipHandle_t handle, int mask,
    const vec_t *origin, const vec_t *angles)
{
    assert(start == zero && end == zero && handle == 17 && mask == -1);
    assert(origin == entity.r.currentOrigin && angles == entity.r.currentAngles);
    assert(mins[0] == -2 && maxs[0] == 2);
    ++brushCalls;
    memset(trace, 0, sizeof(*trace));
    trace->startsolid = brushSolid;
    return 0;
}
int CM_PointLeafnum(const vec_t *origin) { return 3; }
int CM_LeafCluster(int leaf) { assert(leaf == 3); return 2; }
byte *CM_ClusterPVS(int cluster) { assert(cluster == 2); return pvs; }
float G_GetFogOpaqueDistSqrd(void) { ++fogCalls; return fogDistance; }
static void contact(void)
{
    vec3_t mins = {-2, -2, 0}, maxs = {2, 2, 4};
    memset(&entity, 0, sizeof(entity));
    entity.r.maxs[2] = 5;
    /* Cylinder radius 8 + query half-width 2; disk radius 72 + 2 - 64. */
    for (int disk = 0; disk < 2; ++disk) {
        entity.r.svFlags = disk ? 0x40 : 0x20;
        entity.r.maxs[0] = disk ? 72 : 8;
        for (int side = -1; side <= 1; ++side) {
            entity.r.currentOrigin[0] = 10 + side;
            CHECK(SV_EntityContact(mins, maxs, &entity) == (disk ? side >= 0 : side < 0));
        }
    }
    entity.r.svFlags = 0x20;
    entity.r.currentOrigin[0] = 0;
    entity.r.currentOrigin[2] = 4;
    CHECK(!SV_EntityContact(mins, maxs, &entity));
    entity.r.currentOrigin[2] = -5;
    CHECK(!SV_EntityContact(mins, maxs, &entity));
    entity.r.currentOrigin[2] = -4;
    CHECK(SV_EntityContact(mins, maxs, &entity));
    entity.r.svFlags = 0;
    for (brushSolid = 0; brushSolid < 2; ++brushSolid)
        CHECK(SV_EntityContact(mins, maxs, &entity) == brushSolid);
    CHECK(brushCalls == 2);
}
static void visibility(void)
{
    memset(&entity, 0, sizeof(entity));
    sv.gentities = &entity; sv.gentitySize = sizeof(entity);
    entity.r.linked = 1;
    svEntity_t *serverEntity = &sv.svEntities[0];
    serverEntity->numClusters = 16;
    for (int i = 0; i < 16; ++i) serverEntity->clusternums[i] = 16 + i;
    serverEntity->lastCluster = 35;
    /* Preserve Mac's original inclusive scan and its endpoint rejection. */
    int visible[][2] = {{-1,-1}, {2,-1}, {20,-1}, {33,-1}, {35,-1}, {2,35}};
    int expected[] = {1,1,1,1,0,0};
    for (int test = 0; test < 6; ++test) {
        memset(pvs, 0, sizeof(pvs));
        for (int i = 0; i < 2; ++i) {
            int cluster = visible[test][i];
            if (cluster >= 0) pvs[cluster >> 3] |= 1u << (cluster & 7);
        }
        CHECK(SV_inSnapshot(zero, 0) == expected[test]);
    }
    memset(pvs, 0, sizeof(pvs));
    serverEntity->lastCluster = 0;
    CHECK(!SV_inSnapshot(zero, 0));
    pvs[20 >> 3] = 1u << (20 & 7);
    CHECK(SV_inSnapshot(zero, 0));
    fogDistance = 100;
    for (beyondFog = 0; beyondFog < 2; ++beyondFog) {
        entity.r.absmin[0] = entity.r.absmax[0] = beyondFog ? 20 : 0;
        CHECK(SV_inSnapshot(zero, 0) == !beyondFog);
    }
    entity.r.linked = 0;
    CHECK(!SV_inSnapshot(zero, 0));
    entity.r.linked = 1; entity.r.svFlags = 1;
    CHECK(!SV_inSnapshot(zero, 0));
    entity.r.svFlags = 8;
    CHECK(SV_inSnapshot(zero, 0));
}
int main(void)
{
    contact(); visibility();
    if (failures) return 1;
    puts("server contact: shape boundaries, brush delegation and overflow visibility pass");
}
