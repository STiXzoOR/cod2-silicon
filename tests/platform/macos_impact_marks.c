/* Exercise actual mark allocation/copy with the renderer's full buffer contract. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

cg_t cgArray[1];
MarkPoly cg_markPolys[1024];
MarkPoly *cg_freeMarkPolys;
MarkVertAssemblyBuffer markVerts;
static dvar_t enabled, limit;
const dvar_t *cg_marks = &enabled, *cg_marksLimit = &limit;
void *imp_theFxHelper;
static Material material;
static int axisOnly, fragmentCalls, polyCalls;

extern void CG_InitMarkPolys(void);
extern void CG_AddMarks(void);
extern void CG_ImpactMark(MaterialHandle, const vec_t *, const vec_t *, float, const vec_t *, float);

Bool FxHelper_CullSphere(const FxHelper *helper, const vec_t *origin, float radius, int planes)
{ (void)helper; (void)origin; (void)radius; (void)planes; return 0; }
void Com_Error(errorParm_t code, const char *format, ...)
{ (void)code; (void)format; abort(); }

int CL_MarkFragments(const vec3_t *points, const vec_t *origin, const vec3_t *axis, float radius,
                     int maxPoints, GfxWorldVertex *verts, int maxFragments,
                     GfxMarkFragment *fragments, MaterialHandle markMaterial)
{
    (void)origin;
    assert(maxPoints == 1024 && maxFragments == 384 && radius == 3);
    assert(markMaterial == &material);
    ++fragmentCalls;
    if (axisOnly) {
        for (int i = 0; i < 3; ++i) {
            float length = 0;
            for (int j = 0; j < 3; ++j) length += axis[i][j] * axis[i][j];
            assert(fabsf(length - 1) < .00001f);
            for (int k = 0; k < i; ++k) {
                float dot = 0;
                for (int j = 0; j < 3; ++j) dot += axis[i][j] * axis[k][j];
                assert(fabsf(dot) < .00001f);
            }
        }
        /* Mac 1.3 order is normal, cross, right; corners follow those axes. */
        for (int j = 0; j < 3; ++j) {
            assert(fabsf(points[0][j] - (origin[j] - radius * axis[1][j] - radius * axis[2][j])) < .00001f);
            assert(fabsf(points[2][j] - (origin[j] + radius * axis[1][j] + radius * axis[2][j])) < .00001f);
        }
        return 0;
    }
    /* A successful renderer is allowed to fill every advertised vertex. */
    for (int i = 0; i < maxPoints; ++i) {
        memset(&verts[i], 0, sizeof(verts[i]));
        verts[i].xyz[0] = i;
        verts[i].normal[0] = -.6114383936f;
        verts[i].normal[1] = .7078721523f;
    }
    fragments[0] = (GfxMarkFragment){.markMaterial = &material, .lmapIndex = 7, .pointCount = 9, .firstPoint = 1015};
    return 1;
}

void CL_AddPolyToScene(MaterialHandle m, unsigned short lmap, unsigned short count, const GfxWorldVertex *verts)
{
    assert(m == &material && lmap == 7 && count == 9);
    for (int i = 0; i < count; ++i) {
        assert(verts[i].xyz[0] == 1015 + i);
        assert(verts[i].color.array[0] == 255 && verts[i].color.array[1] == 128);
    }
    ++polyCalls;
}

int main(int argc, char **argv)
{
    (void)argv;
    axisOnly = argc > 1;
    enabled.current.enabled = 1;
    limit.current.integer = 1024;
    CG_InitMarkPolys();
    const vec3_t origin = {17,29,41}, normal = {0,0,1};
    const vec4_t color = {.5f,.25f,.75f,1};
    int repeats = axisOnly ? 1 : 1100;
    for (int i = 0; i < repeats; ++i) {
        cgArray[0].clientFrame = i;
        CG_ImpactMark(&material, origin, normal, 37, color, 3);
    }
    assert(fragmentCalls == repeats);
    if (!axisOnly) {
        FxHelper helper = {0}, *ptr = &helper;
        imp_theFxHelper = &ptr;
        CG_AddMarks();
        assert(polyCalls == 1024 && cg_freeMarkPolys == NULL);
        MarkPoly *sentinel = &cgArray[0].activeMarkPolys;
        int seen = 0;
        for (MarkPoly *p = sentinel->nextMark; p != sentinel; p = p->nextMark) {
            assert(p->nextMark->prevMark == p && p->prevMark->nextMark == p);
            assert(++seen <= 1024);
        }
        assert(seen == 1024);
    }
    puts(axisOnly ? "PASS: impact axes are contiguous and match original corner order" :
                    "PASS: 1024-vertex impact storage, full mark pool recycling and scene submission");
    return 0;
}
