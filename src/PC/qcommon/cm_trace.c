#include "common_types.h"
#include "imports.h"
#include "bytematch.h"
#include <math.h>
#include <string.h>

extern void *Sys_GetValue(int key);
cmodel_t *CM_ClipHandleToModel(clipHandle_t handle);
static void __attribute_regparm__(3) CM_TestInLeafBrushNode_r(traceWork_t *tw, byte *node, trace_t *trace);
#if defined(COD2_X64)
static int CM_SightTraceThroughBrush(const traceWork_t *tw, cbrush_t *brush);
static int CM_SightTraceThroughLeafBrushNode_r(const traceWork_t *tw, cLeafBrushNode_t *node, const vec_t *p1, const vec_t *p2);
static int CM_SightTraceThroughLeaf(const traceWork_t *tw, cLeaf_t *leaf, trace_t *trace);
static int CM_TraceThroughLeafBrushNode_r(const vec_t *p2, trace_t *trace);
static int CM_SightTraceThroughTree(const traceWork_t *tw, int num, const vec_t *p1, const vec_t *p2, trace_t *trace);
#else
#if COD2_APPLE_SDK
static int CM_SightTraceThroughBrush(const traceWork_t *tw, cbrush_t *brush, trace_t *tr);
#else
static int CM_SightTraceThroughBrush(cbrush_t *brush);
#endif
static int CM_SightTraceThroughLeafBrushNode_r(const vec_t *p2);
static int CM_SightTraceThroughLeaf(trace_t *trace);
static int CM_TraceThroughLeafBrushNode_r(const vec_t *p2, trace_t *trace);
static int CM_SightTraceThroughTree(const traceWork_t *tw, const vec_t *p2, trace_t *trace);
#endif
static Bool CM_TraceThroughLeafBrushNode(void);
static qboolean CM_TraceSphereThroughSphere(const vec_t *vStationary, trace_t *trace);
#if defined(COD2_X64)
static qboolean CM_SightTraceSphereThroughSphere(const traceWork_t *tw, const vec_t *start, const vec_t *end, const vec_t *center, float radius, trace_t *trace);
#else
static qboolean CM_SightTraceSphereThroughSphere(const vec_t *vStationary, trace_t *trace);
#endif
clipHandle_t CM_TempBoxModel(const vec_t *mins, const vec_t *maxs, int contents);
static int CM_TraceThroughTree(const traceWork_t *tw, int num, const vec_t *p1, const vec_t *p2, trace_t *trace);
int CM_ContentsOfModel(clipHandle_t handle);
float CM_RadiusOfModel(clipHandle_t handle);
static int __attribute_regparm__(3) CM_Trace(trace_t *results, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs, clipHandle_t model, int brushmask);
int CM_BoxTrace(trace_t *results, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs, clipHandle_t model, int brushmask);
int CM_BoxSightTrace(int oldHitNum, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs, clipHandle_t model, int brushmask);
int CM_TransformedBoxTrace(trace_t *results, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs, clipHandle_t model, int brushmask, const vec_t *origin, const vec_t *angles);
int CM_TransformedBoxTraceExternal(trace_t *results, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs, clipHandle_t model, int brushmask, const vec_t *origin, const vec_t *angles);
int CM_TransformedBoxSightTrace(int hitNum, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs, clipHandle_t model, int brushmask, const vec_t *origin, const vec_t *angles);

cmodel_t *CM_ClipHandleToModel(clipHandle_t handle)
{
    clipMap_t *cm = (clipMap_t *)imp_cm;
    if (handle < cm->numSubModels) {
        return &cm->cmodels[handle];
    }
#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)

    return ((struct TraceThreadInfo *)Sys_GetValue(3))->box_model;
#else
    return *(cmodel_t **)((char *)Sys_GetValue(3) + 0x14);
#endif
}

extern clipMap_t cm;
extern void CM_CalcTraceEntents(TraceExtents *extents);
extern int CM_BoxLeafnums(const vec_t *mins, const vec_t *maxs, int *list, int listsize, int *lastLeaf);
extern short int CM_MeshTestInLeaf(const traceWork_t *tw, cLeaf_t *leaf, trace_t *trace);
extern void CM_TraceThroughAabbTree(const traceWork_t *tw, CollisionAabbTree *aabbTree, trace_t *trace);
#if defined(COD2_X64)
extern void CM_SightTraceThroughAabbTree(const traceWork_t *tw, CollisionAabbTree *aabbTree, trace_t *trace);
extern qboolean CM_TraceBox(const TraceExtents *extents, const vec_t *mins, const vec_t *maxs, float fraction);
#endif
extern void AnglesToAxis(const vec_t *angles, vec3_t *axis);
extern void MatrixTransformVector(const vec_t *in, const vec_t *matrix, vec_t *out);
extern void MatrixTransposeTransformVector(const vec_t *in, const vec_t *matrix, vec_t *out);

#define CM_TEMP_BOX_MODEL ((clipHandle_t)0x3ff)

/* Retail inlined the trace-work setup and model resolution into every tracer
 * (Mac STABS shows no standalone CM_InitTraceWork/CM_ModelForHandle). __attribute__
 * is stripped on the VC7.1 matching shim, so force the inline with __forceinline. */
#if defined(_MSC_VER)
#define CM_FORCEINLINE __forceinline
#else
#define CM_FORCEINLINE inline __attribute__((always_inline))
#endif

static cbrush_t cm_fallbackBoxBrush;
static cmodel_t cm_fallbackBoxModel;

static float CM_DotProduct(const vec_t *a, const vec_t *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float CM_AbsFloat(float value)
{
    return value < 0.0f ? -value : value;
}

static float CM_MinFloat(float a, float b)
{
    return a < b ? a : b;
}

static inline __attribute__((always_inline)) void CM_CopyVec3(const vec_t *src, vec_t *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline __attribute__((always_inline)) void CM_ClearTraceResult(trace_t *trace)
{
    memset(trace, 0, sizeof(*trace));
    trace->fraction = 1.0f;
}

static inline __attribute__((always_inline)) TraceThreadInfo *CM_GetThreadInfo(void)
{
    return (TraceThreadInfo *)Sys_GetValue(3);
}

static CM_FORCEINLINE cmodel_t *CM_ModelForHandle(clipHandle_t handle)
{
    if (handle < cm.numSubModels)
        return &cm.cmodels[handle];

    if (handle == CM_TEMP_BOX_MODEL) {
        TraceThreadInfo *threadInfo = CM_GetThreadInfo();
        if (threadInfo && threadInfo->box_model)
            return threadInfo->box_model;
        return &cm_fallbackBoxModel;
    }

    return CM_ClipHandleToModel(handle);
}

static CM_FORCEINLINE cbrush_t *CM_BoxBrush(void)
{
    TraceThreadInfo *threadInfo = CM_GetThreadInfo();
    if (threadInfo && threadInfo->box_brush)
        return threadInfo->box_brush;
    return &cm_fallbackBoxBrush;
}

static CM_FORCEINLINE void CM_InitTraceThreadInfo(traceWork_t *tw)
{
    TraceThreadInfo *threadInfo = CM_GetThreadInfo();

    memset(&tw->threadInfo, 0, sizeof(tw->threadInfo));
    if (!threadInfo)
        return;

    threadInfo->checkcount.global++;
    tw->threadInfo = *threadInfo;
}

static CM_FORCEINLINE void CM_InitTraceWork(traceWork_t *tw, const vec_t *start, const vec_t *end,
                             const vec_t *mins, const vec_t *maxs, int brushmask)
{
    int axis;
    float sizeSum;
    float radius;

    memset(tw, 0, sizeof(*tw));

    for (axis = 0; axis < 3; axis++) {
        float offset = (mins[axis] + maxs[axis]) * 0.5f;

        tw->size[axis] = maxs[axis] - offset;
        tw->extents.start[axis] = start[axis] + offset;
        tw->extents.end[axis] = end[axis] + offset;
        tw->midpoint[axis] = (tw->extents.start[axis] + tw->extents.end[axis]) * 0.5f;
        tw->delta[axis] = tw->extents.end[axis] - tw->extents.start[axis];
        tw->halfDelta[axis] = tw->delta[axis] * 0.5f;
        tw->halfDeltaAbs[axis] = CM_AbsFloat(tw->halfDelta[axis]);
    }

    CM_CalcTraceEntents(&tw->extents);

    tw->deltaLenSq = CM_DotProduct(tw->delta, tw->delta);
    tw->deltaLen = sqrtf(tw->deltaLenSq);

    radius = CM_MinFloat(CM_AbsFloat(tw->size[0]), CM_AbsFloat(tw->size[2]));
    tw->radius = radius;
    tw->offsetZ = tw->size[2] - radius;

    for (axis = 0; axis < 3; axis++) {
        float expand = axis == 2 ? CM_AbsFloat(tw->size[2]) : radius;
        float startAxis = tw->extents.start[axis];
        float endAxis = tw->extents.end[axis];

        if (endAxis > startAxis) {
            tw->bounds[0][axis] = startAxis - expand;
            tw->bounds[1][axis] = endAxis + expand;
        } else {
            tw->bounds[0][axis] = endAxis - expand;
            tw->bounds[1][axis] = startAxis + expand;
        }
    }

    tw->contents = brushmask;
    sizeSum = CM_AbsFloat(tw->size[0]) + CM_AbsFloat(tw->size[1]) + CM_AbsFloat(tw->size[2]);
    tw->isPoint = (sizeSum == 0.0f);
    tw->axialCullOnly = 0;
    tw->radiusOffset[0] = radius;
    tw->radiusOffset[1] = radius;
    tw->radiusOffset[2] = CM_AbsFloat(tw->size[2]);

    CM_InitTraceThreadInfo(tw);
}

static void CM_SetTraceMaterial(trace_t *trace, int materialNum, int contents)
{
    trace->contents = contents;
    if (materialNum >= 0 && materialNum < cm.numMaterials) {
        trace->surfaceFlags = cm.materials[materialNum].surfaceFlags;
        trace->material = (const char *)&cm.materials[materialNum];
    }
}

static void CM_TraceBrushPlane(const traceWork_t *tw, const vec_t *normal, float dist,
                               int materialNum, float *enterFrac, float *leaveFrac,
                               vec3_t enterNormal, int *enterMaterial, int *startsOut,
                               int *getOut)
{
    float support;
    float expandedDist;
    float startDist;
    float endDist;
    float denominator;
    float fraction;

    support = CM_AbsFloat(normal[0]) * CM_AbsFloat(tw->size[0]) +
              CM_AbsFloat(normal[1]) * CM_AbsFloat(tw->size[1]) +
              CM_AbsFloat(normal[2]) * CM_AbsFloat(tw->size[2]);
    expandedDist = dist + support;

    startDist = CM_DotProduct(tw->extents.start, normal) - expandedDist;
    endDist = CM_DotProduct(tw->extents.end, normal) - expandedDist;

    if (startDist > 0.0f)
        *startsOut = 1;
    if (endDist > 0.0f)
        *getOut = 1;

    if (startDist > 0.0f && endDist > 0.0f) {
        *enterFrac = 1.0f;
        *leaveFrac = 0.0f;
        return;
    }

    if (startDist <= 0.0f && endDist <= 0.0f)
        return;

    denominator = startDist - endDist;
    if (denominator == 0.0f)
        return;

    if (startDist > endDist) {
        fraction = (startDist - 0.125f) / denominator;
        if (fraction > *enterFrac) {
            *enterFrac = fraction;
            enterNormal[0] = normal[0];
            enterNormal[1] = normal[1];
            enterNormal[2] = normal[2];
            *enterMaterial = materialNum;
        }
    } else {
        fraction = (startDist + 0.125f) / denominator;
        if (fraction < *leaveFrac)
            *leaveFrac = fraction;
    }
}

static void CM_TraceThroughBrush(const traceWork_t *tw, cbrush_t *brush, trace_t *trace)
{
    int axis;
    int sideIndex;
    int startsOut = 0;
    int getOut = 0;
    int enterMaterial = -1;
    float enterFrac = -1.0f;
    float leaveFrac = 1.0f;
    vec3_t enterNormal = { 0.0f, 0.0f, 0.0f };

    if (!brush || !(brush->contents & tw->contents))
        return;

    for (axis = 0; axis < 3; axis++) {
        if (tw->bounds[0][axis] > brush->maxs[axis] + 1.0f)
            return;
        if (tw->bounds[1][axis] < brush->mins[axis] - 1.0f)
            return;
    }

    for (axis = 0; axis < 3; axis++) {
        vec3_t normal;

        normal[0] = 0.0f;
        normal[1] = 0.0f;
        normal[2] = 0.0f;
        normal[axis] = 1.0f;
        CM_TraceBrushPlane(tw, normal, brush->maxs[axis],
                           brush->axialMaterialNum[1][axis],
                           &enterFrac, &leaveFrac, enterNormal, &enterMaterial,
                           &startsOut, &getOut);
        if (enterFrac >= leaveFrac)
            return;

        normal[axis] = -1.0f;
        CM_TraceBrushPlane(tw, normal, -brush->mins[axis],
                           brush->axialMaterialNum[0][axis],
                           &enterFrac, &leaveFrac, enterNormal, &enterMaterial,
                           &startsOut, &getOut);
        if (enterFrac >= leaveFrac)
            return;
    }

    for (sideIndex = 0; sideIndex < brush->numsides; sideIndex++) {
        cbrushside_t *side = &brush->sides[sideIndex];
        if (!side->plane)
            continue;

        CM_TraceBrushPlane(tw, side->plane->normal, side->plane->dist,
                           side->materialNum, &enterFrac, &leaveFrac,
                           enterNormal, &enterMaterial, &startsOut, &getOut);
        if (enterFrac >= leaveFrac)
            return;
    }

    if (!startsOut) {
        trace->startsolid = 1;
        CM_SetTraceMaterial(trace, enterMaterial, brush->contents);
        if (!getOut) {
            trace->allsolid = 1;
            trace->fraction = 0.0f;
        }
        return;
    }

    if (enterFrac < 0.0f)
        enterFrac = 0.0f;

    if (enterFrac < leaveFrac && enterFrac < trace->fraction) {
        trace->fraction = enterFrac;
        trace->normal[0] = enterNormal[0];
        trace->normal[1] = enterNormal[1];
        trace->normal[2] = enterNormal[2];
        CM_SetTraceMaterial(trace, enterMaterial, brush->contents);
    }
}

static void CM_TraceLeafBrushNode_r(const traceWork_t *tw, cLeafBrushNode_t *node,
                                    trace_t *trace, int depth)
{
    int i;

    if (!node || depth > 64 || trace->allsolid)
        return;

    if (!(node->contents & tw->contents))
        return;

    if (node->leafBrushCount > 0) {
        for (i = 0; i < node->leafBrushCount; i++) {
            unsigned short brushIndex = node->data.leaf.brushes[i];
            if (brushIndex < cm.numBrushes)
                CM_TraceThroughBrush(tw, &cm.brushes[brushIndex], trace);
            if (trace->allsolid)
                return;
        }
        return;
    }

    if (node->leafBrushCount < 0)
        CM_TraceLeafBrushNode_r(tw, node + 1, trace, depth + 1);

    if (node->data.children.childOffset[0] != 0)
        CM_TraceLeafBrushNode_r(tw, node + node->data.children.childOffset[0], trace, depth + 1);
    if (node->data.children.childOffset[1] != 0 &&
        node->data.children.childOffset[1] != node->data.children.childOffset[0])
        CM_TraceLeafBrushNode_r(tw, node + node->data.children.childOffset[1], trace, depth + 1);
}

static void CM_TraceLeafBrushes(const traceWork_t *tw, cLeaf_t *leaf, trace_t *trace)
{
    if (!leaf || !(leaf->brushContents & tw->contents))
        return;

    if (leaf->leafBrushNode >= 0 && leaf->leafBrushNode < cm.leafbrushNodesCount)
        CM_TraceLeafBrushNode_r(tw, &cm.leafbrushNodes[leaf->leafBrushNode], trace, 0);
}

static void CM_TraceLeafTerrain(const traceWork_t *tw, cLeaf_t *leaf, trace_t *trace)
{
    int k;

    if (!leaf || !(leaf->terrainContents & tw->contents))
        return;

    for (k = 0; k < leaf->collAabbCount; k++) {
        int treeIndex = leaf->firstCollAabbIndex + k;
        if (treeIndex >= 0 && treeIndex < cm.aabbTreeCount)
            CM_TraceThroughAabbTree(tw, &cm.aabbTrees[treeIndex], trace);
        if (trace->allsolid || trace->fraction == 0.0f)
            return;
    }
}

static void CM_TraceLeaf(const traceWork_t *tw, cLeaf_t *leaf, trace_t *trace)
{
    CM_TraceLeafBrushes(tw, leaf, trace);
    if (!trace->allsolid)
        CM_TraceLeafTerrain(tw, leaf, trace);
}

/* Collision epsilons and clamp bounds. */
#define CM_AXIAL_EPS   0.125f     /* axial box-offset epsilon */
#define CM_DIAG_OFFSET 2048.0f    /* box on a diagonal plane: conservative straddle */
#define CM_ONE         1.0f       /* reciprocal numerator / clamp bound */
/* Memory-resident static const rather than #define, so the comparison against them
 * emits `fcomp ds:[const]` rather than `ftst`. */
static const float cm_splitZero = 0.0f;             /* sign-test threshold */
static const float cm_parallelEps = 4.76837158e-07f;/* 0x35000000 (2^-21) */
#define CM_SPLIT_EPS   cm_splitZero
#define CM_PARALLEL_EP cm_parallelEps

/* Recursive parametric BSP box-trace: internal nodes split the (box-center) segment
 * on the node plane; a leaf (num<0) traces inline.
 * p1/p2 are vec4 {x,y,z,frac}. Recurses the near child, iterates the far. */
static int CM_TraceThroughTree(const traceWork_t *tw, int num,
                               const vec_t *p1_in, const vec_t *p2, trace_t *trace)
{
    float p1[4];
    float mid[4];
    int i;

    p1[0] = p1_in[0];
    p1[1] = p1_in[1];
    p1[2] = p1_in[2];
    p1[3] = p1_in[3];

    while (num >= 0) {
        cNode_t *node = &cm.nodes[num];
        cplane_t *plane = node->plane;
        float d1, d2, offset, spread, aspread, sd1, inv, fracA, fracB;
        int nearSide;

        if (plane->type < 3) {
            d1 = p1[plane->type] - plane->dist;
            d2 = p2[plane->type] - plane->dist;
            offset = tw->size[plane->type] + CM_AXIAL_EPS;
        } else {
            d1 = p1[0] * plane->normal[0] + p1[1] * plane->normal[1] + p1[2] * plane->normal[2] - plane->dist;
            d2 = p2[0] * plane->normal[0] + p2[1] * plane->normal[1] + p2[2] * plane->normal[2] - plane->dist;
            offset = tw->isPoint ? CM_AXIAL_EPS : CM_DIAG_OFFSET;
        }

        if ((d2 - d1 < CM_SPLIT_EPS ? d2 : d1) >= offset) {
            num = node->children[0];
            continue;
        }
        if ((d1 - d2 < CM_SPLIT_EPS ? d2 : d1) <= -offset) {
            num = node->children[1];
            continue;
        }

        if (trace->fraction < p1[3])
            return 0;

        spread = d2 - d1;
        aspread = fabsf(spread);

        if (aspread <= CM_PARALLEL_EP) {
            fracA = 0.0f;
            fracB = CM_ONE;
            nearSide = 0;
        } else {
            sd1 = spread < CM_SPLIT_EPS ? d1 : -d1;
            inv = CM_ONE / aspread;
            fracA = (sd1 - offset) * inv;
            fracB = (sd1 + offset) * inv;
            nearSide = spread < CM_SPLIT_EPS ? 0 : 1;
        }

        /* near sub-segment [p1 .. p1+fracB*(p2-p1)] -> children[nearSide] */
        if (CM_ONE - fracB < CM_SPLIT_EPS)
            fracB = CM_ONE;
        for (i = 0; i < 4; i++)
            mid[i] = (p2[i] - p1[i]) * fracB + p1[i];
        CM_TraceThroughTree(tw, node->children[nearSide], p1, mid, trace);

        /* far sub-segment: advance p1 to p1+fracA*(p2-p1) -> children[nearSide^1] */
        if (fracA < CM_SPLIT_EPS)
            fracA = 0.0f;
        for (i = 0; i < 4; i++)
            p1[i] = (p2[i] - p1[i]) * fracA + p1[i];
        num = node->children[nearSide ^ 1];
    }

    CM_TraceLeaf(tw, &cm.leafs[-1 - num], trace);
    return 0;
}
#if defined(COD2_X64)

extern const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out);

static void CM_TestBrushPosition(const traceWork_t *tw, const cbrush_t *brush, trace_t *trace)
{
    int i;

    if (!(brush->contents & tw->contents))
        return;
    for (i = 0; i < 3; i++) {
        if (tw->bounds[0][i] > brush->maxs[i] || tw->bounds[1][i] < brush->mins[i])
            return;
    }
    for (i = 0; i < brush->numsides; i++) {
        const cplane_t *plane = brush->sides[i].plane;
        float support = tw->radius + CM_AbsFloat(tw->offsetZ * plane->normal[2]);
        if (CM_DotProduct(tw->extents.start, plane->normal) > plane->dist + support)
            return;
    }
    trace->startsolid = 1;
    trace->allsolid = 1;
    trace->fraction = 0.0f;
    trace->contents = brush->contents;
}

static void CM_TestLeafBrushNode(const traceWork_t *tw, const cLeafBrushNode_t *node, trace_t *trace, int depth)
{
    int i;

    if (depth > 64 || trace->allsolid || !(node->contents & tw->contents))
        return;
    if (node->leafBrushCount > 0) {
        for (i = 0; i < node->leafBrushCount && !trace->allsolid; i++) {
            unsigned short index = node->data.leaf.brushes[i];
            if (index < cm.numBrushes)
                CM_TestBrushPosition(tw, &cm.brushes[index], trace);
        }
        return;
    }
    if (node->leafBrushCount < 0)
        CM_TestLeafBrushNode(tw, &node[1], trace, depth + 1);
    if (trace->allsolid)
        return;
    if (tw->bounds[0][node->axis] > node->data.children.dist) {
        CM_TestLeafBrushNode(tw, node + node->data.children.childOffset[0], trace, depth + 1);
    } else if (tw->bounds[1][node->axis] < node->data.children.dist) {
        CM_TestLeafBrushNode(tw, node + node->data.children.childOffset[1], trace, depth + 1);
    } else {
        CM_TestLeafBrushNode(tw, node + node->data.children.childOffset[0], trace, depth + 1);
        if (!trace->allsolid)
            CM_TestLeafBrushNode(tw, node + node->data.children.childOffset[1], trace, depth + 1);
    }
}

static void CM_TestLeafPosition(const traceWork_t *tw, cLeaf_t *leaf, trace_t *trace)
{
    int i;
    int overlap = 1;

    if (leaf->brushContents & tw->contents) {
        for (i = 0; i < 3; i++) {
            if (leaf->mins[i] >= tw->bounds[1][i] || tw->bounds[0][i] >= leaf->maxs[i])
                overlap = 0;
        }
        if (overlap && leaf->leafBrushNode >= 0 && leaf->leafBrushNode < cm.leafbrushNodesCount)
            CM_TestLeafBrushNode(tw, &cm.leafbrushNodes[leaf->leafBrushNode], trace, 0);
    }
    if (!trace->allsolid && (leaf->terrainContents & tw->contents))
        CM_MeshTestInLeaf(tw, leaf, trace);
}

static void CM_TempCapsuleGeometry(const cmodel_t *model, vec3_t center, float *radius, float *offsetZ)
{
    int i;

    for (i = 0; i < 3; i++)
        center[i] = (model->mins[i] + model->maxs[i]) * 0.5f;
    *radius = CM_MinFloat(model->maxs[0] - center[0], model->maxs[2] - center[2]);
    *offsetZ = model->maxs[2] - center[2] - *radius;
}

static void CM_TestTempCapsulePosition(const traceWork_t *tw, const cmodel_t *model, trace_t *trace)
{
    vec3_t center;
    float radius, offsetZ, x, y, z;

    if (!(CM_BoxBrush()->contents & tw->contents))
        return;
    CM_TempCapsuleGeometry(model, center, &radius, &offsetZ);
    radius += tw->radius;
    offsetZ += tw->offsetZ;
    x = tw->extents.start[0] - center[0];
    y = tw->extents.start[1] - center[1];
    z = CM_AbsFloat(tw->extents.start[2] - center[2]) - offsetZ;
    if (z < 0.0f)
        z = 0.0f;
    if (x * x + y * y + z * z < radius * radius) {
        trace->startsolid = 1;
        trace->allsolid = 1;
        trace->fraction = 0.0f;
    }
}

/* The reference stores the start separation normal, and backs the quadratic
 * contact up by separation length * 0.125 / projection (Mac 0x63cec..0x63d14). */
static int CM_TraceTempSphere(const traceWork_t *tw, const vec_t *start, const vec_t *end,
                              const vec_t *center, float radius, trace_t *trace)
{
    vec3_t separation, normal;
    float distanceSq, c, projection, discriminant, length, fraction;
    int i;

    for (i = 0; i < 3; i++)
        separation[i] = start[i] - center[i];
    radius += tw->radius;
    distanceSq = CM_DotProduct(separation, separation);
    c = distanceSq - radius * radius;
    length = Vec3NormalizeTo(separation, normal);
    if (c <= 0.0f) {
        trace->fraction = 0.0f;
        trace->startsolid = 1;
        CM_CopyVec3(normal, trace->normal);
        trace->contents = CM_BoxBrush()->contents;
        for (i = 0; i < 3; i++)
            separation[i] = end[i] - center[i];
        if (CM_DotProduct(separation, separation) <= radius * radius)
            trace->allsolid = 1;
        return 0;
    }
    projection = CM_DotProduct(separation, tw->delta);
    if (projection >= 0.0f)
        return 1;
    discriminant = projection * projection - tw->deltaLenSq * c;
    if (discriminant < 0.0f)
        return 1;
    fraction = (-projection - sqrtf(discriminant)) / tw->deltaLenSq + length * 0.125f / projection;
    if (fraction >= trace->fraction)
        return 1;
    trace->fraction = fraction < 0.0f ? 0.0f : fraction;
    CM_CopyVec3(normal, trace->normal);
    trace->contents = CM_BoxBrush()->contents;
    return 0;
}

static void CM_TraceTempCapsule(const traceWork_t *tw, const cmodel_t *model, trace_t *trace)
{
    vec3_t center, top, bottom, startTop, startBottom, endTop, endBottom, normal;
    float radius, offsetZ, totalRadius, totalOffset, x, y, c, projection, lengthSq;
    float discriminant, length, root, epsilon, fraction;
    int i;

    if (!(CM_BoxBrush()->contents & tw->contents))
        return;
    for (i = 0; i < 3; i++) {
        if (tw->bounds[0][i] > model->maxs[i] + 1.0f || tw->bounds[1][i] < model->mins[i] - 1.0f)
            return;
    }
    CM_TempCapsuleGeometry(model, center, &radius, &offsetZ);
    CM_CopyVec3(center, top);
    CM_CopyVec3(center, bottom);
    top[2] += offsetZ;
    bottom[2] -= offsetZ;
    CM_CopyVec3(tw->extents.start, startTop);
    CM_CopyVec3(tw->extents.start, startBottom);
    CM_CopyVec3(tw->extents.end, endTop);
    CM_CopyVec3(tw->extents.end, endBottom);
    startTop[2] += tw->offsetZ;
    startBottom[2] -= tw->offsetZ;
    endTop[2] += tw->offsetZ;
    endBottom[2] -= tw->offsetZ;
    if (startBottom[2] > top[2]) {
        if (!CM_TraceTempSphere(tw, startBottom, endBottom, top, radius, trace) || tw->delta[2] >= 0.0f)
            return;
    } else if (startTop[2] < bottom[2]) {
        if (!CM_TraceTempSphere(tw, startTop, endTop, bottom, radius, trace) || tw->delta[2] <= 0.0f)
            return;
    }
    totalRadius = radius + tw->radius;
    totalOffset = offsetZ + tw->offsetZ;
    x = tw->extents.start[0] - center[0];
    y = tw->extents.start[1] - center[1];
    normal[0] = x;
    normal[1] = y;
    normal[2] = 0.0f;
    length = Vec3NormalizeTo(normal, normal);
    c = x * x + y * y - totalRadius * totalRadius;
    if (c <= 0.0f) {
        if (CM_AbsFloat(tw->extents.start[2] - center[2]) <= totalOffset) {
            trace->fraction = 0.0f;
            trace->startsolid = 1;
            CM_CopyVec3(normal, trace->normal);
            trace->contents = CM_BoxBrush()->contents;
            if (CM_AbsFloat(tw->extents.end[2] - center[2]) <= totalOffset)
                trace->allsolid = 1;
            return;
        }
    } else {
        projection = x * tw->delta[0] + y * tw->delta[1];
        if (projection < 0.0f) {
            lengthSq = tw->delta[0] * tw->delta[0] + tw->delta[1] * tw->delta[1];
            discriminant = projection * projection - lengthSq * c;
            if (discriminant >= 0.0f) {
                root = (-projection - sqrtf(discriminant)) / lengthSq;
                epsilon = length * 0.125f / projection;
                fraction = root + epsilon;
                if (fraction < trace->fraction && CM_AbsFloat(tw->extents.start[2] + root * tw->delta[2] - center[2]) <= totalOffset) {
                    trace->fraction = fraction < 0.0f ? 0.0f : fraction;
                    CM_CopyVec3(normal, trace->normal);
                    trace->contents = CM_BoxBrush()->contents;
                    return;
                }
            }
        }
    }
    if (endBottom[2] > top[2] && startBottom[2] <= top[2])
        CM_TraceTempSphere(tw, startBottom, endBottom, top, radius, trace);
    else if (endTop[2] < bottom[2] && startTop[2] >= bottom[2])
        CM_TraceTempSphere(tw, startTop, endTop, bottom, radius, trace);
}
#endif

static int __attribute_regparm__(3) CM_Trace(trace_t *results, const vec_t *start, const vec_t *end,
                                                         const vec_t *mins, const vec_t *maxs,
                                                         clipHandle_t model, int brushmask)
{
    traceWork_t tw;
    cmodel_t *cmodel;

    CM_InitTraceWork(&tw, start, end, mins, maxs, brushmask);
#if defined(COD2_X64)

    if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2]) {
        if (model == CM_TEMP_BOX_MODEL) {
            CM_TestTempCapsulePosition(&tw, CM_ModelForHandle(model), results);
        } else if (model != 0) {
            cmodel = CM_ModelForHandle(model);
            if (cmodel)
                CM_TestLeafPosition(&tw, &cmodel->leaf, results);
        } else {
            int leafs[1024], lastLeaf = 0, count, i;
            vec3_t boundsMin, boundsMax;

            for (i = 0; i < 3; i++) {
                boundsMin[i] = tw.extents.start[i] - tw.size[i] - 1.0f;
                boundsMax[i] = tw.extents.start[i] + tw.size[i] + 1.0f;
            }
            count = CM_BoxLeafnums(boundsMin, boundsMax, leafs, 1024, &lastLeaf);
            for (i = 0; i < count && !results->allsolid; i++) {
                if (leafs[i] >= 0 && leafs[i] < cm.numLeafs)
                    CM_TestLeafPosition(&tw, &cm.leafs[leafs[i]], results);
            }
        }
        return 0;
    }
#endif

    if (model == CM_TEMP_BOX_MODEL) {
#if defined(COD2_X64)
        CM_TraceTempCapsule(&tw, CM_ModelForHandle(model), results);
#else
        CM_TraceThroughBrush(&tw, CM_BoxBrush(), results);
#endif
        return 0;
    }

    if (model != 0) {
        cmodel = CM_ModelForHandle(model);
        if (cmodel)
            CM_TraceLeaf(&tw, &cmodel->leaf, results);
        return 0;
    }

    {
        float p1[4], p2[4];
        int i;

        for (i = 0; i < 3; i++) {
            p1[i] = tw.extents.start[i];
            p2[i] = tw.extents.end[i];
        }
        p1[3] = 0.0f;
        p2[3] = 1.0f;
        CM_TraceThroughTree(&tw, 0, p1, p2, results);
    }

    return 0;
}

int CM_ContentsOfModel(clipHandle_t handle)
{
    clipMap_t *cm = (clipMap_t *)imp_cm;
    cmodel_t *model;

    if (handle < cm->numSubModels)
        model = &cm->cmodels[handle];
    else
#if defined(COD2_X64)
        model = ((TraceThreadInfo *)Sys_GetValue(3))->box_model;
#else
        model = *(cmodel_t **)((byte *)Sys_GetValue(3) + 0x14);
#endif

    return model->leaf.brushContents | model->leaf.terrainContents;
}

float CM_RadiusOfModel(clipHandle_t handle)
{
    clipMap_t *cm = (clipMap_t *)imp_cm;
    cmodel_t *model;

    if (handle < cm->numSubModels)
        model = &cm->cmodels[handle];
    else
#if defined(COD2_X64)
        model = ((TraceThreadInfo *)Sys_GetValue(3))->box_model;
#else
        model = *(cmodel_t **)((byte *)Sys_GetValue(3) + 0x14);
#endif

    return model->radius;
}

clipHandle_t CM_TempBoxModel(const vec_t *mins, const vec_t *maxs, int contents)
{
    TraceThreadInfo *threadInfo = CM_GetThreadInfo();
    cbrush_t *brush = threadInfo->box_brush;
    cmodel_t *model = threadInfo->box_model;

    CM_CopyVec3(mins, model->mins);
    CM_CopyVec3(maxs, model->maxs);
    CM_CopyVec3(mins, brush->mins);
    CM_CopyVec3(maxs, brush->maxs);
    brush->contents = contents;

    return CM_TEMP_BOX_MODEL;
}

int CM_BoxTrace(trace_t *results, const vec_t *start, const vec_t *end,
                const vec_t *mins, const vec_t *maxs, clipHandle_t model, int brushmask)
{
    CM_ClearTraceResult(results);
    return CM_Trace(results, start, end, mins, maxs, model, brushmask);
}

extern const vec_t Vec3Normalize(vec_t *v);
extern const vec_t Vec3NormalizeTo(const vec_t *v, vec_t *out);

#if defined(COD2_X64)
/* Sight brushes clip an unshifted interval and return a stable brush hint.
 * Capsule support and endpoint comparisons follow Mac 0x628fe..0x62b97. */
static int CM_SightTraceThroughBrush(const traceWork_t *tw, cbrush_t *brush)
{
    float enter = 0.0f, leave = 1.0f;
    int pass, axis, i;

    for (pass = 0; pass < 2; pass++) {
        const vec_t *bounds = pass ? brush->maxs : brush->mins;
        float sign = pass ? 1.0f : -1.0f;

        for (axis = 0; axis < 3; axis++) {
            float startDist = (tw->extents.start[axis] - bounds[axis]) * sign - tw->radiusOffset[axis];
            float endDist = (tw->extents.end[axis] - bounds[axis]) * sign - tw->radiusOffset[axis];
            float fraction;

            if (startDist > 0.0f) {
                if (endDist > 0.0f)
                    return 0;
                fraction = startDist * tw->extents.invDelta[axis] * sign;
                if (fraction >= leave)
                    return 0;
                if (fraction > enter)
                    enter = fraction;
            } else if (endDist > 0.0f) {
                fraction = startDist * tw->extents.invDelta[axis] * sign;
                if (enter >= fraction)
                    return 0;
                if (fraction < leave)
                    leave = fraction;
            }
        }
    }
    for (i = 0; i < brush->numsides; i++) {
        const cplane_t *plane = brush->sides[i].plane;
        float expandedDist = CM_AbsFloat(tw->offsetZ * plane->normal[2]) + (tw->radius + plane->dist);
        float startDist = CM_DotProduct(tw->extents.start, plane->normal) - expandedDist;
        float endDist = CM_DotProduct(tw->extents.end, plane->normal) - expandedDist;
        float denominator = startDist - endDist;

        if (startDist > 0.0f) {
            if (endDist > 0.0f)
                return 0;
            if (startDist > enter * denominator) {
                enter = startDist / denominator;
                if (enter >= leave)
                    return 0;
            }
        } else if (endDist > 0.0f && startDist > leave * denominator) {
            leave = startDist / denominator;
            if (leave <= enter)
                return 0;
        }
    }
    return (int)(brush - cm.brushes) + 1;
}

static int CM_SightTraceThroughLeafBrushNode_r(const traceWork_t *tw, cLeafBrushNode_t *node,
                                              const vec_t *p1_in, const vec_t *p2)
{
    vec3_t p1, mid;
    int i, hit, side;

    CM_CopyVec3(p1_in, p1);
    while (node->contents & tw->contents) {
        if (node->leafBrushCount > 0) {
            for (i = 0; i < node->leafBrushCount; i++) {
                unsigned short index = node->data.leaf.brushes[i];
                if (index < cm.numBrushes && (cm.brushes[index].contents & tw->contents)) {
                    hit = CM_SightTraceThroughBrush(tw, &cm.brushes[index]);
                    if (hit)
                        return hit;
                }
            }
            return 0;
        }
        if (node->leafBrushCount < 0) {
            hit = CM_SightTraceThroughLeafBrushNode_r(tw, &node[1], p1, p2);
            if (hit)
                return hit;
        }
        float d1 = p1[node->axis] - node->data.children.dist;
        float d2 = p2[node->axis] - node->data.children.dist;
        float offset = (tw->size[node->axis] + 0.125f) - node->data.children.range;
        float spread = d2 - d1;
        float fractionNear, fractionFar;

        if (CM_MinFloat(d1, d2) >= offset) {
            if ((d1 > d2 ? d1 : d2) <= -offset)
                return 0;
            node += node->data.children.childOffset[0];
            continue;
        }
        if ((d1 > d2 ? d1 : d2) <= -offset) {
            node += node->data.children.childOffset[1];
            continue;
        }
        if (CM_AbsFloat(spread) <= CM_PARALLEL_EP) {
            fractionNear = 1.0f;
            fractionFar = 0.0f;
            side = 0;
        } else {
            float signedDist = spread >= 0.0f ? -d1 : d1;
            float inverse = 1.0f / CM_AbsFloat(spread);
            fractionNear = (signedDist + offset) * inverse;
            fractionFar = (signedDist - offset) * inverse;
            side = spread >= 0.0f;
        }
        if (fractionNear > 1.0f)
            fractionNear = 1.0f;
        for (i = 0; i < 3; i++)
            mid[i] = (p2[i] - p1[i]) * fractionNear + p1[i];
        hit = CM_SightTraceThroughLeafBrushNode_r(tw, node + node->data.children.childOffset[side], p1, mid);
        if (hit)
            return hit;
        if (fractionFar < 0.0f)
            fractionFar = 0.0f;
        for (i = 0; i < 3; i++)
            p1[i] = (p2[i] - p1[i]) * fractionFar + p1[i];
        node += node->data.children.childOffset[side ^ 1];
    }
    return 0;
}

static int CM_SightTraceThroughLeaf(const traceWork_t *tw, cLeaf_t *leaf, trace_t *trace)
{
    int i, hit;

    if (leaf->brushContents & tw->contents) {
        vec3_t mins, maxs;
        for (i = 0; i < 3; i++) {
            mins[i] = leaf->mins[i] - tw->size[i];
            maxs[i] = leaf->maxs[i] + tw->size[i];
        }
        if (!CM_TraceBox(&tw->extents, mins, maxs, 1.0f) &&
            leaf->leafBrushNode >= 0 && leaf->leafBrushNode < cm.leafbrushNodesCount) {
            hit = CM_SightTraceThroughLeafBrushNode_r(tw, &cm.leafbrushNodes[leaf->leafBrushNode],
                                                     tw->extents.start, tw->extents.end);
            if (hit)
                return hit;
        }
    }
    if (leaf->terrainContents & tw->contents) {
        for (i = 0; i < leaf->collAabbCount; i++) {
            int index = leaf->firstCollAabbIndex + i;
            if (index >= 0 && index < cm.aabbTreeCount)
                CM_SightTraceThroughAabbTree(tw, &cm.aabbTrees[index], trace);
            if (trace->fraction != 1.0f)
                return cm.numBrushes + index + 1;
        }
    }
    return 0;
}

static int CM_SightTraceThroughTree(const traceWork_t *tw, int num, const vec_t *p1_in,
                                    const vec_t *p2, trace_t *trace)
{
    vec3_t p1, mid;
    int i, side, hit;

    CM_CopyVec3(p1_in, p1);
    while (num >= 0) {
        cNode_t *node = &cm.nodes[num];
        const cplane_t *plane = node->plane;
        float d1, d2, offset, spread, fractionNear, fractionFar;

        if (plane->type < 3) {
            d1 = p1[plane->type] - plane->dist;
            d2 = p2[plane->type] - plane->dist;
            offset = tw->size[plane->type] + 0.125f;
        } else {
            d1 = CM_DotProduct(p1, plane->normal) - plane->dist;
            d2 = CM_DotProduct(p2, plane->normal) - plane->dist;
            offset = tw->isPoint ? 0.125f : 2048.0f;
        }
        if (CM_MinFloat(d1, d2) >= offset) {
            num = node->children[0];
            continue;
        }
        if ((d1 > d2 ? d1 : d2) <= -offset) {
            num = node->children[1];
            continue;
        }
        spread = d2 - d1;
        if (CM_AbsFloat(spread) <= CM_PARALLEL_EP) {
            fractionNear = 1.0f;
            fractionFar = 0.0f;
            side = 0;
        } else {
            float signedDist = spread >= 0.0f ? -d1 : d1;
            float inverse = 1.0f / CM_AbsFloat(spread);
            fractionNear = (signedDist + offset) * inverse;
            fractionFar = (signedDist - offset) * inverse;
            side = spread >= 0.0f;
        }
        if (fractionNear > 1.0f)
            fractionNear = 1.0f;
        for (i = 0; i < 3; i++)
            mid[i] = (p2[i] - p1[i]) * fractionNear + p1[i];
        hit = CM_SightTraceThroughTree(tw, node->children[side], p1, mid, trace);
        if (hit)
            return hit;
        if (fractionFar < 0.0f)
            fractionFar = 0.0f;
        for (i = 0; i < 3; i++)
            p1[i] = (p2[i] - p1[i]) * fractionFar + p1[i];
        num = node->children[side ^ 1];
    }
    i = -1 - num;
    if (i >= 0 && i < cm.numLeafs)
        return CM_SightTraceThroughLeaf(tw, &cm.leafs[i], trace);
    return 0;
}

static qboolean CM_SightTraceSphereThroughSphere(const traceWork_t *tw, const vec_t *start,
                                                 const vec_t *end, const vec_t *center, float radius, trace_t *trace)
{
    vec3_t separation, normal;
    float c, projection, discriminant, length, fraction;
    int i;

    (void)end;
    for (i = 0; i < 3; i++)
        separation[i] = start[i] - center[i];
    radius += tw->radius;
    c = CM_DotProduct(separation, separation) - radius * radius;
    if (c <= 0.0f)
        return 0;
    projection = CM_DotProduct(separation, tw->delta);
    if (projection >= 0.0f)
        return 1;
    discriminant = projection * projection - tw->deltaLenSq * c;
    if (discriminant < 0.0f)
        return 1;
    length = Vec3NormalizeTo(separation, normal);
    /* Sight uses projection/length, unlike the swept trace's length/projection
     * contact bias (Mac 0x63ef1..0x63efe). */
    fraction = (-projection - sqrtf(discriminant)) / tw->deltaLenSq + (projection * 0.125f) / length;
    return fraction >= trace->fraction;
}

static int CM_SightTraceTempCapsule(const traceWork_t *tw, const cmodel_t *model, trace_t *trace)
{
    vec3_t center, top, bottom, startTop, startBottom, endTop, endBottom, separation, normal;
    float radius, offsetZ, totalRadius, totalOffset, c, projection, discriminant, length, epsilon, root;
    int i;

    if (!(CM_BoxBrush()->contents & tw->contents))
        return 0;
    for (i = 0; i < 3; i++) {
        if (tw->bounds[0][i] > model->maxs[i] + 1.0f || tw->bounds[1][i] < model->mins[i] - 1.0f)
            return 0;
    }
    CM_TempCapsuleGeometry(model, center, &radius, &offsetZ);
    CM_CopyVec3(center, top);
    CM_CopyVec3(center, bottom);
    top[2] += offsetZ;
    bottom[2] -= offsetZ;
    CM_CopyVec3(tw->extents.start, startTop);
    CM_CopyVec3(tw->extents.start, startBottom);
    CM_CopyVec3(tw->extents.end, endTop);
    CM_CopyVec3(tw->extents.end, endBottom);
    startTop[2] += tw->offsetZ;
    startBottom[2] -= tw->offsetZ;
    endTop[2] += tw->offsetZ;
    endBottom[2] -= tw->offsetZ;
    if (startBottom[2] > top[2]) {
        if (!CM_SightTraceSphereThroughSphere(tw, startBottom, endBottom, top, radius, trace))
            return -1;
        if (tw->delta[2] >= 0.0f)
            return 0;
    } else if (startTop[2] < bottom[2]) {
        if (!CM_SightTraceSphereThroughSphere(tw, startTop, endTop, bottom, radius, trace))
            return -1;
        if (tw->delta[2] <= 0.0f)
            return 0;
    }
    totalRadius = radius + tw->radius;
    totalOffset = tw->size[2] - tw->radius + offsetZ;
    for (i = 0; i < 3; i++)
        separation[i] = tw->extents.start[i] - center[i];
    c = separation[0] * separation[0] + separation[1] * separation[1] - totalRadius * totalRadius;
    if (c <= 0.0f) {
        if (CM_AbsFloat(separation[2]) <= totalOffset)
            return -1;
    } else {
        projection = separation[0] * tw->delta[0] + separation[1] * tw->delta[1];
        if (projection < 0.0f) {
            /* The sight cylinder deliberately uses the full delta length
             * here, including Z (Mac 0x65cb6..0x65cd1). */
            discriminant = projection * projection - tw->deltaLenSq * c;
            if (discriminant >= 0.0f) {
                separation[2] = 0.0f;
                length = Vec3NormalizeTo(separation, normal);
                epsilon = (projection * 0.125f) / length;
                root = (-projection - sqrtf(discriminant)) / tw->deltaLenSq + epsilon;
                if (root < trace->fraction &&
                    CM_AbsFloat(tw->extents.start[2] + (root - epsilon) * tw->delta[2] - center[2]) <= totalOffset)
                    return -1;
            }
        }
    }
    if (endBottom[2] > top[2] && startBottom[2] > top[2])
        return 0;
    if (endBottom[2] > top[2])
        return CM_SightTraceSphereThroughSphere(tw, startBottom, endBottom, top, radius, trace) ? 0 : -1;
    if (endTop[2] >= bottom[2])
        return 0;
    if (startTop[2] < bottom[2])
        return 0;
    return CM_SightTraceSphereThroughSphere(tw, startTop, endTop, bottom, radius, trace) ? 0 : -1;
}
#else
/* 0x0041E070 ray-vs-sphere sight test: 1 = clears, 0 = blocked. */
static int CM_SightRaySphere(const traceWork_t *tw, const vec_t *a, const vec_t *b,
                             float rbias, trace_t *tr)
{
    vec3_t d, dn;
    float R, c, proj, disc, dls, len;

    d[0] = a[0] - b[0];
    d[1] = a[1] - b[1];
    d[2] = a[2] - b[2];
    R = rbias + tw->radius;
    c = (d[2] * d[2] + d[1] * d[1] + d[0] * d[0]) - R * R;
    if (c <= 0.0f)
        return 0;
    proj = d[2] * tw->delta[2] + d[1] * tw->delta[1] + d[0] * tw->delta[0];
    if (proj >= 0.0f)
        return 1;
    dls = tw->deltaLenSq;
    disc = proj * proj - dls * c;
    if (disc < 0.0f)
        return 1;
    len = Vec3NormalizeTo(d, dn);
    if ((proj * 0.125f) / len + (-proj - sqrtf(disc)) / dls < tr->fraction)
        return 0;
    return 1;
}

/* 0x0041E180 ray-vs-capsule (cylinder wall) sight test: 1 = clears, 0 = blocked. */
static int CM_SightRayCapsule(const traceWork_t *tw, const vec_t *end,
                              float rbias, trace_t *tr)
{
    vec3_t d, dn;
    float R, c, proj, disc, dls, len, root, axial, halfLen;

    d[0] = tw->extents.start[0] - end[0];
    d[1] = tw->extents.start[1] - end[1];
    d[2] = tw->extents.start[2] - end[2];
    R = rbias + tw->radius;
    c = (d[1] * d[1] + d[0] * d[0]) - R * R;
    root = 0.0f;
    if (c > 0.0f) {
        proj = d[1] * tw->delta[1] + d[0] * tw->delta[0];
        if (!(proj < 0.0f))
            return 1;
        dls = tw->deltaLenSq;
        disc = proj * proj - dls * c;
        if (!(disc >= 0.0f))
            return 1;
        len = Vec3NormalizeTo(d, dn);
        root = (proj * 0.125f) / len + (-proj - sqrtf(disc)) / dls;
        if (!(root < tr->fraction))
            return 1;
    }
    axial = root * tw->delta[2] + tw->extents.start[2] - end[2];
    if (axial < 0.0f)
        axial = -axial;
    halfLen = tw->size[2] - tw->radius + rbias;
    if (!(axial <= halfLen))
        return 1;
    return 0;
}

/* 0x0041E2C0 swept-box vs rounded brush sight test: -1 = blocked, 0 = clears.
 * AABB cull is exact; corner/edge sweep is being reconstructed against the carved bytes. */
static int CM_SightTraceThroughBrush(const traceWork_t *tw, cbrush_t *brush, trace_t *tr)
{
    if (brush->maxs[0] + 1.0f < tw->bounds[0][0])
        return 0;
    if (brush->maxs[1] + 1.0f < tw->bounds[0][1])
        return 0;
    if (brush->maxs[2] + 1.0f < tw->bounds[0][2])
        return 0;
    if (brush->mins[0] - 1.0f > tw->bounds[1][0])
        return 0;
    if (brush->mins[1] - 1.0f > tw->bounds[1][1])
        return 0;
    if (brush->mins[2] - 1.0f > tw->bounds[1][2])
        return 0;

    {
        vec3_t corner;
        corner[0] = brush->mins[0];
        corner[1] = brush->mins[1];
        corner[2] = brush->mins[2];
        if (!CM_SightRaySphere(tw, corner, tw->extents.start, 0.0f, tr))
            return -1;
        if (!CM_SightRayCapsule(tw, corner, 0.0f, tr))
            return -1;
    }
    return 0;
}
#endif

int CM_BoxSightTrace(int oldHitNum, const vec_t *start, const vec_t *end,
                     const vec_t *mins, const vec_t *maxs, clipHandle_t model, int brushmask)
{
    traceWork_t tw;
    trace_t trace;

#if defined(COD2_X64)
    cmodel_t *cmodel;
    float volume;
    int hit;

    CM_InitTraceWork(&tw, start, end, mins, maxs, brushmask);
    volume = (tw.bounds[1][2] - tw.bounds[0][2]) *
             ((tw.bounds[1][1] - tw.bounds[0][1]) * (tw.bounds[1][0] - tw.bounds[0][0]));
    tw.axialCullOnly = tw.size[0] * tw.size[1] * tw.size[2] * 16.0f * tw.deltaLen > volume;
    CM_ClearTraceResult(&trace);
    if (model == CM_TEMP_BOX_MODEL)
        return CM_SightTraceTempCapsule(&tw, CM_ModelForHandle(model), &trace);
    if (model != 0) {
        cmodel = CM_ModelForHandle(model);
        return cmodel ? CM_SightTraceThroughLeaf(&tw, &cmodel->leaf, &trace) : 0;
    }
    if (oldHitNum > 0 && oldHitNum <= cm.numBrushes) {
        hit = CM_SightTraceThroughBrush(&tw, &cm.brushes[oldHitNum - 1]);
        if (hit)
            return hit;
    }
    return CM_SightTraceThroughTree(&tw, 0, tw.extents.start, tw.extents.end, &trace);
#else
    (void)oldHitNum;
    CM_InitTraceWork(&tw, start, end, mins, maxs, brushmask);
    trace.fraction = 1.0f;
    trace.startsolid = 0;
    trace.allsolid = 0;

    if (model == CM_TEMP_BOX_MODEL) {
        return CM_SightTraceThroughBrush(&tw, CM_BoxBrush(), &trace);
    }

    if (model != 0) {
        cmodel_t *cmodel = CM_ModelForHandle(model);
        if (cmodel)
            CM_TraceLeaf(&tw, &cmodel->leaf, &trace);
        return trace.fraction < 1.0f || trace.startsolid || trace.allsolid;
    }

    {
        int leafs[1024];
        int lastLeaf = 0;
        int leafCount;
        int i;

        leafCount = CM_BoxLeafnums(tw.bounds[0], tw.bounds[1], leafs, 1024, &lastLeaf);
        for (i = 0; i < leafCount; i++) {
            int leafIndex = leafs[i];
            if (leafIndex >= 0 && leafIndex < cm.numLeafs) {
                CM_TraceLeaf(&tw, &cm.leafs[leafIndex], &trace);
                if (trace.fraction < 1.0f || trace.startsolid || trace.allsolid)
                    return 1;
            }
        }
    }

    return 0;
#endif
}

extern void AngleVectors(const vec_t *angles, vec_t *forward, vec_t *right, vec_t *up);

int CM_TransformedBoxTrace(trace_t *results, const vec_t *start, const vec_t *end,
                           const vec_t *mins, const vec_t *maxs, clipHandle_t model,
                           int brushmask, const vec_t *origin, const vec_t *angles)
{
    vec3_t sizeNeg;
    vec3_t sizePos;
    vec3_t localStart;
    vec3_t localEnd;
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    float oldFraction;
    int rotate;
    int i;
    int traceResult;

    for (i = 0; i < 3; i++) {
        float offset = (mins[i] + maxs[i]) * 0.5f;
        sizeNeg[i] = mins[i] - offset;
        sizePos[i] = maxs[i] - offset;
        localStart[i] = start[i] + offset;
        localEnd[i] = end[i] + offset;
    }

    localStart[0] = localStart[0] - origin[0];
    localStart[1] = localStart[1] - origin[1];
    localStart[2] = localStart[2] - origin[2];
    localEnd[0] = localEnd[0] - origin[0];
    localEnd[1] = localEnd[1] - origin[1];
    localEnd[2] = localEnd[2] - origin[2];

    rotate = (angles[0] != 0.0f || angles[1] != 0.0f || angles[2] != 0.0f);
    if (rotate) {
        vec3_t ls;
        vec3_t le;

        AngleVectors(angles, forward, right, up);
        right[0] = -right[0];
        right[1] = -right[1];
        right[2] = -right[2];

        ls[0] = localStart[0];
        ls[1] = localStart[1];
        ls[2] = localStart[2];
        localStart[0] = ls[0] * forward[0] + ls[1] * forward[1] + ls[2] * forward[2];
        localStart[1] = ls[0] * right[0] + ls[1] * right[1] + ls[2] * right[2];
        localStart[2] = ls[0] * up[0] + ls[1] * up[1] + ls[2] * up[2];

        le[0] = localEnd[0];
        le[1] = localEnd[1];
        le[2] = localEnd[2];
        localEnd[0] = le[0] * forward[0] + le[1] * forward[1] + le[2] * forward[2];
        localEnd[1] = le[0] * right[0] + le[1] * right[1] + le[2] * right[2];
        localEnd[2] = le[0] * up[0] + le[1] * up[1] + le[2] * up[2];
    }

    oldFraction = results->fraction;
    traceResult = CM_Trace(results, localStart, localEnd, sizeNeg, sizePos, model, brushmask);

    if (rotate && results->fraction < oldFraction) {
        vec3_t n;
        n[0] = results->normal[0];
        n[1] = results->normal[1];
        n[2] = results->normal[2];
        results->normal[0] = n[0] * forward[0] + n[1] * right[0] + n[2] * up[0];
        results->normal[1] = n[0] * forward[1] + n[1] * right[1] + n[2] * up[1];
        results->normal[2] = n[0] * forward[2] + n[1] * right[2] + n[2] * up[2];
    }

    return traceResult;
}

int CM_TransformedBoxTraceExternal(trace_t *results, const vec_t *start, const vec_t *end,
                                   const vec_t *mins, const vec_t *maxs, clipHandle_t model,
                                   int brushmask, const vec_t *origin, const vec_t *angles)
{
    memset(results, 0, sizeof(*results));
    results->fraction = 1.0f;
    return CM_TransformedBoxTrace(results, start, end, mins, maxs, model, brushmask, origin, angles);
}

int CM_TransformedBoxSightTrace(int hitNum, const vec_t *start, const vec_t *end,
                                const vec_t *mins, const vec_t *maxs, clipHandle_t model,
                                int brushmask, const vec_t *origin, const vec_t *angles)
{
    vec3_t sizeNeg;
    vec3_t sizePos;
    vec3_t localStart;
    vec3_t localEnd;
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    int i;

    for (i = 0; i < 3; i++) {
        float offset = (mins[i] + maxs[i]) * 0.5f;
        sizeNeg[i] = mins[i] - offset;
        sizePos[i] = maxs[i] - offset;
        localStart[i] = start[i] + offset;
        localEnd[i] = end[i] + offset;
    }

    localStart[0] = localStart[0] - origin[0];
    localStart[1] = localStart[1] - origin[1];
    localStart[2] = localStart[2] - origin[2];
    localEnd[0] = localEnd[0] - origin[0];
    localEnd[1] = localEnd[1] - origin[1];
    localEnd[2] = localEnd[2] - origin[2];

    if (angles[0] != 0.0f || angles[1] != 0.0f || angles[2] != 0.0f) {
        vec3_t ls;
        vec3_t le;

        AngleVectors(angles, forward, right, up);
        right[0] = -right[0];
        right[1] = -right[1];
        right[2] = -right[2];

        ls[0] = localStart[0];
        ls[1] = localStart[1];
        ls[2] = localStart[2];
        localStart[0] = ls[0] * forward[0] + ls[1] * forward[1] + ls[2] * forward[2];
        localStart[1] = ls[0] * right[0] + ls[1] * right[1] + ls[2] * right[2];
        localStart[2] = ls[0] * up[0] + ls[1] * up[1] + ls[2] * up[2];

        le[0] = localEnd[0];
        le[1] = localEnd[1];
        le[2] = localEnd[2];
        localEnd[0] = le[0] * forward[0] + le[1] * forward[1] + le[2] * forward[2];
        localEnd[1] = le[0] * right[0] + le[1] * right[1] + le[2] * right[2];
        localEnd[2] = le[0] * up[0] + le[1] * up[1] + le[2] * up[2];
    }

    return CM_BoxSightTrace(hitNum, localStart, localEnd, sizeNeg, sizePos, model, brushmask);
}
