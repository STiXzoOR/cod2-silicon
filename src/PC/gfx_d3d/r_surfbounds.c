#include "common_types.h"
#include "imports.h"
extern GfxScene scene;

extern void GetRotatedBounds(vec3_t *baseBounds, const vec_t *origin, vec3_t *axis, vec3_t *rotatedBounds);

/* R_BoundsForDrawSurfTable: migrated from the ILP32 data blob to typed C. The
 * blob laid the 4 live fn-pointers at byte offset 12 (NULL entries [0..2],[7]
 * were byte-truncated), which is not an 8-byte entry boundary on x64 -- as a
 * proper [8] array each entry is at i*sizeof(ptr) on both ABIs. Indexed by
 * surfType. (x64 port Stage 2.) */
const vec_t *R_BoundsForSurf_ModelInst(const GfxDrawSurf *drawSurf, int entIndex);
const vec_t *R_BoundsForSurf_StaticModelCached(const GfxDrawSurf *drawSurf, int entIndex);
const vec_t *R_BoundsForSurf_Triangles(const GfxDrawSurf *drawSurf, int entIndex);
const vec_t *(*R_BoundsForDrawSurfTable[8])() = {
    NULL, NULL, NULL,
    (const vec_t *(*)())R_BoundsForSurf_ModelInst,
    (const vec_t *(*)())R_BoundsForSurf_ModelInst,
    (const vec_t *(*)())R_BoundsForSurf_StaticModelCached,
    (const vec_t *(*)())R_BoundsForSurf_Triangles,
    NULL,
};
static vec3_t surfBoundsGlob[2];

extern r_global_permanent_t rgp;

const vec_t *R_BoundsForSurf_Triangles(const GfxDrawSurf *drawSurf, int entIndex)
{
    srfTriangles_t *tri = (srfTriangles_t *)drawSurf->surface;

    if (entIndex > 0x7fd)
        return (const vec_t *)tri->bounds;

    GfxEntity *entity = &scene.def.entities[entIndex];
    GetRotatedBounds( (vec3_t (*))((const vec_t *)tri->bounds), entity->origin, (vec3_t (*))((const vec_t *)entity->axis), (vec3_t (*))((vec_t *)surfBoundsGlob));
    return (const vec_t *)surfBoundsGlob;
}

const vec_t *R_BoundsForSurf_ModelInst(const GfxDrawSurf *drawSurf, int entIndex)
{
    return scene.sceneEnts[entIndex].curMins;
}

const vec_t *R_BoundsForSurf_StaticModelCached(const GfxDrawSurf *drawSurf, int entIndex)
{
    GfxStaticModelCachedSurface *cached = (GfxStaticModelCachedSurface *)drawSurf->surface;
    int smodelIndex = cached->surface->smodelIndex;
    return rgp.world->smodelInsts[smodelIndex].mins;
}

const vec_t *R_BoundsForDrawSurf(const GfxDrawSurf *surf)
{
    int surfType = *surf->surface;
    const vec_t *(*boundsFunc)(const GfxDrawSurf *, int) =
        (const vec_t *(*)(const GfxDrawSurf *, int))R_BoundsForDrawSurfTable[surfType];
    int entIndex;
    int sortVal;

    if (!boundsFunc)
        return 0;

    sortVal = *(int *)&surf->sort;
    if (sortVal >= 0) {
        entIndex = ((unsigned int)sortVal >> 4) & 0xfff;
    } else {
        entIndex = ((unsigned int)sortVal >> 19) & 0xfff;
    }
    if (entIndex >= 0x800)
        entIndex = 0x7fe;

    return boundsFunc(surf, entIndex);
}
