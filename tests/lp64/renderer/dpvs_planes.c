#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "PC/gfx_d3d/r_dpvs.c"

_Alignas(struct DpvsGlobals) unsigned char dpvsGlob[224];
GfxBackEndData *frontEndDataOut;
refimport_t ri;
GfxScene scene;
r_globals_t rg;
r_global_permanent_t rgp;
void *imp_colorWhite;
static dvar_t enabledDvar = { .current.enabled = 1 };
static dvar_t disabledDvar;
const dvar_t *r_portalFineCull = &enabledDvar;
const dvar_t *r_showSModelNames = &disabledDvar;
static int submitted;

const vec_t Vec3Distance(const vec_t *a, const vec_t *b)
{ (void)a; (void)b; assert(0); return 0; }
const char *XModelGetName(const XModel *model)
{ (void)model; assert(0); return NULL; }
void R_AddDebugString(DebugGlobals *globals, const vec_t *origin, const vec_t *color, float scale, const char *text)
{ (void)globals; (void)origin; (void)color; (void)scale; (void)text; assert(0); }
int R_AddStaticModelToScene(int index)
{ (void)index; assert(0); return -1; }
void R_SkinStaticModel(GfxSceneEntity *sceneEnt, GfxEntity *ent, int index)
{ (void)sceneEnt; (void)ent; (void)index; assert(0); }
void R_AddXModelSurfaces(int index)
{ (void)index; assert(0); }
void R_AddDrawSurfForSurface(GfxSurface *surface, int index)
{ assert(surface == &rgp.world->surfaces[0] && index == 0x800); ++submitted; }

static DpvsPlane Plane(byte cachedLevel)
{
    DpvsPlane plane = { .coeffs = { 1, 0, 0, 0 }, .side = { 12, 16, 20 } };
    plane.u.frontal = cachedLevel;
    return plane;
}

static void TestPlaneCaches(void)
{
    const float outside[6] = { -3, -1, -1, -2, 1, 1 };
    const float inside[6] = { 2, -1, -1, 3, 1, 1 };
    DpvsPlane plane = Plane(0xff);
    assert(!R_CullByFrustumPlanes(&plane, 1, 0, outside));
    plane = Plane(0xff);
    assert(R_CullByFrustumPlanes(&plane, 1, 0, inside));
    plane = Plane(0);
    assert(R_CullByFrustumPlanes(&plane, 1, 1, outside) && plane.u.frontal == 0);
    assert(!R_CullByFrustumPlanes(&plane, 1, 0, outside) && plane.u.frontal == 0xff);

    DpvsPlane occPlanes[2] = { Plane(0xff), Plane(0xff) };
    GfxOccluder occluder = { .ignoreStackLevel = 10, .viewPlaneCount = 1, .viewPlanes = occPlanes };
    GfxOccluder *occluders[] = { &occluder };
    dpvsG.occluderCount = 1;
    dpvsG.occluderList = occluders;
    assert(!R_CullByOccluders(0, outside));
    assert(R_CullByOccluders(0, inside));
    occPlanes[0] = Plane(0);
    assert(!R_CullByOccluders(1, inside) && occPlanes[0].u.frontal == 0);
    occluder.viewPlaneCount = 2;
    assert(R_CullByOccluders(1, inside) && occPlanes[0].u.frontal == 0);
    assert(!R_CullByOccluders(1, outside) && occPlanes[0].u.frontal == 0);
    occluder.ignoreStackLevel = 0;
    assert(R_CullByOccluders(1, outside));
    dpvsG.occluderCount = 0;
    dpvsG.occluderList = NULL;
}

static void TestAabbTraversal(void)
{
    GfxWorld world = { 0 };
    GfxSurface surface = { 0 };
    int surfaceViewCount = 0;
    world.surfaces = &surface;
    rgp.world = &world;
    rg.surfaces = (void *)&surfaceViewCount;
    scene.viewCount = 1;
    GfxAabbTree tree = { .mins = { 2, -1, -1 }, .maxs = { 3, 1, 1 }, .surfaceCount = 1 };
    DpvsPlane plane = Plane(0xff);
    submitted = 0;
    R_AddAabbTreeSurfaces_r_impl(&tree, &plane, 1, 0);
    assert(submitted == 1 && plane.u.frontal == 0);

    /* Ancestor caches remain attached to their original level in both loops. */
    surfaceViewCount = 0;
    plane = Plane(0);
    R_AddAabbTreeSurfaces_r_impl(&tree, &plane, 1, 1);
    assert(submitted == 2 && plane.u.frontal == 0);
    DpvsPlane occPlane = Plane(0);
    GfxOccluder occluder = { .ignoreStackLevel = 10, .viewPlaneCount = 1, .viewPlanes = &occPlane };
    GfxOccluder *occluders[] = { &occluder };
    dpvsG.occluderCount = 1;
    dpvsG.occluderList = occluders;
    surfaceViewCount = 0;
    R_AddAabbTreeSurfaces_r_impl(&tree, &plane, 1, 1);
    assert(submitted == 2 && occPlane.u.frontal == 0);

    dpvsG.occluderCount = 0;
    dpvsG.occluderList = NULL;
    tree.mins[0] = -3;
    tree.maxs[0] = -2;
    plane = Plane(0xff);
    R_AddAabbTreeSurfaces_r_impl(&tree, &plane, 1, 0);
    assert(submitted == 2 && surfaceViewCount == 0);
}

int main(void)
{
    /* Mac 1.3: 0xef0ab/0xef14c/0xef2bb/0xef358 skip only stackLevel > frontal. */
    TestPlaneCaches();
    TestAabbTraversal();
    puts("native DPVS planes: uncached bounds, ancestor caches and AABB traversal passed");
}
