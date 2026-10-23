#include "cm_world_source.c"
#include "../fidelity/check.h"
clipMap_t cm;
server_t sv;
void *imp_sv = &sv;
int XModelGetContents(const XModel *model)
{
    return 1;
}
gentity_t *SV_GEntityForSvEntity(svEntity_t *ent)
{
    static gentity_t entity;
    return &entity;
}
static int sightHit;
int SV_PointSightTraceToEntity(const sightpointtrace_t *clip, svEntity_t *ent)
{
    return sightHit;
}
int SV_ClipSightToEntity(const sightclip_t *clip, svEntity_t *ent)
{
    return sightHit;
}
int main(void)
{
    cStaticModel_t model = { .absmin = { 100,100,0 }, .absmax = { 110,110,10 } };
    cm.staticModelList = &model;
    memset(&cm_world, 0, sizeof(cm_world));
    cm_world.freeHead = 2;
    cm_world.sectors[1].contents.staticModels = 1;
    cm_world.sectors[1].tree.axis = 0;
    cm_world.sectors[1].tree.dist = 0;
    const vec2_t mins = { -1500,-1000 }, maxs = { 1500,1000 };
    CM_SortNode(1, (vec_t *)mins, (vec_t *)maxs);
    CHECK(cm_world.sectors[1].tree.child[0] == 2);
    // Mac allocates the new sector using the caller's complete bounds.
    // Clipping them before allocation changes the split from X to Y.
    CHECK(cm_world.sectors[2].tree.axis == 0);
    CHECK(cm_world.sectors[2].tree.dist == 0);
    CHECK(cm_world.sectors[2].contents.staticModels == 1);
    memset(&cm_world, 0, sizeof(cm_world));
    cm_world.sectors[1].contents.contentsEntities = 1;
    cm_world.sectors[1].contents.entities = 1;
    const vec3_t start = { 1,0,0 }, end = { 2,0,0 };
    sightpointtrace_t pointClip = { .contentmask = 1 };
    sightclip_t boxClip = { .contentmask = 1 };
    sightHit = 137;
    CHECK(CM_PointSightTraceToEntities_r(&pointClip, 1, start, end) == 137);
    CHECK(CM_ClipSightTraceToEntities_r(&boxClip, 1, start, end) == 137);
    // Preserve negative temporary-model hints and propagate child results.
    sightHit = -1;
    cm_world.sectors[1].contents.entities = 0;
    cm_world.sectors[1].tree.child[0] = 2;
    cm_world.sectors[2].contents.contentsEntities = 1;
    cm_world.sectors[2].contents.entities = 1;
    CHECK(CM_PointSightTraceToEntities_r(&pointClip, 1, start, end) == -1);
    CHECK(CM_ClipSightTraceToEntities_r(&boxClip, 1, start, end) == -1);
    FINISH("collision world");
}
