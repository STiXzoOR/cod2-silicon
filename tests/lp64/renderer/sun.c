#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "sun_source.h"
#include "sun_dvar_source.h"

static materialCommands_t commands;
static r_backEndGlobals_t backend;
static DxGlobals deviceGlobals;
static GfxWorld fixtureWorld;
static GfxViewParms view;
static dvar_t enabled, renderer;
static Material sprite, flare, white, additive, screen;
static r_index_t indices[64];
static vidConfig_t video;
static dvar_t *drawSun = &enabled;
const dvar_t *sv_cheats = &enabled;
void *imp_backEnd = &backend, *imp_tess = &commands, *imp_dx = &deviceGlobals;
void *imp_r_drawSun = &drawSun, *imp_vidConfig = &video;
static float whiteColor[4] = { 1, 1, 1, 1 };
void *imp_colorWhite = whiteColor;
dvar_t *r_rendererInUse = &renderer;
r_global_permanent_t rgp;
refimport_t ri;
int alwaysfails;
SunFlareDynamic sunFlareArray[4];
static int commandArgc, loadCalls;
static char commandArgv[] = "test";
static char message[256];
static int cmd_argc(void)
{
    return commandArgc;
}
static char *cmd_argv(int index)
{
    assert(index == 1);
    return commandArgv;
}
static void print_message(int level, const char *format, ...)
{
    assert(level == 0);
    snprintf(message, sizeof(message), "%s", format);
}
void R_LoadSunThroughDvars(const char *name, sunflare_t *sun)
{
    assert(!strcmp(name, "test") && sun == &rgp.world->sun);
    loadCalls++;
}
const char *va(const char *format, ...)
{
    return "sun/test.sun";
}
static int draws, matrices, traceHit, traceCalls, fullscreens, begins, sleeps;
static D3DCOLOR screenColor;
static GfxVertex captured[4];
static GfxVertexDx7 capturedDx7[4];
static const Material *drawMaterial;
static int issues[8], issueCount, getResult, pending, sampleValue;
static void *queryVtable[8], *deviceVtable[43];
static void **query = queryVtable, **device = deviceVtable;

static void near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.00001f);
}
void RB_EndSurface(void)
{
    if (commands.vertexCount) {
        assert(commands.vertexCount == 4 && commands.indexCount == 6);
        memcpy(captured, commands.verts.dx9.generic, sizeof(captured));
        memcpy(capturedDx7, commands.verts.dx7.generic, sizeof(capturedDx7));
        drawMaterial = commands.material;
        draws++;
    }
    commands.vertexCount = commands.indexCount = commands.optimizedIndexCount = 0;
}
void RB_BeginSurface(const Material *m, MaterialTechniqueType t, int l)
{
    assert(l == 31);
    commands.material = m;
    commands.techType = t;
}
void RB_PushMatrixStack(void)
{
    matrices++;
}
void RB_PopMatrixStack(void)
{
    matrices--;
}
void RB_SetProjectionMatrix(const D3DMATRIX *m)
{
    near(m->m[0][0], 1);
}
void RB_SetViewMatrix(const D3DMATRIX *m)
{
    near(m->m[3][3], 1);
}
void RB_Set3D(void)
{
    backend.projection2D = 0;
}
void RB_SetViewMatrixForWDx7(float w)
{
    assert(w == 0 || w == 1);
}
void MatrixIdentity44(float (*m)[4])
{
    memset(m, 0, 64);
    for (int i = 0; i < 4; i++)
        m[i][i] = 1;
}
void Vec3Cross(const vec_t *a, const vec_t *b, vec_t *c)
{
    c[0] = a[1] * b[2] - a[2] * b[1];
    c[1] = a[2] * b[0] - a[0] * b[2];
    c[2] = a[0] * b[1] - a[1] * b[0];
}
const vec_t Vec3Normalize(vec_t *v)
{
    float n = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (n)
        for (int i = 0; i < 3; i++)
            v[i] /= n;
    return n;
}
int CM_BoxSightTrace(int old, const vec_t *start, const vec_t *end, const vec_t *mins, const vec_t *maxs, int model, int mask)
{
    assert(mask == 0x2003 && model == 0);
    near(end[0] - start[0], fixtureWorld.sun.sunFxPosition[0] * 262144);
    assert(mins[0] == 0 && maxs[2] == 0);
    traceCalls++;
    return traceHit;
}
void RB_DrawFullScreenColoredQuad(const Material *m, float s, float t, float u, float v, D3DCOLOR c)
{
    assert(m == &screen);
    fullscreens++;
    screenColor = c;
}
void RB_ClearScreen(int which, const vec_t *c, float depth, int stencil)
{
    assert(which == 2 && depth == 1 && stencil == 0);
    near(c[0], 1);
}
void RB_DrawStretchPic(const Material *m, float x, float y, float w, float h, float s, float t, float u, float v, D3DCOLOR c, GfxPrimStatsTarget stats)
{
    assert(m == &white && w == 16 && h == 16 && c == 0xffffffff && stats == 10);
}
void WinSleep(DWORD ms)
{
    assert(ms == 0);
    sleeps++;
}
static int D3DVTCC begin(void *d)
{
    assert(d == &device);
    begins++;
    return 0;
}
static int D3DVTCC end_scene(void *d)
{
    assert(d == &device);
    begins--;
    return 0;
}
static int D3DVTCC issue(void *q, int f)
{
    assert(q == &query);
    issues[issueCount++] = f;
    return 0;
}
static int D3DVTCC query_get(void *q, void *data, int size, int flags)
{
    assert(q == &query && size == 4 && flags == 1);
    if (pending) {
        pending--;
        return 1;
    }
    *(unsigned *)data = sampleValue;
    return getResult;
}
static void setup(void)
{
    commands.indices = indices;
    rgp.world = &fixtureWorld;
    rgp.whiteMaterial = &white;
    rgp.additiveMaterial = &additive;
    rgp.glareBlindMaterial = &screen;
    enabled.current.enabled = 1;
    renderer.current.integer = 1;
    fixtureWorld.sun.hasValidData = 1;
    fixtureWorld.sun.spriteMaterial = &sprite;
    fixtureWorld.sun.spriteSize = 16;
    fixtureWorld.sun.sunFxPosition[0] = 1;
    backend.viewParms = &view;
    backend.width = 640;
    backend.height = 480;
    backend.tileCount = 1;
    backend.sceneDef.time = 100;
    view.axis[0][0] = 1;
    video.width = 640;
    video.height = 480;
    backend.viewProjectionMatrix.m[0][2] = 0.5f;
    backend.viewProjectionMatrix.m[0][3] = 1;
    view.viewProjectionMatrix = backend.viewProjectionMatrix;
    queryVtable[6] = (void *)issue;
    queryVtable[7] = (void *)query_get;
    deviceVtable[41] = (void *)begin;
    deviceVtable[42] = (void *)end_scene;
    deviceGlobals.device = (void (*)())&device;
    deviceGlobals.sunSpriteSamples = 256;
}
static void quad(void)
{
    commands.vertexCount = 2;
    commands.indexCount = 1;
    GfxVertex *v = RB_SetTessQuad(bm_color_from_bits(0x12345678));
    assert(v == &commands.verts.dx9.generic[2] && commands.vertexCount == 6 && commands.indexCount == 7);
    unsigned short expected[] = { 5, 2, 4, 4, 2, 3 };
    for (int i = 0; i < 6; i++)
        assert(indices[i + 1] == expected[i]);
    for (int i = 0; i < 4; i++) {
        assert(v[i].color.packed == 0x12345678);
        near(v[i].normal[2], 1);
        near(v[i].tangent[0], 1);
        near(v[i].binormal[1], 1);
    }
    commands.vertexCount = commands.indexCount = 0;
    GfxVertexDx7 *d = RB_SetTessQuadDx7(bm_color_from_bits(0xaabbccdd));
    assert(d == commands.verts.dx7.generic && d[3].color.packed == 0xaabbccdd);
    near(d[2].texCoord[1], 1);
}
static void sprite_case(void)
{
    RB_DrawSun(0);
    assert(draws == 1 && drawMaterial == &sprite && traceCalls == 1);
    near(sunFlareArray[0].lastVisibility, 1);
    near(sunFlareArray[0].lastDot, 1);
    float h = 16 * 0.0013110929867252707f;
    near(captured[0].xyzw[0], 1);
    near(captured[0].xyzw[1], -h);
    near(captured[0].xyzw[2], -h);
    near(captured[0].xyzw[3], 0);
    assert(captured[0].color.packed == 0xffffffff);
    renderer.current.integer = 2;
    RB_DrawSun(0);
    assert(draws == 2);
    near(capturedDx7[2].xyz[1], h);
    near(capturedDx7[2].xyz[2], h);
}
static void visibility_case(void)
{
    view.viewProjectionMatrix.m[0][0] = 1;
    RB_DrawSun(0);
    near(sunFlareArray[0].lastVisibility, .5f);
    traceHit = 3;
    RB_DrawSun(0);
    assert(sunFlareArray[0].hitNum == 3);
    near(sunFlareArray[0].lastVisibility, 0);
    view.viewProjectionMatrix.m[0][3] = -1;
    int calls = traceCalls;
    RB_DrawSun(0);
    assert(traceCalls == calls);
    near(sunFlareArray[0].lastVisibility, 0);
}
static void query_case(void)
{
    sunFlareArray[0].sunQuery[0] = (IDirect3DQuery9 *)&query;
    sunFlareArray[0].sunQueryIssued[0] = 1;
    sampleValue = 128;
    view.axis[0][0] = -1;
    RB_DrawSun(0);
    assert(draws == 1 && drawMaterial == &additive && traceCalls == 0 && matrices == 0);
    near(sunFlareArray[0].lastVisibility, .5f);
    assert(issues[0] == 2 && issues[1] == 1);
    near(captured[0].xyzw[0], 16.0f / 640);
    near(captured[0].xyzw[1], 16.0f / 480);
    near(captured[0].xyzw[2], .499f);
    assert(captured[0].color.packed == 0x000000ff);
    getResult = -1;
    sampleValue = 0;
    RB_DrawSun(0);
    near(sunFlareArray[0].lastVisibility, .5f);
    assert(!sunFlareArray[0].error);
}
static void samples_case(void)
{
    assert(RB_CalcSunSpriteSamples() == 0);
    sunFlareArray[0].sunQuery[0] = (IDirect3DQuery9 *)&query;
    sampleValue = 384;
    pending = 1;
    assert(RB_CalcSunSpriteSamples() == 384 && sleeps == 1 && begins == 0);
    getResult = -1;
    assert(RB_CalcSunSpriteSamples() == 256);
}
static void flare_case(void)
{
    fixtureWorld.sun.flareMaterial = &flare;
    fixtureWorld.sun.flareMinDot = .5f;
    fixtureWorld.sun.flareMaxDot = 1;
    fixtureWorld.sun.flareMinSize = 10;
    fixtureWorld.sun.flareMaxSize = 100;
    fixtureWorld.sun.flareMaxAlpha = .8f;
    sunFlareArray[0].lastDot = .75f;
    sunFlareArray[0].lastVisibility = .5f;
    RB_DrawSunPostEffects(0);
    assert(draws == 1 && drawMaterial == &flare && matrices == 0);
    assert(captured[0].color.packed == 0x333333ff);
    near(captured[0].xyzw[0], 60.0f / 640);
    near(captured[0].xyzw[1], 60.0f / 480);
    renderer.current.integer = 2;
    RB_DrawSunPostEffects(0);
    assert(draws == 2 && capturedDx7[0].color.packed == 0x333333ff);
    near(capturedDx7[0].xyz[0], 60.0f / 640);
    near(capturedDx7[0].xyz[2], .499f);
}
static void commands_case(void)
{
    ri.Cmd_Argc = cmd_argc;
    ri.Cmd_Argv = cmd_argv;
    ri.Printf = print_message;
    commandArgc = 1;
    R_Cmd_LoadSun();
    assert(!strcmp(message, "USAGE: r_loadsun <sunname>\n  sunname must not have an extension\n"));
    R_Cmd_SaveSun();
    assert(!strcmp(message, "USAGE: r_savesun <sunname>\n  sunname must not have an extension\n"));
    commandArgc = 2;
    enabled.current.enabled = 0;
    R_Cmd_LoadSun();
    assert(!strcmp(message, "You must have cheats enabled to use r_loadsun\n"));
    enabled.current.enabled = 1;
    rgp.world = NULL;
    R_Cmd_LoadSun();
    assert(!strcmp(message, "You can't r_loadsun while a map isn't loaded\n"));
    rgp.world = &fixtureWorld;
    R_Cmd_LoadSun();
    assert(loadCalls == 1);
}
static void fade_case(void)
{
    fixtureWorld.sun.blindMaxDarken = .5f;
    fixtureWorld.sun.glareMaxLighten = .25f;
    fixtureWorld.sun.blindMaxDot = fixtureWorld.sun.glareMaxDot = 1;
    fixtureWorld.sun.blindFadeInTime = fixtureWorld.sun.glareFadeInTime = 100;
    fixtureWorld.sun.blindFadeOutTime = fixtureWorld.sun.glareFadeOutTime = 200;
    sunFlareArray[0].lastVisibility = 1;
    RB_DrawSunPostEffects(0);
    near(sunFlareArray[0].currentBlind, .1f);
    assert(screenColor == 0x0606060c);
    RB_DrawSunPostEffects(0);
    near(sunFlareArray[0].currentBlind, .1f);
    backend.sceneDef.time = 120;
    RB_DrawSunPostEffects(0);
    near(sunFlareArray[0].currentBlind, .3f);
    sunFlareArray[0].lastVisibility = 0;
    backend.sceneDef.time = 140;
    RB_DrawSunPostEffects(0);
    near(sunFlareArray[0].currentBlind, .2f);
    backend.sceneDef.time = 130;
    RB_DrawSunPostEffects(0);
    near(sunFlareArray[0].currentBlind, .15f);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    setup();
    if (!strcmp(argv[1], "quad"))
        quad();
    else if (!strcmp(argv[1], "sprite"))
        sprite_case();
    else if (!strcmp(argv[1], "visibility"))
        visibility_case();
    else if (!strcmp(argv[1], "query"))
        query_case();
    else if (!strcmp(argv[1], "samples"))
        samples_case();
    else if (!strcmp(argv[1], "flare"))
        flare_case();
    else if (!strcmp(argv[1], "fade"))
        fade_case();
    else if (!strcmp(argv[1], "commands"))
        commands_case();
    else
        assert(0);
    puts("sun fixture passed");
}
