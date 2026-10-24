#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "utils_source.h"

refimport_t ri;
static vec3_t zero;
static infoParm_t parms[23];
void *imp_vec3_origin = zero;
void *imp_infoParms = parms;
static trace_t result;
static int traceCalls;

static int Trace(trace_t *out, const vec_t *start, const vec_t *end,
                  const vec_t *mins, const vec_t *maxs, clipHandle_t model, int mask)
{
    assert(start[0] == 10 && end[0] == 262154 && end[1] == 20 && end[2] == 30);
    assert(mins == zero && maxs == zero && model == 0 && mask == 0x0f83fff7);
    ++traceCalls;
    *out = result;
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "fresnel")) {
        /* Total internal reflection: asin's input saturates at one. */
        float value = FresnelTerm(1.5f, 1.0f, 0.5f);
        assert(isfinite(value) && fabsf(value - 1.0f) < 0.000001f);
        return 0;
    }
    ri.CM_BoxTrace = Trace;
    printf("trace slot=%zu, legacy selected slot=%u\n", offsetof(refimport_t, CM_BoxTrace) / sizeof(void *), 0x158 / 4);
    fflush(stdout);
    result.fraction = 0.5f;
    result.material = "test_material";
    result.contents = 1;
    result.surfaceFlags = !strcmp(argv[1], "default") ? 0 : 0x100000;
    result.startsolid = !strcmp(argv[1], "startsolid");
    parms[0].name = "bark";
    const vec3_t origin = {10,20,30}, direction = {1,0,0};
    char name[64] = "unchanged", flags[64] = "unchanged", contents[64] = "unchanged";
    int picked = R_PickMaterial(origin,direction,name,flags,contents,64);
    assert(traceCalls == 1);
    if (result.startsolid) {
        assert(!picked && !strcmp(name,"unchanged"));
    } else {
        assert(picked && !strcmp(name,"test_material") && !strcmp(contents,"solid"));
        assert(!strcmp(flags,result.surfaceFlags ? "bark" : "^1default^7"));
    }
    puts("renderer material utility passed");
    return 0;
}
