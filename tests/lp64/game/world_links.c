/* Synthetic entities exercise the production link/query/unlink chain. */
#include "ws39_cm_world.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

server_t sv;
clipMap_t cm;
void *imp_sv = &sv;
static gentity_t entities[1024];
static cmodel_t fixtureModel;
gentity_t *SV_GEntityForSvEntity(svEntity_t *ent)
{
    ptrdiff_t index = ent - sv.svEntities;
    assert(index >= 0 && index < 1024);
    return &entities[index];
}
cmodel_t *CM_ClipHandleToModel(clipHandle_t handle) { return &fixtureModel; }
int XModelGetContents(const XModel *model) { return 0; }
void Com_DPrintf(const char *format, ...) { abort(); }

int main(void)
{
    vec3_t mins = {-2,-2,-2}, maxs = {2,2,2};
    int ids[] = {0,3,1,1023}, found[1024];
    fixtureModel.leaf.brushContents = -1;
    cm_world.mins[0] = cm_world.mins[1] = -256;
    cm_world.maxs[0] = cm_world.maxs[1] = 256;
    for (int i = 0; i < 4; ++i) {
        int id = ids[i];
        entities[id].r.contents = 0x200004;
        for (int axis = 0; axis < 3; ++axis) {
            entities[id].r.absmin[axis] = mins[axis];
            entities[id].r.absmax[axis] = maxs[axis];
        }
        CM_LinkEntity(&sv.svEntities[id], mins, maxs, 1023);
        assert(CM_WorldEntityIndex(&sv.svEntities[id]) == id + 1);
        assert(CM_WorldEntityForIndex(id + 1) == &sv.svEntities[id]);
    }
    assert(cm_world.sectors[1].contents.entities == 1);
    assert(CM_AreaEntities(mins, maxs, found, 1024, 0x200000) == 4);
    assert(found[0] == 0 && found[1] == 1 && found[2] == 3 && found[3] == 1023);
    CM_UnlinkEntity(&sv.svEntities[1]); /* middle */
    CM_UnlinkEntity(&sv.svEntities[0]); /* head */
    CM_UnlinkEntity(&sv.svEntities[1023]); /* tail */
    assert(CM_AreaEntities(mins, maxs, found, 1024, 0x200000) == 1 && found[0] == 3);
    CM_UnlinkEntity(&sv.svEntities[3]);
    assert(!cm_world.sectors[1].contents.entities);
    assert(!cm_world.sectors[1].contents.contentsEntities);
    puts("production broadphase: zero/max IDs, sorted query, head/middle/tail unlink passed");
}
