#include "common_types.h"
#include "imports.h"

extern struct lightGlob_type lightGlob;
extern refimport_t ri;
extern r_global_permanent_t rgp;
extern r_globals_t rg;
extern const dvar_t *r_dlightLimit;
extern GfxBackEndData *frontEndDataOut;
extern GfxScene scene;

extern GfxLightDef *R_LoadLightDef(const char *name);
extern void R_Error(int level, const char *fmt, ...);
extern int stricmp(const char *s1, const char *s2);
extern const vec_t Vec3DistanceSq(const vec_t *p1, const vec_t *p2);
extern float PointToBoxDistSq(const vec_t *pt, const vec_t *mins, const vec_t *maxs);
extern const vec_t *R_BoundsForDrawSurf(const GfxDrawSurf *surf);

GfxLightDef *R_RegisterLightDef(const char *name);
void R_InitLightDefs(void);
void R_ShutdownLightDefs(void);
int R_GetPointLightPartitions(const GfxDrawSurf *drawSurfs, int drawSurfCount, PointLightPartition *partitions, int partitionLimit);

GfxLightDef *R_RegisterLightDef(const char *name)
{
    int defIndex;
    GfxLightDef *result;

    for (;;) {

        if (lightGlob.defCount > 0) {
            for (defIndex = 0; defIndex < lightGlob.defCount; defIndex++) {
                if (stricmp(name, lightGlob.defs[defIndex]->name) == 0) {
                    return lightGlob.defs[defIndex];
                }
            }
        }

        if (lightGlob.defCount == 64) {

            ri.Printf(0, "Exceeded %i light defs. Currently loaded light defs:\n");
            if (lightGlob.defCount > 0) {
                for (defIndex = 0; defIndex < lightGlob.defCount; defIndex++) {
                    ri.Printf(0, "  %s\n", lightGlob.defs[defIndex]->name);
                }
            }
            R_Error(1, "Can't load light def %s; %i unique light defs already loaded", name, lightGlob.defCount);
        }

        result = R_LoadLightDef(name);
        if (result != NULL) {

            lightGlob.defs[lightGlob.defCount] = result;
            lightGlob.defCount++;
            return result;
        }

        if (stricmp(name, "default") == 0) {
            R_Error(1, "Can't load the default light def '%s'", name);
        }
        name = "default";
    }
}

void R_InitLightDefs(void)
{
    GfxLightDef *def = R_RegisterLightDef("light_dynamic");
    rgp.dlightDef = def;
}

void R_ShutdownLightDefs(void)
{
    lightGlob.defCount = 0;
}

static void R_SelectClosestLights(const GfxLight **lights, int totalCount, int wantedCount, const vec_t *viewOrg)
{
    const GfxLight *pivotLight;
    const GfxLight *tmp;
    float cutoff_a, cutoff_b;
    float dist_a, dist_b;
    float score_a, score_b;
    int i, j;

    for (;;) {
        pivotLight = lights[0];
        i = 0;

        for (;;) {
            i++;
            if (i >= totalCount) {
                break;
            }

            cutoff_a = lights[i]->position[3];
            cutoff_b = pivotLight->position[3];
            dist_a = Vec3DistanceSq(&lights[i]->position[0], viewOrg);
            dist_b = Vec3DistanceSq(&pivotLight->position[0], viewOrg);

            score_a = cutoff_a * cutoff_a * dist_b;
            score_b = cutoff_b * cutoff_b * dist_a;

            if (score_a < score_b) {
                break;
            }
        }

        j = totalCount - 1;
        while (i <= j) {
            const GfxLight *backLight = lights[j];

            cutoff_a = pivotLight->position[3];
            cutoff_b = backLight->position[3];
            dist_a = Vec3DistanceSq(&pivotLight->position[0], viewOrg);
            dist_b = Vec3DistanceSq(&backLight->position[0], viewOrg);

            score_a = cutoff_a * cutoff_a * dist_b;
            score_b = cutoff_b * cutoff_b * dist_a;

            if (score_a < score_b) {

                tmp = lights[i];
                lights[i] = lights[j];
                lights[j] = tmp;

                for (;;) {
                    i++;
                    if (i >= j) {
                        goto partition_done;
                    }

                    cutoff_a = lights[i]->position[3];
                    cutoff_b = pivotLight->position[3];
                    dist_a = Vec3DistanceSq(&lights[i]->position[0], viewOrg);
                    dist_b = Vec3DistanceSq(&pivotLight->position[0], viewOrg);

                    score_a = cutoff_a * cutoff_a * dist_b;
                    score_b = cutoff_b * cutoff_b * dist_a;

                    if (score_a < score_b) {
                        break;
                    }
                }
                continue;
            }
            j--;
        }

    partition_done:

        if (totalCount == i) {

            tmp = lights[0];
            lights[0] = lights[j];
            lights[j] = tmp;
            i--;
        }

        if (wantedCount == i) {
            return;
        }

        if (wantedCount < i) {

            totalCount = i;
        } else {

            lights = &lights[i];
            totalCount -= i;
            wantedCount -= i;
        }
    }
}

static void R_AddDrawSurfsForLight(const GfxLight *light, const GfxDrawSurf *drawSurfs, int drawSurfCount)
{
    float cutoffDistSq;
    float distSq;
    const vec_t *origin;
    const vec_t *bounds;
    int lastDrawSurf;
    int j;
    const GfxDrawSurf *surf;
    GfxDrawSurf *dstSurfs;
    int dsc;

    origin = &light->position[0];
    cutoffDistSq = light->position[3] * light->position[3];

    lastDrawSurf = drawSurfCount - 1;
    if (lastDrawSurf < 0) {
        return;
    }

    surf = &drawSurfs[lastDrawSurf];
    for (j = 0; j < drawSurfCount; j++) {
        bounds = R_BoundsForDrawSurf(surf);
        if (bounds != NULL) {
            distSq = PointToBoxDistSq(origin, bounds, bounds + 3);
            if (distSq <= cutoffDistSq) {

                dsc = scene.drawSurfCount;
                dstSurfs = scene.drawSurfs;
                dstSurfs[dsc] = *surf;

                scene.drawSurfCount++;
                frontEndDataOut->drawSurfCount++;
            }
        }
        surf--;
    }
}

int R_GetPointLightPartitions(const GfxDrawSurf *drawSurfs, int drawSurfCount, PointLightPartition *partitions, int partitionLimit)
{
    const GfxLight *visibleLights[32];
    int visibleCount;
    int visibleLimit;
    int partitionCount;
    int i;
    PointLightPartition *curPartition;
    const GfxLight *light;
    int drawSurfsBefore;

    visibleCount = 0;
    if (scene.dlightCount > 0) {
        for (i = 0; i < scene.dlightCount; i++) {
            if (!scene.dlightCulled[i]) {
                visibleLights[visibleCount] = &scene.dlights[i];
                visibleCount++;
            }
        }
    }

    visibleLimit = partitionLimit;
    if (r_dlightLimit->current.integer < visibleLimit) {
        visibleLimit = r_dlightLimit->current.integer;
    }

    if (visibleLimit < visibleCount) {
        R_SelectClosestLights(visibleLights, visibleCount, visibleLimit, &rg.viewOrg[0]);
        visibleCount = visibleLimit;
    }

    if (visibleCount <= 0) {
        return 0;
    }

    partitionCount = 0;
    curPartition = partitions;

    for (i = 0; i < visibleCount; i++) {
        light = visibleLights[i];

        curPartition->light = light;
        curPartition->firstDrawSurf = scene.drawSurfCount;

        R_AddDrawSurfsForLight(light, drawSurfs, drawSurfCount);

        curPartition->drawSurfCount = scene.drawSurfCount - curPartition->firstDrawSurf;
        if (curPartition->drawSurfCount != 0) {
            partitionCount++;
            curPartition++;
        }
    }

    return partitionCount;
}
