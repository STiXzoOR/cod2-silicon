#include "cm_trace_source.c"
#include "../fidelity/check.h"

clipMap_t cm;
void *imp_cm = &cm;
static TraceThreadInfo info;
static cbrush_t tempBrush;
static cmodel_t tempModel;
static unsigned short partitionChecks[4];
static int leafCalls;
static vec3_t leafBounds[2];

void *Sys_GetValue(int key)
{
    CHECK(key == 3);
    return &info;
}

const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    float inverse = length ? 1.0f / length : 0.0f;
    for (int i = 0; i < 3; ++i)
        out[i] = v[i] * inverse;
    return length;
}

const vec_t Vec3Normalize(vec_t *v)
{
    return Vec3NormalizeTo(v, v);
}

int CM_BoxLeafnums(const vec_t *mins, const vec_t *maxs, int *list, int listsize, int *lastLeaf)
{
    ++leafCalls;
    memcpy(leafBounds[0], mins, sizeof(vec3_t));
    memcpy(leafBounds[1], maxs, sizeof(vec3_t));
    CHECK(listsize == 1024);
    *lastLeaf = 0;
    for (int i = 0; i < cm.numLeafs; ++i)
        list[i] = i;
    return cm.numLeafs;
}

static void Reset(void)
{
    memset(&cm, 0, sizeof(cm));
    memset(&info, 0, sizeof(info));
    memset(&tempBrush, 0, sizeof(tempBrush));
    memset(&tempModel, 0, sizeof(tempModel));
    memset(partitionChecks, 0, sizeof(partitionChecks));
    info.box_brush = &tempBrush;
    info.box_model = &tempModel;
    info.checkcount.partitions = partitionChecks;
    leafCalls = 0;
}

static void Near(float actual, float expected)
{
    CHECK(fabsf(actual - expected) < 0.00001f);
}

static void Clear(const trace_t *trace)
{
    CHECK(trace->fraction == 1.0f);
    CHECK(!trace->startsolid && !trace->allsolid);
    CHECK(trace->contents == 0);
}

static void TemporaryCapsule(void)
{
    const vec3_t boundsMin = { -10, -10, -20 }, boundsMax = { 10, 10, 20 };
    const vec3_t zero = { 0, 0, 0 };
    const float corners[][2] = { { 9, 9 }, { -9, 9 }, { 0, 0 }, { 9, 0 }, { 16, 0 } };
    trace_t trace;
    Reset();
    clipHandle_t model = CM_TempBoxModel(boundsMin, boundsMax, 1);
    CHECK(model == 0x3ff);
    for (int i = 0; i < 5; ++i) {
        vec3_t start = { corners[i][0], corners[i][1], 0 };
        vec3_t end = { corners[i][0], corners[i][1], 1 };
        CM_BoxTrace(&trace, start, end, zero, zero, model, 1);
        int inside = corners[i][0] * corners[i][0] + corners[i][1] * corners[i][1] < 100;
        CHECK(!!trace.startsolid == inside);
        CHECK(!!trace.allsolid == inside);
        Near(trace.fraction, inside ? 0 : 1);
        CM_BoxTrace(&trace, start, start, zero, zero, model, 1);
        CHECK(!!trace.allsolid == inside);
        Near(trace.fraction, inside ? 0 : 1);
    }

    // Rounded endcap: (8, 0, 19) is outside the sphere centered at z=10.
    const vec3_t capCorner = { 8, 0, 19 }, capEnd = { 8, 0, 18 };
    CM_BoxTrace(&trace, capCorner, capEnd, zero, zero, model, 1);
    Clear(&trace);
    CM_BoxTrace(&trace, capCorner, capCorner, zero, zero, model, 1);
    Clear(&trace);

    // Mac's sweep subtracts 0.125 / travel for this axial cylinder/sphere ray.
    const vec3_t horizontal = { 20, 0, 0 }, center = { 0, 0, 0 };
    CM_BoxTrace(&trace, horizontal, center, zero, zero, model, 1);
    Near(trace.fraction, (10.0f - 0.125f) / 20.0f);
    Near(trace.normal[0], 1);
    Near(trace.normal[1], 0);
    Near(trace.normal[2], 0);
    CHECK(trace.contents == 1 && !trace.startsolid && !trace.allsolid);
    const vec3_t above = { 0, 0, 30 }, top = { 0, 0, 10 };
    CM_BoxTrace(&trace, above, top, zero, zero, model, 1);
    Near(trace.fraction, (10.0f - 0.125f) / 20.0f);
    Near(trace.normal[2], 1);
    const vec3_t below = { 0, 0, -30 }, bottom = { 0, 0, -10 };
    CM_BoxTrace(&trace, below, bottom, zero, zero, model, 1);
    Near(trace.fraction, (10.0f - 0.125f) / 20.0f);
    Near(trace.normal[2], -1);
    const vec3_t diagonal = { 20, 10, 0 };
    float separation = sqrtf(500.0f);
    CM_BoxTrace(&trace, diagonal, center, zero, zero, model, 1);
    Near(trace.fraction, (separation - 10.0f - 0.125f) / separation);
    Near(trace.normal[0], 20.0f / separation);
    Near(trace.normal[1], 10.0f / separation);
    CM_BoxTrace(&trace, horizontal, center, zero, zero, model, 2);
    Clear(&trace);

    // A moving capsule contributes radius 2 and axial segment half-length 3.
    const vec3_t bodyMin = { -2, -2, -5 }, bodyMax = { 2, 2, 5 };
    const vec3_t bodyCorner = { 9, 9, 0 }, bodyEnd = { 9, 9, 1 };
    CM_BoxTrace(&trace, bodyCorner, bodyEnd, bodyMin, bodyMax, model, 1);
    Clear(&trace); // sqrt(162) > 10+2; a box expansion incorrectly blocks it.
    const vec3_t bodyAbove = { 0, 0, 30 }, bodyDown = { 0, 0, 20 };
    CM_BoxTrace(&trace, bodyAbove, bodyDown, bodyMin, bodyMax, model, 1);
    Near(trace.fraction, (5.0f - 0.125f) / 10.0f);
    Near(trace.normal[2], 1);

    // Position tangency is strict in the reference's rounded model branch.
    const vec3_t tangent = { 10, 0, 0 };
    CM_BoxTrace(&trace, tangent, tangent, zero, zero, model, 1);
    Clear(&trace);
    const vec3_t capTangent = { 0, 0, 20 };
    CM_BoxTrace(&trace, capTangent, capTangent, zero, zero, model, 1);
    Clear(&trace);
    const vec3_t capInside = { 0, 0, 19 }, capExit = { 0, 0, 30 };
    CM_BoxTrace(&trace, capInside, capExit, zero, zero, model, 1);
    CHECK(trace.startsolid && !trace.allsolid && trace.fraction == 0 && trace.contents == 1);
    Near(trace.normal[2], 1);
    // Mac's middle-cylinder overlap sets allsolid from axial containment alone.
    const vec3_t cylinderInside = { 9, 0, 0 }, cylinderExit = { 20, 0, 0 };
    CM_BoxTrace(&trace, cylinderInside, cylinderExit, zero, zero, model, 1);
    CHECK(trace.startsolid && trace.allsolid && trace.fraction == 0 && trace.contents == 1);
    const vec3_t sphereMin = { -10, -10, -10 }, sphereMax = { 10, 10, 10 };
    CM_TempBoxModel(sphereMin, sphereMax, 1);
    const vec3_t sphereCorner = { 8, 0, 8 }, sphereEnd = { 8, 0, 9 };
    CM_BoxTrace(&trace, sphereCorner, sphereEnd, zero, zero, model, 1);
    Clear(&trace);
    const vec3_t sphereInside = { 6, 0, 6 };
    CM_BoxTrace(&trace, sphereInside, sphereInside, zero, zero, model, 1);
    CHECK(trace.startsolid && trace.allsolid && trace.fraction == 0 && trace.contents == 0);

    const vec3_t shiftedMin = { 90, 190, 280 }, shiftedMax = { 110, 210, 320 };
    const vec3_t asymmetricMin = { 0, 0, 0 }, asymmetricMax = { 4, 4, 10 };
    CM_TempBoxModel(shiftedMin, shiftedMax, 1);
    const vec3_t shiftedCorner = { 107, 207, 295 }, shiftedCenter = { 98, 198, 295 };
    CM_BoxTrace(&trace, shiftedCorner, shiftedCorner, asymmetricMin, asymmetricMax, model, 1);
    Clear(&trace);
    CM_BoxTrace(&trace, shiftedCenter, shiftedCenter, asymmetricMin, asymmetricMax, model, 1);
    CHECK(trace.startsolid && trace.allsolid && trace.fraction == 0);
}

static void StationaryBrush(void)
{
    Reset();
    cplane_t sidePlane = { .normal = { 0.6f, 0.8f, 0 }, .dist = 0 };
    cbrushside_t side = { .plane = &sidePlane };
    cbrush_t brush = { .mins = { -20, -20, -20 }, .maxs = { 20, 20, 20 },
        .contents = 1, .numsides = 1, .sides = &side };
    unsigned short brushIndex = 0;
    cLeafBrushNode_t node = { .leafBrushCount = 1, .contents = 1, .data.leaf.brushes = &brushIndex };
    cmodel_t models[2] = { 0 };
    models[1].leaf = (cLeaf_t){ .brushContents = 1, .mins = { -20, -20, -20 },
        .maxs = { 20, 20, 20 }, .leafBrushNode = 0 };
    cm.numSubModels = 2;
    cm.cmodels = models;
    cm.brushes = &brush;
    cm.numBrushes = 1;
    cm.leafbrushNodes = &node;
    cm.leafbrushNodesCount = 1;
    trace_t trace;
    const vec3_t bodyMin = { -1, -2, -3 }, bodyMax = { 1, 2, 3 };
    const vec3_t outside = { 2, 0, 0 }, inside = { 1, 0, 0 };
    CM_BoxTrace(&trace, outside, outside, bodyMin, bodyMax, 1, 1);
    Clear(&trace); // n dot center = 1.2; capsule support = 1, box support = 2.2.
    CM_BoxTrace(&trace, inside, inside, bodyMin, bodyMax, 1, 1);
    CHECK(trace.allsolid && trace.startsolid && trace.fraction == 0 && trace.contents == 1);
    CHECK(!trace.material && trace.surfaceFlags == 0);
    CM_BoxTrace(&trace, inside, inside, bodyMin, bodyMax, 1, 2);
    Clear(&trace);

    // World position tests enumerate leaves with full box extents plus one unit.
    cm.leafs = &models[1].leaf;
    cm.numLeafs = 1;
    cNode_t root = { .children = { -1, -1 } };
    cplane_t rootPlane = { .normal = { 1, 0, 0 }, .dist = 100, .type = 0 };
    root.plane = &rootPlane;
    cm.nodes = &root;
    CM_BoxTrace(&trace, outside, outside, bodyMin, bodyMax, 0, 1);
    Clear(&trace);
    CHECK(leafCalls == 1);
    Near(leafBounds[0][0], 0);
    Near(leafBounds[1][0], 4);
    Near(leafBounds[0][1], -3);
    Near(leafBounds[1][1], 3);
    Near(leafBounds[0][2], -4);
    Near(leafBounds[1][2], 4);
    sidePlane.normal[1] = 0;
    sidePlane.normal[2] = 0.8f;
    const vec3_t slopeOutside = { 5, 0, 0 };
    CM_BoxTrace(&trace, slopeOutside, slopeOutside, bodyMin, bodyMax, 1, 1);
    Clear(&trace); // n dot center = 3; radius + |offsetZ*nz| = 2.6.
    sidePlane.normal[1] = 0.8f;
    sidePlane.normal[2] = 0;
    sidePlane.dist = 5;

    // A split selects only the matching position branch; the negative-count
    // node also owns an immediate brush child before its spatial children.
    cLeafBrushNode_t branches[4] = { 0 };
    branches[0] = (cLeafBrushNode_t){ .axis = 0, .contents = 1,
        .data.children = { .dist = 0, .childOffset = { 2, 3 } } };
    branches[1] = node;
    branches[2] = (cLeafBrushNode_t){ .leafBrushCount = 1, .contents = 2, .data.leaf.brushes = &brushIndex };
    branches[3] = node;
    cm.leafbrushNodes = branches;
    cm.leafbrushNodesCount = 4;
    const vec3_t positive = { 1, 0, 0 }, negative = { -1, 0, 0 };
    const vec3_t zero = { 0, 0, 0 };
    CM_BoxTrace(&trace, positive, positive, zero, zero, 1, 1);
    Clear(&trace);
    CM_BoxTrace(&trace, negative, negative, zero, zero, 1, 1);
    CHECK(trace.startsolid && trace.allsolid && trace.contents == 1);
    branches[0].leafBrushCount = -1;
    CM_BoxTrace(&trace, positive, positive, zero, zero, 1, 1);
    CHECK(trace.startsolid && trace.allsolid && trace.contents == 1);
    // Strict leaf cull excludes contact exactly on the leaf's maximum plane.
    brush.numsides = 0;
    const vec3_t boundary = { 20, 0, 0 };
    CM_BoxTrace(&trace, boundary, boundary, zero, zero, 1, 1);
    Clear(&trace);
}

static void StationaryMesh(void)
{
    Reset();
    CollisionTriangle triangle = { .plane = { 0, 0, 1, 0 },
        .svec = { 0.01f, 0, 0, 0 }, .tvec = { 0, 0.01f, 0, 0 } };
    CollisionPartition partition = { .triCount = 1, .tris = &triangle };
    CollisionAabbTree tree = { .origin = { 50, 50, 0 }, .halfSize = { 50, 50, 1 }, .u.partitionIndex = 0 };
    dmaterial_t material = { .contentFlags = 4, .surfaceFlags = 123 };
    cmodel_t models[2] = { 0 };
    models[1].leaf = (cLeaf_t){ .terrainContents = 4, .collAabbCount = 1 };
    cm.numSubModels = 2;
    cm.cmodels = models;
    cm.aabbTrees = &tree;
    cm.aabbTreeCount = 1;
    cm.partitions = &partition;
    cm.materials = &material;
    cm.numMaterials = 1;
    const vec3_t point = { 10, 10, 0.5f }, clear = { 10, 10, 4 };
    const vec3_t bodyMin = { -1, -1, -2 }, bodyMax = { 1, 1, 2 };
    trace_t trace;
    CM_BoxTrace(&trace, point, point, bodyMin, bodyMax, 1, 4);
    CHECK(trace.startsolid && trace.allsolid && trace.fraction == 0);
    CHECK(trace.contents == 4 && trace.surfaceFlags == 123 && trace.material == (const char *)&material);
    CM_BoxTrace(&trace, clear, clear, bodyMin, bodyMax, 1, 4);
    Clear(&trace);
    CM_BoxTrace(&trace, point, point, bodyMin, bodyMax, 1, 1);
    Clear(&trace);
    cm.leafs = &models[1].leaf;
    cm.numLeafs = 1;
    cplane_t plane = { .normal = { 1, 0, 0 }, .type = 0 };
    cNode_t node = { .plane = &plane, .children = { -1, -1 } };
    cm.nodes = &node;
    CM_BoxTrace(&trace, point, point, bodyMin, bodyMax, 0, 4);
    CHECK(trace.startsolid && trace.allsolid && trace.fraction == 0 && trace.contents == 4);
    CHECK(leafCalls == 1);
}

int main(void)
{
    TemporaryCapsule();
    StationaryBrush();
    StationaryMesh();
    FINISH("collision");
}
