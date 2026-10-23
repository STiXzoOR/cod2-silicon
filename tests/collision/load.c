#include "cm_load_source.c"
#include "../fidelity/check.h"
#include <stdlib.h>

clipMap_t cm;
static cLeafBrushNode_t arena[128];
static int used;
char *TempMalloc(int size)
{
    char *p = (char *)arena + used;
    used += size;
    CHECK(used <= sizeof(arena));
    return p;
}
void *CM_Hunk_AllocateTempMemoryHigh(int size, const char *name)
{
    return malloc(size);
}
void CM_Hunk_CheckTempMemoryHighClear(void)
{
}
void CM_Hunk_ClearTempMemoryHigh(void)
{
}
void Com_Error(errorParm_t code, const char *format, ...)
{
    abort();
}

int main(void)
{
    cbrush_t brushes[5] = {
        { .mins = { -8, -1, -1 }, .maxs = { -6, 1, 1 }, .contents = 1 },
        { .mins = { -4, -1, -1 }, .maxs = { -2, 1, 1 }, .contents = 2 },
        { .mins = { 2, -1, -1 }, .maxs = { 4, 1, 1 }, .contents = 4 },
        { .mins = { 6, -1, -1 }, .maxs = { 8, 1, 1 }, .contents = 8 },
        { .mins = { -9, -1, -1 }, .maxs = { 9, 1, 1 }, .contents = 16 },
    };
    unsigned short indices[5] = { 0, 1, 2, 3, 4 };
    const vec3_t mins = { -9, -1, -1 }, maxs = { 9, 1, 1 };
    cm.brushes = brushes;
    memset(arena, 0xa5, sizeof(arena));
    cLeafBrushNode_t *node = CMod_PartionLeafBrushes_r(indices, 5, mins, maxs);
    CHECK(node == arena);
    CHECK(node->leafBrushCount == -1 && node->contents == 31 && node->axis == 0);
    CHECK(node->data.children.dist == 0 && node->data.children.range == 2);
    CHECK(node[1].leafBrushCount == 1 && node[1].contents == 16);
    CHECK(node[node->data.children.childOffset[0]].contents == 12);
    CHECK(node[node->data.children.childOffset[1]].contents == 3);
    CHECK(used == 4 * sizeof(cLeafBrushNode_t));
    FINISH("collision loading");
}
