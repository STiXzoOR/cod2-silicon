#include "hud_source.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cg_t cgArray[1];
unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
static centity_t entities[1024];
centity_t *cg_entities = entities;
weaponInfo_t cg_weaponsArray[128];
clientStatic_t cls;
byte *cls_ptr_195ecac = (byte *)&cls;
unsigned char spGlob[sizeof(struct ScreenPlacementGlob)] __attribute__((aligned(16)));
const vec_t colorWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static dvar_t draw_turret, paused, alpha, descriptive;
const dvar_t *cg_drawTurretCrosshair = &draw_turret;
const dvar_t *cg_paused = &paused;
const dvar_t *cg_crosshairAlpha = &alpha;
const dvar_t *cg_descriptiveText = &descriptive;
static snapshot_t snapshots[2];
static WeaponDef definitions[128];
static struct Material reticle_material, white_material;
static struct Font_s font;
static int split_screen, catchers, controls_calls, weapon_calls, follow_enabled;
static int font_height, font_calls, binding_calls, failures;
static float font_scale;

typedef struct {
    float x, y, w, h, uv[4], color[4];
    MaterialHandle material;
} picture_t;
typedef struct {
    char text[512];
    float x, y, scale, color[4];
    int horz, vert, max_chars, style;
    FontHandle font;
} text_t;
static picture_t pictures[16];
static text_t texts[16];
static int picture_count, text_count;
static const char *binding_names[] = { "+attack", "toggleads", "+speed", "+melee", "+melee_breath" };
static const char *binding_values[5];
static char binding_log[16][32];

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition); ++failures; \
} } while (0)
static int near(float left, float right) { return fabsf(left - right) < 0.001f; }
static unsigned int bits(float value)
{
    unsigned int result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

void R_AddCmdDrawStretchPic(float x, float y, float w, float h, float s1, float t1,
                           float s2, float t2, const vec_t *color, MaterialHandle material)
{
    assert(picture_count < 16 && color);
    picture_t *pic = &pictures[picture_count++];
    *pic = (picture_t){ .x = x, .y = y, .w = w, .h = h,
        .uv = { s1, t1, s2, t2 }, .material = material };
    memcpy(pic->color, color, sizeof(pic->color));
}
#include "draw_adapter.h"

void SetScreenScaling(float safe_x, float safe_y, int x, int y, int width, int height);
void UI_DrawHandlePic(float x, float y, float w, float h, int horz, int vert,
                      const vec_t *color, MaterialHandle material)
{ CL_DrawStretchPic(x, y, w, h, horz, vert, 0.0f, 0.0f, 1.0f, 1.0f, color, material); }
Bool CL_IsRenderingSplitScreen(void) { return split_screen; }
int CL_GetKeyCatchers(void) { return catchers; }
void Controls_GetConfig(void) { ++controls_calls; }
WeaponDef *BG_GetWeaponDef(int index)
{
    assert(index == 7);
    ++weapon_calls;
    return &definitions[index];
}
FontHandle UI_GetFontHandle(int index, float scale)
{
    assert(index == 0);
    ++font_calls;
    font_scale = scale;
    return &font;
}
int UI_TextHeight(FontHandle handle, float scale)
{ assert(handle == &font && bits(scale) == 0x3e555555); return font_height; }
int UI_TextWidth(const char *text, int max, FontHandle handle, float scale)
{ (void)text; (void)max; assert(handle == &font); return (int)(60.0f * scale); }
void UI_DrawText(const char *text, int max, FontHandle handle, float x, float y,
                 int horz, int vert, float scale, const vec_t *color, int style)
{
    assert(text_count < 16 && color);
    text_t *draw = &texts[text_count++];
    *draw = (text_t){ .x = x, .y = y, .scale = scale, .horz = horz, .vert = vert,
        .max_chars = max, .style = style, .font = handle };
    snprintf(draw->text, sizeof(draw->text), "%s", text);
    memcpy(draw->color, color, sizeof(draw->color));
}
const char *UI_SafeTranslateString(const char *reference)
{
    if (!strcmp(reference, "PLATFORM_FOLLOWNEXTPLAYER")) return "Suivant : &&1";
    if (!strcmp(reference, "PLATFORM_FOLLOWPREVIOUSPLAYER")) return "Précédent : &&1";
    if (!strcmp(reference, "PLATFORM_FOLLOWSTOP")) return "Arrêt : &&1";
    if (!strcmp(reference, "KEY_UNBOUND")) return "Non assigné";
    if (!strcmp(reference, "CGAME_SPECTATOR")) return "Spectateur";
    assert(!"unexpected localization reference");
    return "";
}
const char *UI_ReplaceConversionString(const char *source, const char *binding)
{
    static char result[512];
    const char *conversion = strstr(source, "&&1");
    assert(conversion);
    snprintf(result, sizeof(result), "%.*s%s%s", (int)(conversion - source), source, binding, conversion + 3);
    return result;
}
int GetKeyBindingLocalizedString(const char *command, char *keys)
{
    assert(controls_calls >= 1 && binding_calls < 16);
    snprintf(binding_log[binding_calls++], sizeof(binding_log[0]), "%s", command);
    for (int i = 0; i < 5; ++i) {
        if (!strcmp(command, binding_names[i])) {
            if (!binding_values[i]) return 0;
            snprintf(keys, 256, "%s", binding_values[i]);
            return 1;
        }
    }
    assert(!"unexpected binding command");
    return 0;
}
void I_strncpyz(char *destination, const char *source, int size)
{ assert(size > 0); snprintf(destination, (size_t)size, "%s", source); }
const char *SEH_LocalizeTextMessage(const char *text, const char *context, int error)
{ (void)text; (void)context; (void)error; return "Suit"; }
#if defined(COD2_CODX) && COD2_CODX
int Cod2x_DrawSpectatedName(void) { return follow_enabled; }
#endif

static void reset(void)
{
    memset(cgArray, 0, sizeof(cgArray));
    memset(entities, 0, sizeof(entities));
    memset(cg_weaponsArray, 0, sizeof(cg_weaponsArray));
    memset(snapshots, 0, sizeof(snapshots));
    memset(pictures, 0, sizeof(pictures));
    memset(texts, 0, sizeof(texts));
    memset(binding_log, 0, sizeof(binding_log));
    draw_turret.current.enabled = 1;
    paused.current.integer = 0;
    alpha.current.value = 0.6f;
    descriptive.current.enabled = 1;
    cg->snap = &snapshots[0];
    cg->nextSnap = &snapshots[1];
    cg->snap->ps.pm_type = 4;
    cg->nextSnap->ps.pm_type = 4;
    cg->predictedPlayerState.viewlocked_entNum = 91;
    entities[91].nextState.weapon = 7;
    entities[91].currentState.weapon = 0;
    definitions[7].iReticleCenterSize = 32;
    cg_weaponsArray[7].hReticleCenter = &reticle_material;
    cgs->media.whiteMaterial = &white_material;
    for (int i = 0; i < 5; ++i) binding_values[i] = NULL;
    binding_values[0] = "Souris gauche";
    binding_values[1] = "Souris droite";
    binding_values[3] = "V";
    picture_count = text_count = controls_calls = weapon_calls = font_calls = binding_calls = 0;
    split_screen = catchers = 0;
    follow_enabled = 1;
    font_height = 12;
    cls.vidConfig.width = 640;
    cls.vidConfig.height = 480;
    cls.vidConfig.aspectRatioPixel = 1.0f;
    SetScreenScaling(1.0f, 1.0f, 0, 0, 640, 480);
}

static void turret_material(void)
{
    CG_DrawTurretCrossHair();
    CHECK(weapon_calls == 1 && picture_count == 1);
    if (picture_count != 1) return;
    CHECK(pictures[0].material == &reticle_material);
    CHECK(near(pictures[0].x, 304.0f) && near(pictures[0].y, 224.0f));
    CHECK(near(pictures[0].w, 32.0f) && near(pictures[0].h, 32.0f));
    CHECK(near(pictures[0].color[3], 0.6f));
    CHECK(near(pictures[0].uv[0], 0.0f) && near(pictures[0].uv[1], 0.0f));
    CHECK(near(pictures[0].uv[2], 1.0f) && near(pictures[0].uv[3], 1.0f));
}

static void turret_gates(void)
{
    for (int scenario = 0; scenario < 6; ++scenario) {
        reset();
        switch (scenario) {
        case 0: draw_turret.current.enabled = 0; break;
        case 1: paused.current.integer = 1; break;
        case 2: entities[91].nextState.weapon = 0; break;
        case 3: cg_weaponsArray[7].hReticleCenter = NULL; break;
        case 4: alpha.current.value = 0.0f; break;
        case 5: alpha.current.value = 0.009f; break;
        }
        CG_DrawTurretCrossHair();
        CHECK(picture_count == 0);
        CHECK(weapon_calls == (scenario >= 3 ? 1 : 0));
    }
    reset();
    alpha.current.value = 0.01f;
    CG_DrawTurretCrossHair();
    CHECK(picture_count == 1 && near(pictures[0].color[3], 0.01f));
}

static void turret_color(void)
{
    const int flags[] = { 0, 0x100000, 0x200000, 0x300000 };
    for (int i = 0; i < 4; ++i) {
        reset();
        cg->predictedPlayerState.pm_flags = flags[i];
        cg->predictedPlayerState.eFlags = flags[i] ^ 0x100000;
        CG_DrawTurretCrossHair();
        CHECK(picture_count == 1);
        CHECK(near(pictures[0].color[0], flags[i] & 0x100000 ? 0.25f : 1.0f));
        CHECK(near(pictures[0].color[1], 1.0f));
        CHECK(near(pictures[0].color[2], flags[i] & 0x100000 ? 0.25f : 1.0f));
    }
}

static void turret_placement(void)
{
    const int sizes[][2] = { { 640, 480 }, { 1280, 720 }, { 2560, 1440 } };
    for (int split = 0; split < 2; ++split) {
        for (int i = 0; i < 3; ++i) {
            reset();
            int width = sizes[i][0], height = sizes[i][1] / (split ? 2 : 1);
            split_screen = split;
            cls.vidConfig.width = sizes[i][0];
            cls.vidConfig.height = sizes[i][1];
            SetScreenScaling(1.0f, 1.0f, 0, 0, width, height);
            CG_DrawTurretCrossHair();
            float expected_size = (split ? 64.0f : 32.0f) * (float)height / 480.0f;
            CHECK(picture_count == 1);
            CHECK(near(pictures[0].w, expected_size) && near(pictures[0].h, expected_size));
            CHECK(near(pictures[0].x, ((float)width - expected_size) * 0.5f));
            CHECK(near(pictures[0].y, ((float)height - expected_size) * 0.5f));
        }
    }
}

static void spectator_gates(void)
{
    for (int scenario = 0; scenario < 4; ++scenario) {
        reset();
        cg->snap->ps.pm_type = 5;
        cg->nextSnap->ps.pm_flags = 0x3000000;
        switch (scenario) {
        case 0: descriptive.current.enabled = 0; break;
        case 1: catchers = 8; break;
        case 2: cg->nextSnap->ps.pm_flags = 0; break;
        case 3: cg->nextSnap = NULL; break;
        }
        CG_DrawSpectatorMessage();
        CHECK(text_count == 0 && controls_calls == 0 && binding_calls == 0);
    }
    reset();
    catchers = 4;
    cg->nextSnap->ps.pm_flags = 0x2000000;
    CG_DrawSpectatorMessage();
    CHECK(text_count == 1 && controls_calls == 1);
}

static void spectator_bindings(void)
{
    cg->nextSnap->ps.pm_flags = 0x3000000;
    binding_values[2] = "Majuscule";
    binding_values[4] = "E";
    CG_DrawSpectatorMessage();
    CHECK(text_count == 3 && controls_calls == 1);
    CHECK(!strcmp(texts[0].text, "Suivant : Souris gauche"));
    CHECK(!strcmp(texts[1].text, "Précédent : Souris droite"));
    CHECK(!strcmp(texts[2].text, "Arrêt : V"));
    text_count = binding_calls = 0;
    binding_values[0] = "Q ou Souris gauche";
    CG_DrawSpectatorMessage();
    CHECK(text_count == 3 && controls_calls == 2);
    CHECK(!strcmp(texts[0].text, "Suivant : Q ou Souris gauche"));
    reset();
    cg->nextSnap->ps.pm_flags = 0x3000000;
    binding_values[0] = binding_values[1] = binding_values[3] = NULL;
    binding_values[2] = "Majuscule";
    binding_values[4] = "E";
    CG_DrawSpectatorMessage();
    CHECK(text_count == 3);
    CHECK(!strcmp(texts[0].text, "Suivant : Non assigné"));
    CHECK(!strcmp(texts[1].text, "Précédent : Majuscule"));
    CHECK(!strcmp(texts[2].text, "Arrêt : E"));
    CHECK(binding_calls >= 4 && !strcmp(binding_log[0], "toggleads"));
    CHECK(!strcmp(binding_log[1], "+speed") && !strcmp(binding_log[2], "+melee"));
    CHECK(!strcmp(binding_log[3], "+melee_breath"));
    reset();
    cg->nextSnap->ps.pm_flags = 0x3000000;
    for (int i = 0; i < 5; ++i) binding_values[i] = NULL;
    CG_DrawSpectatorMessage();
    CHECK(text_count == 3);
    CHECK(!strcmp(texts[1].text, "Précédent : Non assigné"));
    CHECK(!strcmp(texts[2].text, "Arrêt : Non assigné"));
}

static void spectator_layout(void)
{
    const int flags[] = { 0x1000000, 0x2000000, 0x3000000 };
    for (int split = 0; split < 2; ++split) {
        for (int i = 0; i < 3; ++i) {
            reset();
            split_screen = split;
            font_height = 15;
            cg->snap->ps.pm_flags = flags[i] ^ 0x3000000;
            cg->nextSnap->ps.pm_flags = flags[i];
            CG_DrawSpectatorMessage();
            int count = i == 0 ? 2 : i == 1 ? 1 : 3;
            CHECK(text_count == count && controls_calls == 1 && font_calls == 1);
            CHECK(bits(font_scale) == 0x3e555555);
            for (int line = 0; line < count; ++line) {
                CHECK(near(texts[line].x, 240.0f) && near(texts[line].y, 406.0f + 15.0f * line));
                CHECK(bits(texts[line].scale) == 0x3e555555);
                CHECK(texts[line].font == &font && texts[line].horz == 0 && texts[line].vert == 0);
                CHECK(texts[line].max_chars == 0x7fffffff && texts[line].style == 3);
                for (int channel = 0; channel < 4; ++channel) CHECK(near(texts[line].color[channel], 1.0f));
            }
        }
    }
}

static void follow_policy(void)
{
    cg->snap->ps.pm_flags = 0x400000;
    cg->snap->ps.clientNum = 2;
    cg->bgs.clientinfo[2].infoValid = 1;
    snprintf(cg->bgs.clientinfo[2].name, sizeof(cg->bgs.clientinfo[2].name), "Player Two");
    CHECK(CG_DrawFollow() == 1 && text_count == 2);
    CHECK(!strcmp(texts[1].text, "Player Two"));
    text_count = 0;
    follow_enabled = 0;
#if defined(COD2_CODX) && COD2_CODX
    CHECK(CG_DrawFollow() == 0 && text_count == 0);
#else
    CHECK(CG_DrawFollow() == 1 && text_count == 2);
#endif
    text_count = 0;
    follow_enabled = 1;
    cg->inKillCam = 1;
    CHECK(CG_DrawFollow() == 0 && text_count == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    reset();
    if (!strcmp(argv[1], "turret-material")) turret_material();
    else if (!strcmp(argv[1], "turret-gates")) turret_gates();
    else if (!strcmp(argv[1], "turret-color")) turret_color();
    else if (!strcmp(argv[1], "turret-placement")) turret_placement();
    else if (!strcmp(argv[1], "spectator-gates")) spectator_gates();
    else if (!strcmp(argv[1], "spectator-bindings")) spectator_bindings();
    else if (!strcmp(argv[1], "spectator-layout")) spectator_layout();
    else if (!strcmp(argv[1], "follow-policy")) follow_policy();
    else assert(!"unknown case");
    if (!failures) printf("PASS %s\n", argv[1]);
    return failures != 0;
}
