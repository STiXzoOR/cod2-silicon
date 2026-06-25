#include "common_types.h"
#include "imports.h"
extern GfxScene scene;

extern void GetRotatedBounds(const vec_t *surfBounds, const vec_t *entityOrigin, const vec_t *entityAxis, vec_t *out);

extern const vec_t *(*R_BoundsForDrawSurfTable[8])();
static vec3_t surfBoundsGlob[2];

extern r_global_permanent_t rgp;

const vec_t *R_BoundsForSurf_Triangles(const GfxDrawSurf *drawSurf, int entIndex)
{
    srfTriangles_t *tri = (srfTriangles_t *)drawSurf->surface;

    if (entIndex > 0x7fd)
        return (const vec_t *)tri->bounds;

    GfxEntity *entity = &scene.def.entities[entIndex];
    GetRotatedBounds((const vec_t *)tri->bounds, entity->origin, (const vec_t *)entity->axis, (vec_t *)surfBoundsGlob);
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
    const vec_t *(*boundsFunc)() = R_BoundsForDrawSurfTable[surfType];
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
