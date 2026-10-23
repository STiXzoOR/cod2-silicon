#include "cm_test_source.c"
#include "../fidelity/check.h"
clipMap_t cm;
static cmodel_t model;
cmodel_t *CM_ClipHandleToModel(clipHandle_t handle)
{
    return &model;
}
int main(void)
{
    cbrush_t brush = { .mins = { -2,-2,-2 }, .maxs = { 2,2,2 }, .contents = 7 };
    unsigned short index = 0;
    cLeafBrushNode_t nodes[4] = { { .leafBrushCount = 1, .contents = 7, .data.leaf.brushes = &index } };
    cm.brushes = &brush;
    cm.leafbrushNodes = nodes;
    model.leaf = (cLeaf_t){ .mins = { -3,-3,-3 }, .maxs = { 3,3,3 }, .leafBrushNode = 0 };
    const vec3_t origin = { 0,0,0 };
    // Index zero denotes no brush tree, even if bounds happen to contain p.
    CHECK(CM_PointContents(origin, 1) == 0);
    nodes[1] = nodes[0];
    model.leaf.leafBrushNode = 1;
    CHECK(CM_PointContents(origin, 1) == 7);
    const vec3_t boundary = { 2,0,0 }, leafBoundary = { 3,0,0 };
    CHECK(CM_PointContents(boundary, 1) == 7);
    CHECK(CM_PointContents(leafBoundary, 1) == 0);
    // At an equal split distance Mac selects child 1.
    nodes[1] = (cLeafBrushNode_t){ .axis = 0, .data.children = { .dist = 0, .childOffset = { 1,2 } } };
    nodes[2] = nodes[0];
    nodes[3] = (cLeafBrushNode_t){ .leafBrushCount = 1, .data.leaf.brushes = &index };
    CHECK(CM_PointContents(origin, 1) == 7);
    FINISH("collision contents");
}
