#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ownerdraw-source.h"

cg_t cgArray[1];
unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
static centity_t entities[1024];
centity_t *cg_entities = entities;
static snapshot_t snapshots[2];
static dvar_t compass_size, min_range, max_range, min_radius, objective_range, objective_alpha;
static dvar_t fade, enabled, warnings, warning_errors, springy;
const dvar_t *cg_hudCompassSize = &compass_size;
const dvar_t *cg_hudCompassMinRange = &min_range;
const dvar_t *cg_hudCompassMaxRange = &max_range;
const dvar_t *cg_hudCompassMinRadius = &min_radius;
const dvar_t *cg_hudObjectiveMaxRange = &objective_range;
const dvar_t *cg_hudObjectiveMinAlpha = &objective_alpha;
const dvar_t *hud_fade_compass = &fade;
const dvar_t *loc_warnings = &warnings;
const dvar_t *loc_warningsAsErrors = &warning_errors;
const dvar_t **dvar_compassSize = &cg_hudCompassSize;
static const dvar_t *springy_pointer = &springy;
char **compPointerConfig = (char **)&springy_pointer;
static struct Material default_icon, server_icon, supplied_icon;
static struct Font_s font;
static int catchers, allow_ui_hud, compass_enabled, failures;
static int register_calls, config_calls, last_config, localization_calls, warning_calls;
static const char *material_name, *location_name, *localized_location;
static float offset_x, offset_y;

typedef struct {
    float x, y, w, h, color[4];
    int horz, vert;
    MaterialHandle material;
} picture_t;
static picture_t pictures[32];
static int picture_count, text_count;
static char drawn_text[1024];
static float text_x, text_y, text_scale, text_color[4];
static int text_horz, text_vert, text_style, text_max;
static FontHandle text_font;

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition); ++failures; \
} } while (0)
static int near(float a, float b) { return fabsf(a - b) < 0.002f; }

int CL_GetKeyCatchers(void) { return catchers; }
Bool CL_GetDisplayHUDWithKeycatchUI(void) { return allow_ui_hud; }
void UI_DrawHandlePic(float x, float y, float w, float h, int horz, int vert,
                      const vec_t *rgba, MaterialHandle material)
{
    assert(picture_count < 32);
    picture_t *p = &pictures[picture_count++];
    *p = (picture_t){ .x = x, .y = y, .w = w, .h = h, .horz = horz, .vert = vert, .material = material };
    memcpy(p->color, rgba, sizeof(p->color));
}
void UI_DrawText(const char *text, int max, FontHandle handle, float x, float y,
                 int horz, int vert, float scale, const vec_t *rgba, int style)
{
    ++text_count;
    snprintf(drawn_text, sizeof(drawn_text), "%s", text);
    text_x = x; text_y = y; text_scale = scale;
    text_horz = horz; text_vert = vert; text_style = style; text_max = max; text_font = handle;
    memcpy(text_color, rgba, sizeof(text_color));
}
const char *CL_GetConfigString(int index)
{
    ++config_calls;
    last_config = index;
    if (index >= 0x61f && index <= 0x69d) return material_name;
    assert(index == 7 + 0x2e || index == 0x2e);
    return location_name;
}
MaterialHandle CL_RegisterMaterialNoMip(const char *name, int track)
{
    assert(!strcmp(name, material_name) && track == 7);
    ++register_calls;
    return &server_icon;
}
const char *SEH_StringEd_GetString(const char *key)
{
    ++localization_calls;
    if (!strcmp(key, "CGAME_UNKNOWN")) return "Unknown place";
    assert(!strcmp(key, location_name));
    return localized_location;
}
void I_strncpyz(char *dst, const char *src, int count) { snprintf(dst, (size_t)count, "%s", src); }
void I_strncat(char *dst, int count, const char *src)
{ strncat(dst, src, (size_t)count - strlen(dst) - 1); }
void Com_Printf(const char *format, ...) { (void)format; ++warning_calls; }
void Com_Error(errorParm_t error, const char *format, ...) { (void)error; (void)format; abort(); }
const char *va(const char *format, ...) { (void)format; abort(); }
#if defined(COD2_CODX) && COD2_CODX
int Cod2x_DrawCompass(void) { return compass_enabled; }
void Cod2x_CompassOffset(float *x, float *y) { *x += offset_x; *y += offset_y; }
#endif

const float vectoyaw(const vec_t *vec);
const float AngleNormalize360(const float angle);
const float AngleNormalize180(const float angle);
const float AngleSubtract(const float a1, const float a2);
#include "ownerdraw-dependencies.h"

static void reset(void)
{
    memset(cgArray, 0, sizeof(cgArray));
    memset(cgsArray, 0, sizeof(cgsArray));
    memset(entities, 0, sizeof(entities));
    memset(snapshots, 0, sizeof(snapshots));
    memset(pictures, 0, sizeof(pictures));
    picture_count = text_count = register_calls = config_calls = localization_calls = warning_calls = 0;
    catchers = allow_ui_hud = 0;
    compass_enabled = 1;
    offset_x = offset_y = 0;
    compass_size.current.value = 1.0f;
    min_range.current.value = 100.0f;
    max_range.current.value = 300.0f;
    min_radius.current.value = 0.2f;
    objective_range.current.value = 700.0f;
    objective_alpha.current.value = 0.25f;
    fade.current.value = 0.0f;
    enabled.current.enabled = 1;
    warnings.current.enabled = warning_errors.current.enabled = springy.current.enabled = 0;
    hud_enable = &enabled;
    cg->drawHud = 1;
    cg->snap = &snapshots[0];
    cg->nextSnap = &snapshots[1];
    cg->snap->ps.clientNum = 1;
    cg->predictedPlayerState.clientNum = 2;
    cg->nextSnap->ps.clientNum = 3;
    cg->bgs.clientinfo[3].infoValid = 1;
    cg->bgs.clientinfo[3].location = 7;
    cg->refdef.vieworg[0] = 50.0f;
    cg->refdef.vieworg[1] = 60.0f;
    cg->refdef.vieworg[2] = 70.0f;
    cg->time = 1000;
    cg->compassFadeTime = 100;
    cg->compPointerYaw = 123.0f;
    cgs->media.objectiveMaterials[0] = &default_icon;
    material_name = "synthetic/objective";
    location_name = "SYNTHETIC_LOCATION";
    localized_location = "Translated place";
}

static objective_t *objective(int slot, float dx, float dy, float dz)
{
    objective_t *o = &cg->nextSnap->ps.objective[slot];
    o->state = OBJST_CURRENT;
    o->entNum = 1023;
    o->origin[0] = 50.0f + dx;
    o->origin[1] = 60.0f + dy;
    o->origin[2] = 70.0f + dz;
    return o;
}

static void draw(int id)
{
    vec4_t rgba = { 0.2f, 0.4f, 0.8f, 0.17f };
    CG_OwnerDraw(20.0f, 40.0f, 100.0f, 80.0f, 2, 3, 3.0f, 5.0f,
                 id, 0, 0, 0.0f, &font, 0.75f, rgba, &supplied_icon, 6);
    CHECK(near(rgba[0], 0.2f) && near(rgba[1], 0.4f) && near(rgba[2], 0.8f) && near(rgba[3], 0.17f));
}

static void objectives(void)
{
    for (int i = 0; i < 16; ++i) objective(i, 200.0f, 0.0f, 0.0f)->state = (objectiveState_t)(i % 6);
    objective(15, 200.0f, 0.0f, 0.0f)->teamNum = 2;
    draw(86);
    CHECK(picture_count == 3);
    if (picture_count != 3) return;
    for (int i = 0; i < 3; ++i) {
        CHECK(pictures[i].material == &default_icon);
        CHECK(near(pictures[i].x, 65.0f) && near(pictures[i].y, 50.75f));
        CHECK(near(pictures[i].w, 16.0f) && near(pictures[i].h, 16.0f));
        CHECK(near(pictures[i].color[3], 1.0f));
    }
    reset();
    for (int i = 0; i < 16; ++i) objective(i, 0.0f, 0.0f, 0.0f);
    draw(86);
    CHECK(picture_count == 16);
}

static void distance(void)
{
    // Hand-derived cardinal vectors avoid using the production radius/alpha oracle.
    const float vectors[][4] = { { 0, 0, 68.25f, 1 }, { 100, 0, 68.25f, 1 },
        { 200, 0, 50.75f, 1 }, { 300, 0, 33.25f, 1 }, { 500, 0, 33.25f, 0.625f },
        { 700, 0, 33.25f, 0.25f }, { 900, 0, 33.25f, 0.25f } };
    for (int i = 0; i < 7; ++i) {
        reset();
        objective(0, vectors[i][0], 0, 900.0f);
        draw(86);
        CHECK(picture_count == 1);
        if (!picture_count) continue;
        CHECK(near(pictures[0].x, 65.0f) && near(pictures[0].y, vectors[i][2]));
        CHECK(near(pictures[0].color[3], vectors[i][3]));
    }
    reset();
    objective(0, 0, 200, 0);
    draw(86);
    CHECK(picture_count == 1 && near(pictures[0].x, 38.75f) && near(pictures[0].y, 77.0f));
    reset();
    min_range.current.value = max_range.current.value = 300.0f;
    objective_range.current.value = 300.0f;
    objective(0, 300, 0, 0);
    draw(86);
    CHECK(picture_count == 1 && near(pictures[0].y, 68.25f) && near(pictures[0].color[3], 1.0f));
}

static void entity_origin(void)
{
    objective(0, 0, -900, 0)->entNum = 17;
    entities[17].lerpOrigin[0] = 250.0f;
    entities[17].lerpOrigin[1] = 60.0f;
    entities[17].lerpOrigin[2] = -900.0f;
    draw(86);
    CHECK(picture_count == 1 && near(pictures[0].x, 65.0f) && near(pictures[0].y, 50.75f));
    CHECK(near(pictures[0].color[3], 1.0f));
}

static void materials(void)
{
    const int icons[] = { 0, 1, 127, 128, -1 };
    for (int i = 0; i < 5; ++i) {
        reset();
        objective(0, 200, 0, 0)->icon = icons[i];
        draw(86);
        int valid = i == 1 || i == 2;
        CHECK(picture_count == 1 && pictures[0].material == (valid ? &server_icon : &default_icon));
        CHECK(register_calls == valid && config_calls == valid);
        if (valid) CHECK(last_config == icons[i] + 0x61e);
    }
    char long_name[65];
    memset(long_name, 'x', sizeof(long_name) - 1); long_name[64] = 0;
    for (int i = 0; i < 3; ++i) {
        reset();
        material_name = i == 0 ? "" : long_name;
        if (i == 2) long_name[63] = 0;
        objective(0, 200, 0, 0)->icon = 1;
        draw(86);
        CHECK(picture_count == 1 && pictures[0].material == (i == 2 ? &server_icon : &default_icon));
        CHECK(register_calls == (i == 2));
    }
}

static void fade_tests(void)
{
    for (int i = 0; i < 6; ++i) {
        reset();
        objective(0, 500, 0, 0);
        switch (i) {
        case 0: cg->drawHud = 0; break;
        case 1: enabled.current.enabled = 0; break;
        case 2: catchers = 8; break;
        case 3: fade.current.value = 1.0f; cg->compassFadeTime = 0; break;
        case 4: fade.current.value = 0.9f; break;
        case 5: catchers = 8; allow_ui_hud = 1; fade.current.value = 1.0004f; break;
        }
        draw(86);
        CHECK(picture_count == (i == 5));
        CHECK(near(cg->compPointerYaw, i == 5 ? 0.0f : 123.0f));
        if (i == 5) CHECK(near(pictures[0].color[3], 1.0f / 7.0f));
    }
    reset();
    fade.current.value = 2.0f;
    objective(0, 500, 0, 0);
    draw(86);
    CHECK(picture_count == 1 && near(pictures[0].color[3], 0.625f));
}

static void client_gates(void)
{
    objective(0, 200, 0, 0);
    cg->bgs.clientinfo[1].infoValid = cg->bgs.clientinfo[2].infoValid = 1;
    cg->bgs.clientinfo[3].infoValid = 0;
    draw(86);
    draw(33);
    CHECK(picture_count == 0 && text_count == 0 && config_calls == 0);
    for (int i = 0; i < 3; ++i) {
        reset();
        objective(0, 200, 0, 0);
        if (i == 0) cg->nextSnap = NULL;
        else cg->nextSnap->ps.clientNum = i == 1 ? -1 : 64;
        draw(86);
        draw(33);
        CHECK(picture_count == 0 && text_count == 0 && config_calls == 0);
    }
}

static void placement(void)
{
    compass_size.current.value = 2.0f;
    cg->refdefViewAngles[1] = 90.0f;
    objective(0, 200, 0, 0);
    draw(86);
    CHECK(picture_count == 1);
    CHECK(near(pictures[0].x, 159.5f) && near(pictures[0].y, 29.0f));
    CHECK(near(pictures[0].w, 32.0f) && near(pictures[0].h, 32.0f));
    CHECK(pictures[0].horz == 2 && pictures[0].vert == 3);
    CHECK(near(pictures[0].color[0], 0.2f) && near(pictures[0].color[1], 0.4f) && near(pictures[0].color[2], 0.8f));
}

static void codx_policy(void)
{
    objective(0, 200, 0, 0);
    compass_enabled = 0;
    draw(86);
#if defined(COD2_CODX) && COD2_CODX
    CHECK(picture_count == 0 && near(cg->compPointerYaw, 123.0f));
#else
    CHECK(picture_count == 1);
#endif
    picture_count = 0;
    compass_enabled = 1;
    offset_x = 12.0f; offset_y = -4.0f;
    draw(86);
    CHECK(picture_count == 1);
#if defined(COD2_CODX) && COD2_CODX
    CHECK(near(pictures[0].x, 77.0f) && near(pictures[0].y, 46.75f));
#else
    CHECK(near(pictures[0].x, 65.0f) && near(pictures[0].y, 50.75f));
#endif
}

static void location(void)
{
    cg->bgs.clientinfo[1].infoValid = cg->bgs.clientinfo[2].infoValid = 1;
    cg->bgs.clientinfo[1].location = 11;
    cg->bgs.clientinfo[2].location = 12;
    cg->drawHud = 0; enabled.current.enabled = 0; catchers = 8;
    draw(33);
    CHECK(text_count == 1 && !strcmp(drawn_text, "Translated place"));
    CHECK(last_config == 7 + 0x2e && localization_calls == 1);
    CHECK(near(text_x, 23.0f) && near(text_y, 125.0f) && near(text_scale, 0.75f));
    CHECK(text_horz == 2 && text_vert == 3 && text_style == 6 && text_max == 0x7fffffff && text_font == &font);
    CHECK(near(text_color[3], 0.17f));
}

static void location_fallback(void)
{
    cg->bgs.clientinfo[3].location = 0;
    location_name = "";
    draw(33);
    CHECK(text_count == 1 && !strcmp(drawn_text, "Unknown place"));
    reset();
    localized_location = NULL;
    draw(33);
    CHECK(text_count == 1 && !strcmp(drawn_text, "SYNTHETIC_LOCATION"));
    reset();
    localized_location = NULL;
    warnings.current.enabled = 1;
    draw(33);
    CHECK(text_count == 1 && !strcmp(drawn_text, "^1UNLOCALIZED(^7SYNTHETIC_LOCATION^1)^7"));
    CHECK(warning_calls == 1);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    reset();
    if (!strcmp(argv[1], "objectives")) objectives();
    else if (!strcmp(argv[1], "distance")) distance();
    else if (!strcmp(argv[1], "entity-origin")) entity_origin();
    else if (!strcmp(argv[1], "materials")) materials();
    else if (!strcmp(argv[1], "fade")) fade_tests();
    else if (!strcmp(argv[1], "client-gates")) client_gates();
    else if (!strcmp(argv[1], "placement")) placement();
    else if (!strcmp(argv[1], "codx-policy")) codx_policy();
    else if (!strcmp(argv[1], "location")) location();
    else if (!strcmp(argv[1], "location-fallback")) location_fallback();
    else abort();
    if (!failures) printf("PASS %s\n", argv[1]);
    return failures != 0;
}
