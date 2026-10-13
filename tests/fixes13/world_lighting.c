#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static GfxWorld testWorld;
static r_global_permanent_t rgp;
static refimport_t ri;
static dvar_t vars[7];
static dvar_t *handles[7] = {&vars[0], &vars[1], &vars[2], &vars[3], &vars[4], &vars[5], &vars[6]};
static void *imp_r_lightTweakAmbient = &handles[0];
static void *imp_r_lightTweakDiffuseFraction = &handles[1];
static void *imp_r_lightTweakSunLight = &handles[2];
static void *imp_r_lightTweakAmbientColor = &handles[3];
static void *imp_r_lightTweakSunColor = &handles[4];
static void *imp_r_lightTweakSunDiffuseColor = &handles[5];
static void *imp_r_lightTweakSunDirection = &handles[6];
static float observed[7][4];
static int seen[7];

static void RecordFloat(const dvar_t *dvar, float value)
{
    int index = (int)(dvar - vars);
    assert(index >= 0 && index < 3);
    observed[index][0] = value;
    ++seen[index];
}
static void RecordColor(const dvar_t *dvar, float r, float g, float b, float a)
{
    int index = (int)(dvar - vars);
    assert(index >= 3 && index < 6);
    observed[index][0] = r; observed[index][1] = g;
    observed[index][2] = b; observed[index][3] = a;
    ++seen[index];
}
static void RecordAngles(const dvar_t *dvar, float x, float y, float z)
{
    assert(dvar == &vars[6]);
    observed[6][0] = x; observed[6][1] = y; observed[6][2] = z;
    ++seen[6];
}
#include "world_lighting_source.h"

int main(void)
{
    const float expected[7][4] = {
        {.125f}, {.375f}, {1.75f}, {.25f, .5f, .75f, 1},
        {.875f, .625f, .375f, 1}, {.75f, .125f, .5f, 1}, {31, -47, 83}
    };
    strcpy(testWorld.sunParse.name, "synthetic-map-distinct-fields");
    testWorld.sunParse.ambientScale = .125f;
    testWorld.sunParse.diffuseFraction = .375f;
    testWorld.sunParse.sunLight = 1.75f;
    memcpy(testWorld.sunParse.ambientColor, expected[3], sizeof(vec3_t));
    memcpy(testWorld.sunParse.sunColor, expected[4], sizeof(vec3_t));
    memcpy(testWorld.sunParse.diffuseColor, expected[5], sizeof(vec3_t));
    memcpy(testWorld.sunParse.angles, expected[6], sizeof(vec3_t));
    rgp.world = &testWorld;
    ri.Dvar_SetFloat = RecordFloat;
    ri.Dvar_SetColor = RecordColor;
    ri.Dvar_SetVec3 = RecordAngles;
    initialize_world_lighting();
    int failures = 0;
    for (int i = 0; i < 7; ++i) {
        assert(seen[i] == 1);
        int components = i < 3 ? 1 : i == 6 ? 3 : 4;
        for (int j = 0; j < components; ++j) {
            if (observed[i][j] != expected[i][j]) {
                fprintf(stderr, "FAIL lighting field %d channel %d: %g expected %g\n", i, j, observed[i][j], expected[i][j]);
                ++failures;
            }
        }
    }
    if (!failures) puts("PASS map lighting scalars, colors, and direction reach matching dvars");
    return failures != 0;
}
