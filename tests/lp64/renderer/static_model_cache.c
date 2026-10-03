#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"
#include "PC/gfx_d3d/r_staticmodelcache.c"

_Static_assert(sizeof(static_model_leaf_t) == 24, "STABS leaf16: two widened pointers");
_Static_assert(offsetof(static_model_tree_t, leafs) == 144, "STABS leafs136: wider list");
_Static_assert(sizeof(static_model_tree_t) == 528, "STABS tree392: list and16 leaves");
_Static_assert(sizeof(MaterialPassDx7) == 112, "STABS Dx7 pass92: pointers and alignment");
_Static_assert(offsetof(MaterialPassDx7, samplers) == 16, "native sampler arguments");
_Static_assert(offsetof(MaterialTechnique, passArray) == 16, "STABS pass array8");

int main(void)
{
    static static_model_cache_t cache;
    XSurface xsurf = {0};
    GfxStaticSurface surface = {0};
    static_model_tree_t *tree = &cache.trees[0];
    static_model_node_list_t sentinel;
    static_model_node_list_t *freeNode = &tree->leafs[8].freenode;
    GfxStaticModelSurfaceCached *used = &tree->leafs[0].surf;
    xsurf.vertCount = 29;
    used->surface = &surface; used->xsurf = &xsurf;
    surface.cachedLods[2] = used;
    tree->nodes[0].usedVerts = 29;
    tree->nodes[1].usedVerts = 29; tree->nodes[1].inuse = 1;
    sentinel.prev = sentinel.next = (intptr_t)freeNode;
    freeNode->prev = freeNode->next = (intptr_t)&sentinel;
    cache.stats.allocatedVerts = 256; cache.stats.usedVerts = 29;
    SMC_FreeCachedSurface_r(&cache, tree, 0, 4);
    assert(cache.stats.allocatedVerts == 0 && cache.stats.usedVerts == 0);
    assert(!surface.cachedLods[2] && !tree->nodes[1].inuse);
    assert(sentinel.prev == (intptr_t)&sentinel && sentinel.next == (intptr_t)&sentinel);
    puts("renderer static-model cache recursion and native leaf strides: passed");
}
