#define main dpvs_planes_fixture_main
#include "dpvs_planes.c"
#undef main
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>

static dvar_t bevel = { .current.value = 0.7f };
static dvar_t clipArea = { .current.value = 0.02f };
const dvar_t *r_showPortals = &disabledDvar;
const dvar_t *r_portalWalkLimit = &disabledDvar;
const dvar_t *r_portalBevelsOnly = &disabledDvar;
const dvar_t *r_portalBevels = &bevel;
const dvar_t *r_portalMinClipArea = &clipArea;
void *imp_colorMagenta, *imp_colorLtYellow;
static void *allocation;
void LargeLocal_LargeLocal(const LargeLocal *local, int size)
{
    (void)local;
    assert(size == 0x20000);
    allocation = calloc(1, (size_t)size);
    assert(allocation);
}
void *LargeLocal_GetBuf(const LargeLocal *local) { (void)local; return allocation; }
void ZN10LargeLocalD1Ev(LargeLocal *local) { (void)local; free(allocation); }
void R_Error(int level, const char *fmt, ...)
{ (void)level; (void)fmt; abort(); }
void R_AddDebugBox(DebugGlobals *globals, const vec_t *mins, const vec_t *maxs, const vec_t *color)
{ (void)globals;(void)mins;(void)maxs;(void)color; abort(); }
void R_AddDebugPolygon(DebugGlobals *globals, const float *color, int count, vec3_t *verts)
{ (void)globals;(void)color;(void)count;(void)verts; abort(); }
void R_AddDebugLine(DebugGlobals *globals, const vec_t *a, const vec_t *b, const vec_t *color)
{ (void)globals;(void)a;(void)b;(void)color; abort(); }
void R_UpdateXModelBounds(GfxSceneEntity *ent, GfxEntity *gfx) { (void)ent;(void)gfx; abort(); }
void R_SkinSceneEnt(GfxSceneEntity *ent, GfxEntity *gfx) { (void)ent;(void)gfx; abort(); }
void R_AddBModelSurfaces(GfxSceneEntity *ent, int index) { (void)ent;(void)index; abort(); }
void CG_CullIn(const centity_t *cent) { (void)cent; abort(); }
void CG_UsedDObjCalcPose(const centity_t *cent) { (void)cent; abort(); }
qboolean WindingContainsCoplanarPoint(vec3_t *verts, int count, const vec_t *normal, const vec_t *point)
{ (void)verts; (void)count; (void)normal; (void)point; return 0; }
void Vec3Cross(const vec_t *a, const vec_t *b, vec_t *out)
{
    out[0] = a[1]*b[2]-a[2]*b[1];
    out[1] = a[2]*b[0]-a[0]*b[2];
    out[2] = a[0]*b[1]-a[1]*b[0];
}
const vec_t Vec3Normalize(vec_t *v)
{
    float len = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (len) for(int i=0;i<3;i++) v[i] /= len;
    return len;
}
int Com_ConvexHull(vec2_t *points, int count, vec2_t *out)
{
    assert(count == 4);
    memcpy(out, points, (size_t)count*sizeof(vec2_t));
    return count;
}

int main(void)
{
    GfxCell from = {0}, to = {0};
    GfxPortal portal = {0};
    vec3_t verts[4] = {{1,-1,-1},{1,1,-1},{1,1,1},{1,-1,1}};
    D3DMATRIX matrix = {0};
    matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = matrix.m[3][3] = 1;
    portal.plane.coeffs[0] = 1;
    portal.plane.coeffs[3] = -1;
    portal.hullAxis[0][1] = portal.hullAxis[1][2] = 1;
    portal.vertices = verts;
    portal.vertexCount = 4;
    portal.cell = &to;
    dpvsG.eyeW = 1;
    dpvsG.viewProjectionMatrix = &matrix;
    dpvsG.inverseViewProjectionMatrix = NULL;
    dpvsG.eyePlane.coeffs[0] = 1;
    DpvsPlane clip = { .coeffs = {1,0,0,0} };
    /* Empty traversal still initializes the entire exactly sized hull pool. */
    R_VisitPortals_impl(&from, &clip, NULL, 0);
    from.portalCount = 1;
    from.portals = &portal;
    R_VisitPortals_impl(&from, &clip, NULL, 0);
    assert(!portal.hullPoints && !dpvsG.portalQueueCount);

    DpvsPlane parent = { .coeffs = {0,1,0,0} };
    DpvsPlane frustum = { .coeffs = {0,0,1,0} };
    vec3_t scratch[256];
    const GfxCell *list[128];
    assert(R_GetFurtherCellList_r_impl(&from, &parent, &frustum, 1, scratch, list, 0) == 1);
    assert(list[0] == &to);
    /* Exercise the real caller's two alternating winding buffers. */
    R_VisitPortalsForCell_impl(&from, NULL, &parent, &frustum, 1, DPVS_DONT_CLIP_CHILDREN);
    DpvsPlane farPlane = { .coeffs = {0,0,1,-2} };
    dpvsG.farPlanePtr = &farPlane;
    assert(R_GetFurtherCellList_r_impl(&from, &parent, &frustum, 1, scratch, list, 0) == 0);
    dpvsG.farPlanePtr = NULL;
    puts("native portals: exact hull pool, forward projection, scratch buffers and far clip passed");
    return 0;
}
