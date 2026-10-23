#include "cm_trace_source.c"
#include "cm_mesh_source.c"
#include "../fidelity/check.h"

clipMap_t cm;
void *imp_cm = &cm;
static TraceThreadInfo info;
static int edgeChecks[3], vertChecks[3];
static unsigned short partitionChecks[1];
void *Sys_GetValue(int key)
{
    return &info;
}
int CM_BoxLeafnums(const vec_t *a, const vec_t *b, int *l, int n, int *last)
{
    return 0;
}
const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out)
{
    float length = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    float inverse = length ? 1.0f / length : 0;
    for (int i=0; i<3; ++i)
        out[i] = v[i] * inverse;
    return length;
}
const vec_t Vec3Normalize(vec_t *v)
{
    return Vec3NormalizeTo(v, v);
}

static void Init(traceWork_t *tw, trace_t *trace, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs)
{
    memset(&info, 0, sizeof(info));
    memset(edgeChecks, 0, sizeof(edgeChecks));
    memset(vertChecks, 0, sizeof(vertChecks));
    memset(partitionChecks, 0, sizeof(partitionChecks));
    info.checkcount.edges = edgeChecks;
    info.checkcount.verts = vertChecks;
    info.checkcount.partitions = partitionChecks;
    CM_InitTraceWork(tw, start, end, mins, maxs, 1);
    CM_ClearTraceResult(trace);
}
int main(void)
{
    traceWork_t tw;
    trace_t trace;
    const vec3_t mins = { -1,-1,-3 }, maxs = { 1,1,3 };
    // The edge runs along Y. Its first two axes span the perpendicular XZ
    // plane; axis[2] measures finite edge position, in [0,1].
    CollisionEdge edge = { .origin = { 0,0,0 }, .axis = { { 1,0,0 }, { 0,0,1 }, { 0,0.1f,0 } } };
    CollisionTriangle triangle = { .plane = { 1,0,0,0 }, .svec = { 0,0.1f,0,0 },
        .tvec = { 0,-0.1f,0.1f,-1 }, .edges = { 0,-1,-1 }, .verts = { -1,-1,-1 } };
    cm.edges = &edge;
    const vec3_t start = { 2,5,2 }, end = { 0,5,-2 };
    Init(&tw, &trace, start, end, mins, maxs);
    CM_TraceCapsuleThroughTriangle(&tw, &triangle, &trace, 0);
    // At XZ=(2,2), delta=(-2,-4), sphere radius=1.125.
    float expected = (12 - sqrtf(144 - 20 * (8 - 1.125f * 1.125f))) / 20;
    CHECK(fabsf(trace.fraction - expected) < 1e-6f);
    CHECK(trace.normal[0] > 0.7f && trace.normal[2] > 0);
    const vec3_t position = { 0.5f,5,2.5f };
    Init(&tw, &trace, position, position, mins, maxs);
    CM_PositionTestCapsuleInTriangle(&tw, &triangle, &trace);
    CHECK(trace.allsolid && trace.startsolid && trace.fraction == 0);
    // A downward-facing triangle must test the capsule's upper cap as well.
    CollisionTriangle ceiling = { .plane = { 0,0,-1,0 }, .svec = { 0.1f,0,0,0 },
        .tvec = { 0,0.1f,0,0 }, .verts = { -1,-1,-1 }, .edges = { -1,-1,-1 } };
    CollisionPartition capPartition = { .triCount = 1, .tris = &ceiling };
    CollisionAabbTree capTree = { .halfSize = { 100,100,100 } };
    const vec3_t below = { 1,1,-5 }, up = { 1,1,-1 };
    cm.partitions = &capPartition;
    Init(&tw, &trace, below, up, mins, maxs);
    CM_TraceThroughAabbTree_r(&tw, &capTree, &trace);
    CHECK(fabsf(trace.fraction - 0.46875f) < 1e-6f && trace.normal[2] == -1);
    // A border shares a partition with a triangle that this ray misses.
    // Triangles must not suppress the border loop.
    triangle.plane[3] = -100;
    CollisionBorder border = { .distEq = { 1,0,0 }, .start = -10, .length = 20, .zBase = 0 };
    CollisionPartition partition = { .triCount = 1, .borderCount = 1, .tris = &triangle, .borders = &border };
    CollisionAabbTree tree = { .halfSize = { 100,100,100 } };
    const vec3_t borderStart = { 5,0,0 }, borderEnd = { -5,0,0 };
    cm.partitions = &partition;
    Init(&tw, &trace, borderStart, borderEnd, mins, maxs);
    CM_TraceThroughAabbTree_r(&tw, &tree, &trace);
    CHECK(fabsf(trace.fraction - 0.3875f) < 1e-6f);
    CHECK(trace.normal[0] == 1 && trace.normal[1] == 0);
    // The two finite border endpoints use the same circle quadratic.
    partition.triCount = 0;
    for (int sign = -1; sign <= 1; sign += 2) {
        vec3_t a = { 3,sign * 10.5f,0 }, b = { -3,sign * 10.5f,0 };
        Init(&tw, &trace, a, b, mins, maxs);
        CM_TraceThroughAabbTree_r(&tw, &tree, &trace);
        float root = (3 - sqrtf(1.125f * 1.125f - 0.25f)) / 6;
        CHECK(fabsf(trace.fraction - root) < 1e-6f);
        CHECK(trace.normal[0] == 1 && trace.normal[1] == 0);
    }
    const vec3_t endpointStart = { 3,10.5f,0 }, endpointEnd = { -3,10.5f,2 };
    Init(&tw, &trace, endpointStart, endpointEnd, mins, maxs);
    CM_TraceThroughAabbTree_r(&tw, &tree, &trace);
    float c = 9.25f - 1.125f * 1.125f;
    CHECK(fabsf(trace.fraction - (18 - sqrtf(324 - 40 * c)) / 40) < 1e-6f);
    const vec3_t overlapStart = { 0.5f,10.5f,0 }, overlapEnd = { -0.5f,10.5f,0 };
    Init(&tw, &trace, overlapStart, overlapEnd, mins, maxs);
    CM_TraceThroughAabbTree_r(&tw, &tree, &trace);
    CHECK(trace.fraction == 0 && trace.startsolid && !trace.allsolid);
    // Radius 1.125 + epsilon 0.125 = 1.25; (0.75,1) is exact tangency.
    // The endpoint root is exactly zero and Mac rejects it.
    const vec3_t tangentMins = { -1.125f,-1.125f,-3.125f }, tangentMaxs = { 1.125f,1.125f,3.125f };
    const vec3_t tangentStart = { 0.75f,11,0 }, tangentEnd = { -0.25f,11,0 };
    Init(&tw, &trace, tangentStart, tangentEnd, tangentMins, tangentMaxs);
    CM_TraceThroughAabbTree_r(&tw, &tree, &trace);
    CHECK(trace.fraction == 1);
    // Fraction clamps to zero, while the Z test keeps the original root.
    const vec3_t behindStart = { 1,0,2.1f }, behindEnd = { -1,0,4.1f };
    Init(&tw, &trace, behindStart, behindEnd, mins, maxs);
    CM_TraceThroughAabbTree_r(&tw, &tree, &trace);
    CHECK(trace.fraction == 0 && !trace.startsolid && trace.normal[0] == 1);
    // A sphere has no axial segment: borders are deliberately skipped.
    const vec3_t sphereMins = { -1,-1,-1 }, sphereMaxs = { 1,1,1 };
    partition.triCount = 0;
    Init(&tw, &trace, borderStart, borderEnd, sphereMins, sphereMaxs);
    CM_TraceThroughAabbTree_r(&tw, &tree, &trace);
    CHECK(trace.fraction == 1);
    FINISH("collision mesh");
}
