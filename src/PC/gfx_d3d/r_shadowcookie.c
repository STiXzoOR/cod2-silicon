#include "common_types.h"
#include "imports.h"

static ShadowCookieGlob shadowCookieGlob;

void R_ResetShadowCookies(void)
{
    shadowCookieGlob.weightCap = 10.0f;
    shadowCookieGlob.lastTime = -1;
}

#if defined(COD2_X64)
#include <stdlib.h>

extern GfxScene scene;
extern GfxBackEndData *frontEndDataOut;
extern r_globals_t rg;
extern r_global_permanent_t rgp;
extern refimport_t ri;
extern const dvar_t *sc_blur, *sc_debugCasterCount, *sc_debugReceiverCount;
extern const dvar_t *sc_length, *sc_fadeRange, *sc_wantCount, *sc_wantCountMargin;
extern const dvar_t *sc_shadowInRate, *sc_shadowOutRate;
extern UINT32 g_NumBlurShadowPasses;
extern void CG_UsedDObjCalcPose(const centity_t *cent);
extern void CG_CullIn(const centity_t *cent);
extern void R_UpdateXModelBounds(GfxSceneEntity *sceneEnt, GfxEntity *ent);
extern void R_SkinSceneEnt(GfxSceneEntity *sceneEnt, GfxEntity *ent);
extern void R_AddXModelSurfaces(int entIndex);
extern GfxViewParms *R_AllocViewParms(void);
extern void Vec3Cross(const vec_t *a, const vec_t *b, vec_t *out);
extern vec_t Vec3Normalize(vec_t *v);
extern int Vec3MajorAxis(const vec_t *v);
extern vec_t Vec3Distance(const vec_t *a, const vec_t *b);
extern void MatrixMultiply44(const float (*a)[4], const float (*b)[4], float (*out)[4]);
extern void MatrixInverse44(const float *matrix, float *inverse);
extern void R_FrustumClipPlanes(const D3DMATRIX *matrix, vec4_t *sides, int count, DpvsPlane *planes);
extern void R_DecomposeSort(unsigned int sort, int *entIndex, const Material **material, int *lmapIndex);
extern void qsortDrawSurfs(GfxDrawSurf *surfs, int count);
extern GfxDrawGroupGlueBehavior R_BeginDrawGroupSection(GfxDrawGroupType group);
extern void R_BeginDrawGroupLoop(GfxDrawGroupType group, int viewIndex);
extern GfxDrawGroupGlueBehavior R_EndDrawGroupLoop(GfxDrawGroupType group, int viewIndex);
extern void R_EndDrawGroupSection(GfxDrawGroupType group);
extern void R_AddCmdShadowCookie(const D3DMATRIX *lookup, float fade);
extern void R_AddCmdBlurShadowCookie(void);
extern void R_AddCmdSetRenderTarget(GfxRenderTargetId target);
extern void R_AddCmdClearScreen(int flags, const vec_t *color, float depth, int stencil);
extern void R_AddCmdBeginView(int viewCount, const GfxSceneDef *sceneDef, const GfxViewParms *view, const GfxLodParms *lod);
extern void R_AddCmdSetViewport(int x, int y, int width, int height);
extern void R_AddCmdDrawSurfs(GfxDrawSurf *surfs, int count, MaterialTechniqueType technique);

void R_DrawShadowCookieOverlay(void)
{
    extern vidConfig_t vidConfig;
    extern void MatrixIdentity44(float (*matrix)[4]);
    extern void R_AddCmdDrawStretchPic(float x, float y, float w, float h,
        float s0, float t0, float s1, float t1, const vec_t *color, MaterialHandle material);
    D3DMATRIX identity;
    MatrixIdentity44(identity.m);
    R_AddCmdShadowCookie(&identity, 1.0f);
    float size = (float)vidConfig.width * 0.25f;
    R_AddCmdDrawStretchPic(4, 4, size, size, 0, 0, 1, 1,
        (const vec_t *)imp_colorWhite, rgp.shadowCookieOverlayMaterial);
}

/* Windows 1.3 0x101904a8; also Mac 1.3 i386 0x302740. */
static vec4_t shadowFrustumSidePlanes[5] = {
    { -1, 0, 0, 1 }, { 1, 0, 0, 1 },
    { 0, -1, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 0 }
};

/* Mac 1.3 R_AddShadowCookie 0xe08f0; Windows projection 0x10027680. */
static void R_AddShadowCookie(const GfxViewParms *viewParms, int receiverCount,
                              int entIndex, float fade, ShadowCookieList *list)
{
    GfxSceneEntity *sceneEnt = &scene.sceneEnts[entIndex];
    GfxEntity *ent = &scene.def.entities[entIndex];
    vec3_t extent, axis[3], mins, maxs;
    DpvsPlane planes[5];
    int i, j;

    CG_UsedDObjCalcPose(sceneEnt->cent);
    R_UpdateXModelBounds(sceneEnt, ent);
    for (i = 0; i < 3; ++i)
        extent[i] = sceneEnt->curMaxs[i] - sceneEnt->curMins[i];
    if (extent[0] * extent[0] + extent[1] * extent[1] + extent[2] * extent[2] == 0.0f)
        return;

    ShadowCookie *cookie = &list->cookies[list->cookieCount++];
    GfxViewParms *shadow = R_AllocViewParms();
    cookie->shadowViewParms = shadow;
    memset(shadow, 0, sizeof(*shadow));
    for (i = 0; i < 3; ++i)
        axis[0][i] = -rgp.world->sunLight.position[i];
    axis[2][0] = axis[0][0] * axis[0][0] + axis[0][1] * axis[0][1] < 0.1f ? 1.0f : 0.0f;
    axis[2][1] = 0.0f;
    axis[2][2] = 1.0f - axis[2][0];
    Vec3Cross(axis[2], axis[0], axis[1]);
    Vec3Normalize(axis[1]);
    Vec3Cross(axis[0], axis[1], axis[2]);
    for (i = 0; i < 3; ++i)
        axis[1][i] = -axis[1][i];

    for (i = 0; i < 8; ++i) {
        vec3_t corner;
        for (j = 0; j < 3; ++j)
            corner[j] = (i & (1 << j)) ? sceneEnt->curMaxs[j] : sceneEnt->curMins[j];
        for (j = 0; j < 3; ++j) {
            float value = (corner[0] * axis[j][0] + corner[1] * axis[j][1]) + corner[2] * axis[j][2];
            if (i == 0 || value < mins[j]) mins[j] = value;
            if (i == 0 || value > maxs[j]) maxs[j] = value;
        }
    }
    for (i = 0; i < 3; ++i) {
        int column = (i + 2) % 3;
        float scale = i == 0 ? 1.0f / sc_length->current.value :
            (2.0f / (maxs[i] - mins[i])) * 0.9375f;
        for (j = 0; j < 3; ++j)
            shadow->viewMatrix.m[j][column] = axis[i][j] * scale;
        shadow->viewMatrix.m[3][column] = i == 0 ?
            ((mins[i] + maxs[i]) * -0.5f) * scale :
            (scale * -0.5f) * (mins[i] + maxs[i]);
    }
    shadow->viewMatrix.m[3][3] = 1.0f;
    memcpy(shadow->axis, axis, sizeof(axis));
    /* Aspyr's OpenGL caster renders into a bottom-up texture. Windows puts
     * this sign in the lookup instead; the final receiver lookup is equal. */
    for (i = 0; i < 4; ++i)
        shadow->viewMatrix.m[i][1] = -shadow->viewMatrix.m[i][1];
    /* Original OrthographicMatrix(256/126, 256/126, 1). */
    shadow->projectionMatrix.m[0][0] = 2.0f / 2.0317461f;
    shadow->projectionMatrix.m[1][1] = 2.0f / 2.0317461f;
    shadow->projectionMatrix.m[2][2] = 0.5f;
    shadow->projectionMatrix.m[3][2] = 0.5f;
    shadow->projectionMatrix.m[3][3] = 1.0f;
    shadow->depthHackNearClip = -shadow->projectionMatrix.m[3][2];
    MatrixMultiply44(shadow->viewMatrix.m, shadow->projectionMatrix.m, shadow->viewProjectionMatrix.m);
    MatrixInverse44((const float *)&shadow->viewProjectionMatrix, (float *)&shadow->inverseViewProjectionMatrix);
    shadow->viewport.Width = shadow->viewport.Height = 128;
    shadow->viewport.MaxZ = 1.0f;

    cookie->fade = fade;
    cookie->shadowLookupMatrix = shadow->viewProjectionMatrix;
    for (i = 0; i < 4; ++i) {
        cookie->shadowLookupMatrix.m[i][0] *= 0.5f;
        cookie->shadowLookupMatrix.m[i][1] *= 0.5f;
    }
    cookie->shadowLookupMatrix.m[3][0] += 0.5f;
    cookie->shadowLookupMatrix.m[3][1] += 0.5f;
    memcpy(cookie->boxMin, sceneEnt->curMins, sizeof(vec3_t));
    memcpy(cookie->boxMax, sceneEnt->curMaxs, sizeof(vec3_t));
    cookie->firstCasterDrawSurf = scene.drawSurfCount;
    if (list->cookieCount <= sc_debugCasterCount->current.integer) {
        rg.debugViewParms = viewParms;
        CG_CullIn(sceneEnt->cent);
        R_SkinSceneEnt(sceneEnt, ent);
        R_AddXModelSurfaces(entIndex);
    }
    cookie->firstReceiverDrawSurf = cookie->firstUnusedDrawSurf = scene.drawSurfCount;
    R_FrustumClipPlanes(&shadow->viewMatrix, shadowFrustumSidePlanes, 5, planes);
    for (i = 0; i < receiverCount; ++i) {
        GfxDrawSurf *surf = &scene.drawSurfs[i];
        const Material *material;
        const float *bounds[2];
        int receiver, lightmap;
        R_DecomposeSort(surf->sort, &receiver, &material, &lightmap);
        if (!material->techniqueSet->techniques[32] || receiver == entIndex)
            continue;
        if (*surf->surface == SF_TRIANGLES) {
            const srfTriangles_t *tris = (const srfTriangles_t *)surf->surface;
            bounds[0] = tris->bounds[0];
            bounds[1] = tris->bounds[1];
        } else if (receiver < 2046 && (scene.def.entities[receiver].renderFxFlags & 0x100)) {
            bounds[0] = scene.sceneEnts[receiver].curMins;
            bounds[1] = scene.sceneEnts[receiver].curMaxs;
        } else {
            continue;
        }
        for (j = 0; j < 5; ++j) {
            const DpvsPlane *p = &planes[j];
            float distance = (p->coeffs[0] * bounds[p->side[0] / sizeof(vec3_t)][0] +
                              p->coeffs[1] * bounds[p->side[1] / sizeof(vec3_t)][1]) +
                              p->coeffs[2] * bounds[p->side[2] / sizeof(vec3_t)][2] + p->coeffs[3];
            if (distance <= 0.0f) break;
        }
        if (j == 5 && frontEndDataOut->drawSurfCount < 65536) {
            scene.drawSurfs[cookie->firstUnusedDrawSurf++] = *surf;
            ++scene.drawSurfCount;
            ++frontEndDataOut->drawSurfCount;
        }
    }
}

static int R_ShadowCandidateCompare(const void *a, const void *b)
{
    float x = ((const ShadowCandidate *)a)->weight;
    float y = ((const ShadowCandidate *)b)->weight;
    return (x > y) - (x < y);
}

/* Mac 1.3 0xe16f0; Windows selection 0x100280f0, assembly 0x10028330. */
void R_GenerateShadowCookies(const GfxViewParms *viewParms, const GfxViewParms *camera,
                             ShadowCookieList *list)
{
    (void)viewParms;
    ShadowCandidate candidates[24];
    int i, worst = 0, trend = 0;
    for (i = 0; i < 24; ++i) {
        candidates[i].entIndex = -1;
        candidates[i].weight = 100000000.0f;
    }
    for (i = 0; i < scene.def.entityCount; ++i) {
        GfxEntity *ent = &scene.def.entities[i];
        int flags = ent->renderFxFlags & 0xe00;
        vec3_t mins, maxs, size;
        float weight, distance;
        if (ent->reType != 0 || (ent->renderFxFlags & 8) || flags == 0x200)
            continue;
        ri.DObjGetBounds(scene.sceneEnts[i].u.obj, mins, maxs);
        for (int j = 0; j < 3; ++j) size[j] = maxs[j] - mins[j];
        float extent = size[Vec3MajorAxis(size)];
        if (extent == 0.0f) continue;
        distance = Vec3Distance(camera->origin, ent->origin);
        weight = distance / extent;
        if (!(weight < 100000000.0f)) continue;
        if (flags == 0x600) weight *= 0.25f;
        else if (flags == 0x800) weight = 0.0f;
        else if (flags == 0x400) weight *= 4.0f;
        if (weight <= candidates[worst].weight) {
            candidates[worst].entIndex = i;
            candidates[worst].weight = weight;
            worst = 0;
            for (int j = 1; j < 24; ++j)
                if (candidates[j].weight > candidates[worst].weight) worst = j;
        }
    }
    qsort(candidates, 24, sizeof(candidates[0]), R_ShadowCandidateCompare);
    list->cookieCount = 0;
    float invRange = 1.0f / (sc_fadeRange->current.value * shadowCookieGlob.weightCap);
    float fadeStart = (1.0f - sc_fadeRange->current.value) * shadowCookieGlob.weightCap;
    int receiverCount = scene.drawSurfCount;
    for (i = 0; i < 24; ++i) {
        if (candidates[i].entIndex == -1) break;
        float weight = candidates[i].weight;
        if (weight > shadowCookieGlob.weightCap) {
            if (sc_wantCount->current.integer + sc_wantCountMargin->current.integer < i) trend = -1;
            else if (sc_wantCount->current.integer - sc_wantCountMargin->current.integer > i) trend = 1;
            break;
        }
        float fade = weight > fadeStart && weight != 0.0f ? (shadowCookieGlob.weightCap - weight) * invRange : 1.0f;
        R_AddShadowCookie(camera, receiverCount, candidates[i].entIndex, fade, list);
    }
    if (i == 24) trend = -1;
    float elapsed = shadowCookieGlob.lastTime == -1 ? 0.0f : (scene.def.time - shadowCookieGlob.lastTime) * 0.001f;
    shadowCookieGlob.lastTime = scene.def.time;
    if (elapsed < 0.0f) elapsed = 0.0f;
    if (trend > 0) shadowCookieGlob.weightCap += elapsed * sc_shadowOutRate->current.value;
    else if (trend < 0) shadowCookieGlob.weightCap -= elapsed * sc_shadowInRate->current.value;
    if (shadowCookieGlob.weightCap < 0.0001f) shadowCookieGlob.weightCap = 0.0001f;
}

void R_DrawShadowCookies(int viewIndex, const GfxViewParms *viewParms, ShadowCookieList *list)
{
    int needsTarget = R_BeginDrawGroupSection((GfxDrawGroupType)1) == 0;
    const vec4_t clear = { 0.5f, 0.5f, 0.5f, 0.0f };
    R_BeginDrawGroupLoop((GfxDrawGroupType)1, viewIndex);
    /* Windows' shader blur is scheduled below; do not also use Aspyr's copy blur. */
    g_NumBlurShadowPasses = 0;
    for (int i = 0; i < list->cookieCount; ++i) {
        ShadowCookie *cookie = &list->cookies[i];
        int casters = cookie->firstReceiverDrawSurf - cookie->firstCasterDrawSurf;
        int receivers = cookie->firstUnusedDrawSurf - cookie->firstReceiverDrawSurf;
        if (!casters || !receivers || i >= sc_debugCasterCount->current.integer) continue;
        R_AddCmdShadowCookie(&cookie->shadowLookupMatrix, cookie->fade);
        R_AddCmdSetRenderTarget(R_RENDERTARGET_SHADOWCOOKIE);
        R_AddCmdClearScreen(7, clear, 1.0f, 0);
        R_AddCmdBeginView(scene.viewCount, &scene.def, cookie->shadowViewParms, &rg.lodParms);
        R_AddCmdSetViewport(1, 1, 126, 126);
        GfxDrawSurf *surfs = &scene.drawSurfs[cookie->firstCasterDrawSurf];
        qsortDrawSurfs(surfs, casters);
        R_AddCmdDrawSurfs(surfs, casters, (MaterialTechniqueType)31);
        R_AddCmdSetViewport(0, 0, 128, 128);
        if (sc_blur->current.integer) R_AddCmdBlurShadowCookie();
        R_AddCmdSetRenderTarget(R_RENDERTARGET_DYNAMICSHADOWS);
        R_AddCmdBeginView(scene.viewCount, &scene.def, viewParms, &rg.lodParms);
        if (i < sc_debugReceiverCount->current.integer) {
            surfs = &scene.drawSurfs[cookie->firstReceiverDrawSurf];
            qsortDrawSurfs(surfs, receivers);
            R_AddCmdDrawSurfs(surfs, receivers, (MaterialTechniqueType)32);
        }
        needsTarget = 0;
    }
    if (needsTarget) R_AddCmdSetRenderTarget(R_RENDERTARGET_DYNAMICSHADOWS);
    R_EndDrawGroupLoop((GfxDrawGroupType)1, viewIndex);
    R_EndDrawGroupSection((GfxDrawGroupType)1);
}
#endif
