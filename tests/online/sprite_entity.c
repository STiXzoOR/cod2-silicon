#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static char tessStorage[64];
void *imp_tess = tessStorage;
static r_backEndGlobals_t backEnd;
void *imp_backEnd = &backEnd;
static dvar_t renderer, railWidth;
dvar_t *r_rendererInUse = &renderer;
const dvar_t *r_railCoreWidth = &railWidth;
static float builtRadius[2];
static int sprites;
static char *RB_TessBase(void) { return (char *)imp_tess; }
static void RB_BuildSprite_impl(const char *re, const float *worldRadius)
{ (void)re; builtRadius[0] = worldRadius[0]; builtRadius[1] = worldRadius[1]; ++sprites; }
void RB_TessParticleCloud(const GfxEntity *re) { (void)re; }
void MakeNormalVectors(const vec_t *forward, vec_t *right, vec_t *up) { (void)forward; (void)right; (void)up; }
static void RB_AddQuadStampDx7_impl(const vec_t *o, const vec_t *l, const vec_t *u, int c, float s0, float t0, float s1, float t1)
{ (void)o; (void)l; (void)u; (void)c; (void)s0; (void)t0; (void)s1; (void)t1; }
static void RB_AddQuadStamp_impl(const vec_t *o, const vec_t *l, const vec_t *u, int c, float s0, float t0, float s1, float t1)
{ (void)o; (void)l; (void)u; (void)c; (void)s0; (void)t0; (void)s1; (void)t1; }
static void RB_AddLineDx7_impl(const vec_t *a, const vec_t *b, float w, D3DCOLOR c, float s0, float t0, float s1, float t1)
{ (void)a; (void)b; (void)w; (void)c; (void)s0; (void)t0; (void)s1; (void)t1; }
static void RB_AddLine_impl(const vec_t *a, const vec_t *b, float w, D3DCOLOR c, float s0, float t0, float s1, float t1)
{ (void)a; (void)b; (void)w; (void)c; (void)s0; (void)t0; (void)s1; (void)t1; }
#include "sprite_entity_source.h"
int main(void)
{
    GfxViewParms view;
    GfxEntity sprite;
    memset(&view, 0, sizeof(view));
    memset(&sprite, 0, sizeof(sprite));
    backEnd.viewParms = &view;
    /* w = origin . column 3 + m[15] = 1 */
    ((float *)&view.viewProjectionMatrix)[15] = 1.0f;
    for (int k = 0; k < 3; ++k)
        ((float *)&view.inverseViewProjectionMatrix)[4 + k] = 1.0f;
    view.axis[2][0] = 0.5f;
    sprite.reType = 4;
    sprite.renderFxFlags = 0x2000; /* byte 5 bit 0x20: radius is a screen height */
    sprite.radius[1] = 3.0f;
    RB_TessEntity(&sprite);
    /* 2 * 3 * (1 * 0.5) * w */
    assert(sprites == 1 && builtRadius[0] == 3.0f && builtRadius[1] == 3.0f);
    puts("online: screen-height sprite entities stay inside their radius storage");
    return 0;
}
