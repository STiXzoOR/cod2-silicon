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

#if defined(COD2_X64)
static void SightTemporaryCapsule(void)
{
    const vec3_t zero = { 0, 0, 0 };
    const vec3_t boundsMin = { -10, -10, -20 }, boundsMax = { 10, 10, 20 };
    const struct { vec3_t start, end; int hit; } rays[] = {
        { { 9, 9, 0 }, { 9, 9, 1 }, 0 },
        { { -9, 9, 0 }, { -9, 9, 1 }, 0 },
        { { 0, 0, 0 }, { 0, 0, 1 }, -1 },
        { { 9, 0, 0 }, { 9, 0, 1 }, -1 },
        { { 16, 0, 0 }, { 16, 0, 1 }, 0 },
        { { 8, 0, 19 }, { 8, 0, 18 }, 0 },
        { { 8, 0, -19 }, { 8, 0, -18 }, 0 },
        { { 20, 0, 0 }, { 0, 0, 0 }, -1 },
        { { 0, 0, 30 }, { 0, 0, 10 }, -1 },
        { { 0, 0, -30 }, { 0, 0, -10 }, -1 },
        { { 0, 0, 30 }, { 0, 0, 31 }, 0 },
        { { 0, 0, -30 }, { 0, 0, -31 }, 0 },
        { { 20, 10, 0 }, { 0, 0, 0 }, -1 },
        { { 10, 0, 0 }, { 10, 0, 0 }, -1 },
        { { 0, 0, 20 }, { 0, 0, 20 }, -1 },
        { { 0, 0, 20.01f }, { 0, 0, 20.01f }, 0 },
        { { 0, 0, -20 }, { 0, 0, -20 }, -1 },
        { { 30, 30, 30 }, { 31, 31, 31 }, 0 },
        // Mac sight bias is projection * 0.125 / initial separation length.
        // These rays stop one unit short, yet that bias makes sight block.
        { { 20, 0, 0 }, { 11, 0, 0 }, -1 },
        { { 0, 0, 30 }, { 0, 0, 21 }, -1 },
        { { 0, 0, -30 }, { 0, 0, -21 }, -1 },
        // The cylinder clears with full delta length; the upper cap blocks.
        { { 20, 0, 0 }, { 0, 0, 20 }, -1 },
        { { 20, 0, 0 }, { 0, 0, -20 }, -1 },
        // Full delta length can clear even though a horizontal sweep hits.
        { { 20, 0, -9 }, { 0, 0, 9 }, 0 },
        // After the cylinder clears, test the end entering the upper cap.
        { { 0, 0, 0 }, { 0, 0, 30 }, -1 },
    };
    Reset();
    clipHandle_t model = CM_TempBoxModel(boundsMin, boundsMax, 1);
    for (int i = 0; i < (int)(sizeof(rays) / sizeof(rays[0])); ++i) {
        CHECK(CM_BoxSightTrace(0, rays[i].start, rays[i].end, zero, zero, model, 1) == rays[i].hit);
        CHECK(CM_BoxSightTrace(87, rays[i].start, rays[i].end, zero, zero, model, 1) == rays[i].hit);
        CHECK(CM_BoxSightTrace(0, rays[i].start, rays[i].end, zero, zero, model, 2) == 0);
    }
    CHECK(leafCalls == 0);
    trace_t trace;
    CM_BoxTrace(&trace, rays[18].start, rays[18].end, zero, zero, model, 1);
    Clear(&trace); // Sight's bias is a separate contract from swept fractions.
    const vec3_t bodyMin = { -2, -2, -5 }, bodyMax = { 2, 2, 5 };
    const vec3_t corner = { 9, 9, 0 }, middle = { 11, 0, 0 }, cap = { 0, 0, 24 };
    CHECK(CM_BoxSightTrace(0, corner, corner, bodyMin, bodyMax, model, 1) == 0);
    CHECK(CM_BoxSightTrace(0, middle, middle, bodyMin, bodyMax, model, 1) == -1);
    CHECK(CM_BoxSightTrace(0, cap, cap, bodyMin, bodyMax, model, 1) == -1);
    const vec3_t movingSphereMin = { -2, -2, -2 }, movingSphereMax = { 2, 2, 2 };
    const vec3_t movingSphereCap = { 0, 0, 22 };
    CHECK(CM_BoxSightTrace(0, corner, corner, movingSphereMin, movingSphereMax, model, 1) == 0);
    CHECK(CM_BoxSightTrace(0, middle, middle, movingSphereMin, movingSphereMax, model, 1) == -1);
    CHECK(CM_BoxSightTrace(0, movingSphereCap, movingSphereCap, movingSphereMin, movingSphereMax, model, 1) == -1);
    const vec3_t sphereMin = { -10, -10, -10 }, sphereMax = { 10, 10, 10 };
    CM_TempBoxModel(sphereMin, sphereMax, 1);
    const vec3_t sphereCorner = { 8, 0, 8 }, sphereInside = { 6, 0, 6 };
    CHECK(CM_BoxSightTrace(0, sphereCorner, sphereCorner, zero, zero, model, 1) == 0);
    CHECK(CM_BoxSightTrace(0, sphereInside, sphereInside, zero, zero, model, 1) == -1);
    const vec3_t shiftedMin = { 90, 190, 280 }, shiftedMax = { 110, 210, 320 };
    const vec3_t asymmetricMin = { 0, 0, 0 }, asymmetricMax = { 4, 4, 10 };
    const vec3_t shiftedCorner = { 107, 207, 295 }, shiftedCenter = { 98, 198, 295 };
    CM_TempBoxModel(shiftedMin, shiftedMax, 1);
    CHECK(CM_BoxSightTrace(0, shiftedCorner, shiftedCorner, asymmetricMin, asymmetricMax, model, 1) == 0);
    CHECK(CM_BoxSightTrace(0, shiftedCenter, shiftedCenter, asymmetricMin, asymmetricMax, model, 1) == -1);
}

static void SightBrush(void)
{
    Reset();
    cplane_t sidePlane = { .normal = { 0.6f, 0.8f, 0 }, .dist = 0 };
    cbrushside_t side = { .plane = &sidePlane };
    cbrush_t brushes[2] = { 0 };
    brushes[1] = (cbrush_t){ .mins = { -20, -20, -20 }, .maxs = { 20, 20, 20 },
        .contents = 1, .numsides = 1, .sides = &side };
    unsigned short brushIndex = 1;
    cLeafBrushNode_t node = { .leafBrushCount = 1, .contents = 1, .data.leaf.brushes = &brushIndex };
    cmodel_t models[2] = { 0 };
    models[1].leaf = (cLeaf_t){ .brushContents = 1, .mins = { -20, -20, -20 },
        .maxs = { 20, 20, 20 }, .leafBrushNode = 0 };
    cm.numSubModels = 2;
    cm.cmodels = models;
    cm.brushes = brushes;
    cm.numBrushes = 2;
    cm.leafbrushNodes = &node;
    cm.leafbrushNodesCount = 1;
    const vec3_t bodyMin = { -1, -2, -3 }, bodyMax = { 1, 2, 3 };
    const vec3_t outside = { 2, 0, 0 }, inside = { 1, 0, 0 }, zero = { 0, 0, 0 };
    CHECK(CM_BoxSightTrace(0, outside, outside, bodyMin, bodyMax, 1, 1) == 0);
    CHECK(CM_BoxSightTrace(0, inside, inside, bodyMin, bodyMax, 1, 1) == 2);
    CHECK(CM_BoxSightTrace(0, inside, inside, bodyMin, bodyMax, 1, 2) == 0);
    const vec3_t negative = { -1, 0, 0 };
    CHECK(CM_BoxSightTrace(0, outside, zero, zero, zero, 1, 1) == 0);
    CHECK(CM_BoxSightTrace(0, outside, negative, zero, zero, 1, 1) == 2);
    CHECK(CM_BoxSightTrace(0, zero, outside, zero, zero, 1, 1) == 0);
    CHECK(CM_BoxSightTrace(0, negative, outside, zero, zero, 1, 1) == 2);
    brushes[1].contents = 2;
    CHECK(CM_BoxSightTrace(0, inside, inside, bodyMin, bodyMax, 1, 1) == 0);
    brushes[1].contents = 1;
    sidePlane.normal[1] = 0;
    sidePlane.normal[2] = 0.8f;
    const vec3_t slopeOutside = { 5, 0, 0 }, slopeInside = { 4, 0, 0 };
    CHECK(CM_BoxSightTrace(0, slopeOutside, slopeOutside, bodyMin, bodyMax, 1, 1) == 0);
    CHECK(CM_BoxSightTrace(0, slopeInside, slopeInside, bodyMin, bodyMax, 1, 1) == 2);
    brushes[1].numsides = 0;
    for (int axis = 0; axis < 3; ++axis) {
        brushes[1].mins[axis] = -1;
        brushes[1].maxs[axis] = 1;
    }
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
            vec3_t start = { 0, 0, 0 }, end = { 0, 0, 0 };
            start[axis] = 2 * sign;
            end[axis] = sign;
            CHECK(CM_BoxSightTrace(0, start, end, zero, zero, 1, 1) == 0); // Exact end contact clears.
            end[axis] = 0.999f * sign;
            CHECK(CM_BoxSightTrace(0, start, end, zero, zero, 1, 1) == 2);
            start[axis] = sign;
            end[axis] = 2 * sign;
            CHECK(CM_BoxSightTrace(0, start, end, zero, zero, 1, 1) == 0); // Contact while leaving clears.
            end[axis] = 0;
            CHECK(CM_BoxSightTrace(0, start, end, zero, zero, 1, 1) == 2);
            CHECK(CM_BoxSightTrace(0, start, start, zero, zero, 1, 1) == 2);
        }
    }
    const vec3_t cornerStart = { -2, 0, 0 }, cornerEnd = { 0, 2, 0 };
    CHECK(CM_BoxSightTrace(0, cornerStart, cornerEnd, zero, zero, 1, 1) == 0);
    const vec3_t axialOutside = { 0, 2.5f, 0 }, axialInside = { 0, 1.9f, 0 };
    CHECK(CM_BoxSightTrace(0, axialOutside, axialOutside, bodyMin, bodyMax, 1, 1) == 0);
    CHECK(CM_BoxSightTrace(0, axialInside, axialInside, bodyMin, bodyMax, 1, 1) == 2);
    // Leaf bounds cull the brush call, even when synthetic brush bounds overlap.
    models[1].leaf.mins[0] = 50;
    models[1].leaf.maxs[0] = 60;
    CHECK(CM_BoxSightTrace(0, zero, zero, zero, zero, 1, 1) == 0);
    CHECK(leafCalls == 0);
}

static void SightTreeAndHints(void)
{
    Reset();
    const vec3_t zero = { 0, 0, 0 }, front = { 10, 0, 0 }, back = { -10, 0, 0 };
    cbrush_t brushes[2] = {
        { .mins = { -20, -20, -20 }, .maxs = { 20, 20, 20 }, .contents = 1 },
        { .mins = { -20, -20, -20 }, .maxs = { 20, 20, 20 }, .contents = 1 },
    };
    unsigned short indices[2] = { 0, 1 };
    cLeafBrushNode_t brushNodes[2] = {
        { .leafBrushCount = 1, .contents = 1, .data.leaf.brushes = &indices[0] },
        { .leafBrushCount = 1, .contents = 1, .data.leaf.brushes = &indices[1] },
    };
    cLeaf_t leaves[2] = {
        { .brushContents = 1, .mins = { -20, -20, -20 }, .maxs = { 20, 20, 20 }, .leafBrushNode = 0 },
        { .brushContents = 1, .mins = { -20, -20, -20 }, .maxs = { 20, 20, 20 }, .leafBrushNode = 1 },
    };
    cplane_t plane = { .normal = { 1, 0, 0 }, .type = 0 };
    cNode_t root = { .plane = &plane, .children = { -1, -2 } };
    cm.nodes = &root;
    cm.numNodes = 1;
    cm.leafs = leaves;
    cm.numLeafs = 2;
    cm.brushes = brushes;
    cm.numBrushes = 2;
    cm.leafbrushNodes = brushNodes;
    cm.leafbrushNodesCount = 2;
    CHECK(CM_BoxSightTrace(0, front, front, zero, zero, 0, 1) == 1);
    CHECK(CM_BoxSightTrace(0, back, back, zero, zero, 0, 1) == 2);
    const vec3_t positiveBoundary = { 0.125f, 0, 0 }, negativeBoundary = { -0.125f, 0, 0 };
    CHECK(CM_BoxSightTrace(0, positiveBoundary, positiveBoundary, zero, zero, 0, 1) == 1);
    CHECK(CM_BoxSightTrace(0, negativeBoundary, negativeBoundary, zero, zero, 0, 1) == 2);
    CHECK(CM_BoxSightTrace(0, zero, zero, zero, zero, 0, 1) == 1);
    CHECK(CM_BoxSightTrace(0, front, back, zero, zero, 0, 1) == 1);
    CHECK(CM_BoxSightTrace(0, back, front, zero, zero, 0, 1) == 2);
    CHECK(CM_BoxSightTrace(2, front, front, zero, zero, 0, 1) == 2);
    CHECK(CM_BoxSightTrace(3, front, front, zero, zero, 0, 1) == 1);
    CHECK(CM_BoxSightTrace(-1, front, front, zero, zero, 0, 1) == 1);
    // Mac checks a valid old brush directly before the contents-gated tree.
    CHECK(CM_BoxSightTrace(1, front, front, zero, zero, 0, 2) == 1);
    CHECK(CM_BoxSightTrace(0, front, front, zero, zero, 0, 2) == 0);
    brushes[0].mins[0] = 50;
    brushes[0].maxs[0] = 60;
    CHECK(CM_BoxSightTrace(1, back, back, zero, zero, 0, 1) == 2);
    // The first subtree clears, so traversal must reach the far leaf.
    CHECK(CM_BoxSightTrace(0, front, back, zero, zero, 0, 1) == 2);
    brushes[0] = brushes[1];
    plane.type = 3;
    plane.normal[0] = 0.6f;
    plane.normal[1] = 0.8f;
    CHECK(CM_BoxSightTrace(0, front, front, zero, zero, 0, 1) == 1);
    CHECK(CM_BoxSightTrace(0, back, back, zero, zero, 0, 1) == 2);
    const vec3_t bodyMin = { -1, -1, -1 }, bodyMax = { 1, 1, 1 };
    CHECK(CM_BoxSightTrace(0, back, back, bodyMin, bodyMax, 0, 1) == 1); // Diagonal box offset is 2048.
    CHECK(leafCalls == 0);

    cLeafBrushNode_t split[4] = { 0 };
    split[0] = (cLeafBrushNode_t){ .axis = 0, .contents = 1,
        .data.children = { .dist = 0, .childOffset = { 2, 3 } } };
    split[1] = brushNodes[1];
    split[2] = brushNodes[0];
    split[3] = brushNodes[1];
    cm.leafbrushNodes = split;
    cm.leafbrushNodesCount = 4;
    cmodel_t models[2] = { 0 };
    models[1].leaf = leaves[0];
    cm.cmodels = models;
    cm.numSubModels = 2;
    CHECK(CM_BoxSightTrace(2, front, front, zero, zero, 1, 1) == 1); // Submodels ignore hints.
    CHECK(CM_BoxSightTrace(0, back, back, zero, zero, 1, 1) == 2);
    CHECK(CM_BoxSightTrace(0, back, front, zero, zero, 1, 1) == 2);
    CHECK(CM_BoxSightTrace(0, front, back, zero, zero, 1, 1) == 1);
    CHECK(CM_BoxSightTrace(0, zero, zero, zero, zero, 1, 1) == 1); // Parallel split visits front first.
    split[0].leafBrushCount = -1;
    CHECK(CM_BoxSightTrace(0, front, front, zero, zero, 1, 1) == 2); // Immediate child wins.
    split[0].leafBrushCount = 0;
    split[0].data.children.range = 5;
    CHECK(CM_BoxSightTrace(0, zero, zero, zero, zero, 1, 1) == 0); // Empty gap between children.
    split[2].contents = 2;
    CHECK(CM_BoxSightTrace(0, front, back, zero, zero, 1, 1) == 2);
}

static void SightMesh(void)
{
    Reset();
    CollisionTriangle triangle = { .plane = { 0, 0, 1, 0 },
        .svec = { 0.01f, 0, 0, 0 }, .tvec = { 0, 0.01f, 0, 0 } };
    CollisionPartition partition = { .triCount = 1, .tris = &triangle };
    CollisionAabbTree trees[3] = { 0 };
    trees[2] = (CollisionAabbTree){ .origin = { 50, 50, 0 }, .halfSize = { 50, 50, 1 }, .u.partitionIndex = 0 };
    dmaterial_t material = { .contentFlags = 4, .surfaceFlags = 123 };
    cmodel_t models[2] = { 0 };
    models[1].leaf = (cLeaf_t){ .terrainContents = 4, .firstCollAabbIndex = 2, .collAabbCount = 1 };
    cm.numSubModels = 2;
    cm.cmodels = models;
    cm.numBrushes = 5;
    cm.aabbTrees = trees;
    cm.aabbTreeCount = 3;
    cm.partitions = &partition;
    cm.materials = &material;
    cm.numMaterials = 1;
    const vec3_t above = { 10, 10, 4 }, below = { 10, 10, -4 }, zero = { 0, 0, 0 };
    const vec3_t miss = { 150, 150, -4 }, bodyMin = { -1, -1, -2 }, bodyMax = { 1, 1, 2 };
    CHECK(CM_BoxSightTrace(0, above, below, zero, zero, 1, 4) == 8);
    CHECK(CM_BoxSightTrace(8, above, below, bodyMin, bodyMax, 1, 4) == 8);
    CHECK(CM_BoxSightTrace(0, below, above, zero, zero, 1, 4) == 0);
    CHECK(CM_BoxSightTrace(0, above, above, zero, zero, 1, 4) == 0);
    const vec3_t stationaryOverlap = { 10, 10, 0.5f };
    CHECK(CM_BoxSightTrace(0, stationaryOverlap, stationaryOverlap, bodyMin, bodyMax, 1, 4) == 0);
    CHECK(CM_BoxSightTrace(0, above, miss, zero, zero, 1, 4) == 0);
    CHECK(CM_BoxSightTrace(0, above, below, zero, zero, 1, 1) == 0);
    material.contentFlags = 8;
    CHECK(CM_BoxSightTrace(0, above, below, zero, zero, 1, 4) == 0);
    material.contentFlags = 4;
    cm.leafs = &models[1].leaf;
    cm.numLeafs = 1;
    cplane_t plane = { .normal = { 1, 0, 0 }, .type = 0 };
    cNode_t node = { .plane = &plane, .children = { -1, -1 } };
    cm.nodes = &node;
    cm.numNodes = 1;
    CHECK(CM_BoxSightTrace(8, above, below, zero, zero, 0, 4) == 8);
    CHECK(leafCalls == 0);
    cbrush_t brushes[5] = { 0 };
    brushes[3] = (cbrush_t){ .mins = { -20, -20, -20 }, .maxs = { 20, 20, 20 }, .contents = 1 };
    unsigned short brushIndex = 3;
    cLeafBrushNode_t brushNode = { .leafBrushCount = 1, .contents = 1, .data.leaf.brushes = &brushIndex };
    cm.brushes = brushes;
    cm.leafbrushNodes = &brushNode;
    cm.leafbrushNodesCount = 1;
    models[1].leaf.brushContents = 1;
    for (int i = 0; i < 3; ++i) {
        models[1].leaf.mins[i] = -100;
        models[1].leaf.maxs[i] = 100;
    }
    CHECK(CM_BoxSightTrace(0, above, below, zero, zero, 1, 5) == 4); // Brushes precede terrain.
    CHECK(CM_BoxSightTrace(0, above, below, zero, zero, 1, 4) == 8);
}

static int SlabCull(const TraceExtents *extents, const vec_t *mins, const vec_t *maxs, float fraction)
{
    double enter = 0, leave = fraction;
    for (int i = 0; i < 3; ++i) {
        double start = extents->start[i], delta = extents->end[i] - start;
        if (!delta) {
            if (start < mins[i] || start > maxs[i])
                return 1;
            continue;
        }
        double a = (mins[i] - start) / delta, b = (maxs[i] - start) / delta;
        if (a > b) {
            double temporary = a;
            a = b;
            b = temporary;
        }
        if (a > enter)
            enter = a;
        if (b < leave)
            leave = b;
        if (enter >= leave)
            return 1;
    }
    return 0;
}

static void TraceBoxCull(void)
{
    const vec3_t mins = { -1, -2, -3 }, maxs = { 1, 2, 3 };
    TraceExtents extents = { .start = { 0, 0, 0 }, .end = { 0, 0, 0 } };
    CM_CalcTraceEntents(&extents);
    CHECK(CM_TraceBox(&extents, mins, maxs, 1) == 0);
    extents.start[0] = -2;
    extents.end[0] = 2;
    CM_CalcTraceEntents(&extents);
    CHECK(CM_TraceBox(&extents, mins, maxs, 1) == 0); // Leaving before the end still overlaps.
    CHECK(CM_TraceBox(&extents, mins, maxs, 0.25f) == 1);
    CHECK(CM_TraceBox(&extents, mins, maxs, 0.251f) == 0);
    extents.start[0] = 2;
    extents.end[0] = -2;
    CM_CalcTraceEntents(&extents);
    CHECK(CM_TraceBox(&extents, mins, maxs, 1) == 0);
    CHECK(CM_TraceBox(&extents, mins, maxs, 0.25f) == 1);
    CHECK(CM_TraceBox(&extents, mins, maxs, 0.251f) == 0);
    unsigned int seed = 0x615b;
    for (int sample = 0; sample < 2000; ++sample) {
        for (int i = 0; i < 3; ++i) {
            seed = seed * 1664525u + 1013904223u;
            extents.start[i] = ((int)(seed >> 24) - 128) * 0.125f;
            seed = seed * 1664525u + 1013904223u;
            extents.end[i] = ((int)(seed >> 24) - 128) * 0.125f;
        }
        CM_CalcTraceEntents(&extents);
        CHECK(CM_TraceBox(&extents, mins, maxs, 1) == SlabCull(&extents, mins, maxs, 1));
    }
}
#endif

int main(void)
{
    TemporaryCapsule();
    StationaryBrush();
    StationaryMesh();
#if defined(COD2_X64)
    SightTemporaryCapsule();
    SightBrush();
    SightTreeAndHints();
    SightMesh();
    TraceBoxCull();
#endif
    FINISH("collision");
}
