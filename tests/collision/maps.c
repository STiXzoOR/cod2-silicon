/* Real-map differential driver. Proprietary BSPs and result streams stay local. */
#include "common_types.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern void CM_LoadMapFromBsp(const char *, int);
extern int CM_BoxTrace(trace_t *, const vec_t *, const vec_t *, const vec_t *, const vec_t *, clipHandle_t, int);
extern int CM_BoxSightTrace(int, const vec_t *, const vec_t *, const vec_t *, const vec_t *, clipHandle_t, int);
extern int CM_PointContents(const vec_t *, clipHandle_t);

clipMap_t cm;
void *imp_cm = &cm;
static TraceThreadInfo info;
static byte *bsp;
static byte scratch[32 * 1024 * 1024];
static size_t scratchUsed;
static unsigned int seed = 0xf1b13;

void *Sys_GetValue(int key)
{
    if (key != 3)
        abort();
    return &info;
}
void *CM_Hunk_Alloc(int size, const char *name, int type)
{
    void *p = calloc(1, size ? size : 1);
    if (!p)
        abort();
    return p;
}
void *CM_Hunk_AllocateTempMemoryHigh(int size, const char *name)
{
    return CM_Hunk_Alloc(size, name, 0);
}
void CM_Hunk_CheckTempMemoryClear(void)
{
}
void CM_Hunk_CheckTempMemoryHighClear(void)
{
}
void CM_Hunk_ClearTempMemory(void)
{
}
void CM_Hunk_ClearTempMemoryHigh(void)
{
}
void TempMemoryReset(void)
{
    scratchUsed = 0;
}
char *TempMalloc(int size)
{
    byte *p = scratch + scratchUsed;
    scratchUsed += size;
    if (scratchUsed > sizeof(scratch))
        abort();
    return (char *)p;
}
const dheader_t *Com_GetBsp(int *size, unsigned int *checksum)
{
    *checksum = 0;
    return (const dheader_t *)bsp;
}
void Com_Memset(void *p, const int value, int size)
{
    memset(p, value, size);
}
void Com_Memcpy(void *p, const void *s, int size)
{
    memcpy(p, s, size);
}
void Com_Error(errorParm_t code, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    exit(2);
}
static unsigned int Random(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
static float SignedRandom(float scale)
{
    return ((int)(Random() >> 8) - 8388608) * (scale / 8388608.0f);
}

/* Fixed-width output: compare every observable field, with material pointers
 * replaced by indices. No pointer bytes or padding enter the comparison. */
typedef struct Result {
    float fraction;
    float normal[3];
    int surfaceFlags, contents, material, solid, hit;
} Result;

int main(int argc, char **argv)
{
    if (argc != 4)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 2;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    bsp = malloc(length);
    if (fread(bsp, 1, length, file) != (size_t)length)
        return 2;
    fclose(file);
    CM_LoadMapFromBsp("private-map", 1);
    info.box_model = &cm.box_model;
    info.box_brush = cm.box_brush;
    info.checkcount.partitions = calloc(cm.partitionCount, sizeof(unsigned short));
    info.checkcount.edges = calloc(cm.edgeCount, sizeof(int));
    info.checkcount.verts = calloc(cm.vertCount, sizeof(int));
    file = fopen(argv[2], "wb");
    if (!file)
        return 2;
    int queries = atoi(argv[3]);
    Result *results = calloc(queries, sizeof(*results));
    struct timespec before, after;
    clock_gettime(CLOCK_MONOTONIC, &before);
    for (int q = 0; q < queries; ++q) {
        vec3_t start, end, mins, maxs;
        cbrush_t *brush = &cm.brushes[Random() % cm.numBrushes];
        for (int axis = 0; axis < 3; ++axis) {
            start[axis] = (brush->mins[axis] + brush->maxs[axis]) * 0.5f + SignedRandom(96);
            end[axis] = start[axis] + SignedRandom(256);
        }
        int kind = q % 6;
        float radius = kind == 0 || kind == 2 || kind == 4 ? 0 : 15;
        for (int axis = 0; axis < 3; ++axis) {
            maxs[axis] = axis == 2 && radius ? 36 : radius;
            mins[axis] = -maxs[axis];
            if (kind == 5)
                end[axis] = start[axis];
        }
        int mask = q % 4 ? 0x02810011 : -1;
        Result *result = &results[q];
        if (kind == 4) {
            result->contents = CM_PointContents(start, 0);
        } else if (kind == 2 || kind == 3) {
            result->hit = CM_BoxSightTrace(0, start, end, mins, maxs, 0, mask);
        } else {
            trace_t trace;
            CM_BoxTrace(&trace, start, end, mins, maxs, 0, mask);
            result->fraction = trace.fraction;
            memcpy(result->normal, trace.normal, sizeof(result->normal));
            result->surfaceFlags = trace.surfaceFlags;
            result->contents = trace.contents;
            result->material = trace.material ? (int)(((const dmaterial_t *)trace.material) - cm.materials) + 1 : 0;
            result->solid = trace.startsolid | (trace.allsolid << 1);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &after);
    double seconds = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) * 1e-9;
    if (fwrite(results, sizeof(*results), queries, file) != (size_t)queries)
        return 2;
    fclose(file);
    int split = 0, immediate = 0;
    for (int i = 1; i < cm.leafbrushNodesCount; ++i) {
        split += cm.leafbrushNodes[i].leafBrushCount <= 0;
        immediate += cm.leafbrushNodes[i].leafBrushCount < 0;
    }
    printf("{\"queries\":%d,\"seconds\":%.9f,\"brushes\":%d,\"brush_nodes\":%d,\"split_nodes\":%d,\"immediate_nodes\":%d}\n",
           queries, seconds, cm.numBrushes, cm.leafbrushNodesCount, split, immediate);
    return 0;
}
