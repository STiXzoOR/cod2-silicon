/* Script-visible HUD contracts through the complete production translation unit. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern BuiltinMethod HudElem_GetMethod(const char **);
extern void Scr_SetHudElemField(int, int);
extern void Scr_GetHudElemField(int, int);
extern void GScr_NewHudElem(void);
extern void GScr_NewClientHudElem(void);
extern void GScr_NewTeamHudElem(void);
extern void HudElem_DestroyAll(void);
extern void HudElem_UpdateClient(gclient_t *, int, hudelem_update_t);
game_hudelem_t g_hudelems[1024];
level_locals_t level;
scr_const_t scr_const;
const char str_002b3f60[] = "fullscreen";
static gentity_t entity;
static gclient_t client;
static float floats[5], vectorValue[3];
static int ints[5], paramCount, freed;
static const char *hudStrings[5];
static game_hudelem_t *addedHud;
static char outputString[2048];
static float outputFloat, outputVector[3];
static int expectedError, errorKind, errorIndex;
static jmp_buf errorJump;

int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
float Scr_GetFloat(unsigned int index) { assert(index < 5); return floats[index]; }
int Scr_GetInt(unsigned int index) { assert(index < 5); return ints[index]; }
const char *Scr_GetString(unsigned int index) { assert(index < 5); return hudStrings[index]; }
const char *Scr_GetIString(unsigned int index) { return Scr_GetString(index); }
unsigned int Scr_GetConstString(unsigned int index) { return Scr_GetInt(index); }
void Scr_GetVector(unsigned int index, float *v) { assert(index == 0); memcpy(v, vectorValue, sizeof(vectorValue)); }
unsigned int Scr_GetNumParam(void) { return paramCount; }
gentity_t *Scr_GetEntity(unsigned int index) { assert(index == 0); return &entity; }
void Scr_AddHudElem(game_hudelem_t *hud) { addedHud = hud; }
void Scr_FreeHudElem(game_hudelem_t *hud) { assert(hud >= g_hudelems && hud < g_hudelems + 1024); ++freed; }
void Scr_AddFloat(float x) { outputFloat = x; }
void Scr_AddVector(const float *v) { memcpy(outputVector, v, sizeof(outputVector)); }
void Scr_AddString(const char *s) { snprintf(outputString, sizeof(outputString), "%s", s); }
void Scr_AddClassField(int classnum, const char *name, unsigned int offset) { abort(); }
void Scr_GetGenericField(byte *p, fieldtype_t type, int offset) { abort(); }
void Scr_SetGenericField(byte *p, fieldtype_t type, int offset) { abort(); }
void Scr_SetString(scr_string_t *p, unsigned int value) { *p = value; }
int G_LocalizedStringIndex(const char *s) { assert(!strcmp(s, "label")); return 17; }
int G_ShaderIndex(const char *s) { assert(!strcmp(s, "shader")); return 18; }
qboolean SV_MapExists(const char *s) { return !strcmp(s, "mp_test"); }
const char *Scr_GetGameTypeNameForScript(const char *s) { return !strcmp(s, "dm") ? "Deathmatch" : NULL; }
void SV_SetConfigstring(int index, const char *s) { assert(index == 19 || index == 20); }
const char *va(const char *format, ...)
{
    static char buffer[2048];
    va_list args; va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); va_end(args); return buffer;
}
void Com_Printf(const char *format, ...)
{
    va_list args; va_start(args, format);
    vsnprintf(outputString, sizeof(outputString), format, args); va_end(args);
}
static void Error(int kind, int index, const char *message)
{
    assert(expectedError);
    errorKind = kind; errorIndex = index;
    snprintf(outputString, sizeof(outputString), "%s", message);
    longjmp(errorJump, 1);
}
void Scr_Error(const char *s) { Error(1, -1, s); }
void Scr_ObjectError(const char *s) { Error(2, -1, s); }
void Scr_ParamError(unsigned int i, const char *s) { Error(3, i, s); }

static void Reset(void)
{
    memset(g_hudelems, 0, sizeof(g_hudelems));
    memset(&client, 0, sizeof(client));
    memset(&entity, 0, sizeof(entity));
    memset(floats, 0, sizeof(floats)); memset(ints, 0, sizeof(ints));
    memset(hudStrings, 0, sizeof(hudStrings));
    level.time = 1000; entity.client = &client; entity.s.number = 3;
    addedHud = NULL; freed = expectedError = errorKind = 0;
    paramCount = 1; hudStrings[0] = "label";
    g_hudelems[0].elem.type = HE_TYPE_TEXT;
    g_hudelems[0].elem.color.rgba = -1;
}
static void Command(const char *name)
{
    BuiltinMethod method = HudElem_GetMethod(&name);
    assert(method);
    ((void (*)(scr_entref_t))method)((scr_entref_t){ .entnum = 0, .classnum = 1 });
}
static void Lookup(void)
{
    const char *name = "settext";
    assert(HudElem_GetMethod(&name)); assert(!strcmp(name, "settext"));
    name = "SetText"; assert(HudElem_GetMethod(&name) == NULL);
    assert(!strcmp(name, "SetText"));
    name = "unknown"; assert(HudElem_GetMethod(&name) == NULL);
}
static void FontField(void)
{
    Reset(); hudStrings[0] = "BIGFIXED";
    Scr_SetHudElemField(0, 4); assert(g_hudelems[0].elem.font == 1);
    Scr_GetHudElemField(0, 4); assert(!strcmp(outputString, "bigfixed"));
    g_hudelems[0].elem.alignOrg = 0xff;
    hudStrings[0] = "right"; Scr_SetHudElemField(0, 5);
    assert(g_hudelems[0].elem.alignOrg == 0xfb);
    hudStrings[0] = "bottom"; Scr_SetHudElemField(0, 6);
    assert(g_hudelems[0].elem.alignOrg == 0xfa);
}
static void TextValue(void)
{
    Reset(); g_hudelems[0].elem.value = 42;
    Command("settext");
    assert(g_hudelems[0].elem.type == HE_TYPE_TEXT);
    assert(g_hudelems[0].elem.text == 17 && g_hudelems[0].elem.value == 0);
}
static void TypeValue(const char *command)
{
    Reset(); g_hudelems[0].elem.value = 42;
    g_hudelems[0].elem.moveStartTime = 800; g_hudelems[0].elem.moveTime = 900;
    g_hudelems[0].elem.fadeStartTime = 600; g_hudelems[0].elem.fadeTime = 700;
    floats[0] = 2; floats[1] = 4;
    if (!strcmp(command, "setshader")) hudStrings[0] = "shader";
    if (!strcmp(command, "setmapnamestring")) hudStrings[0] = "mp_test";
    if (!strcmp(command, "setclock")) { paramCount = 3; hudStrings[2] = "shader"; }
    Command(command);
    assert(g_hudelems[0].elem.value == 0);
    assert(g_hudelems[0].elem.moveStartTime == 800 && g_hudelems[0].elem.moveTime == 900);
    assert(g_hudelems[0].elem.fadeStartTime == 600 && g_hudelems[0].elem.fadeTime == 700);
}
static void ResetMove(void)
{
    Reset(); game_hudelem_t *h = &g_hudelems[0];
    h->elem.moveStartTime = 900; h->elem.moveTime = 700;
    h->elem.value = 42; h->elem.x = 50; h->clientNum = 3; h->team = TEAM_ALLIES;
    Command("reset");
    assert(h->elem.moveStartTime == 900 && h->elem.moveTime == 700);
    assert(h->elem.value == 0 && h->elem.x == 0 && h->elem.fontScale == 1);
    assert(h->clientNum == 3 && h->team == TEAM_ALLIES && h->archived == 1);
}
static void ReuseMove(void)
{
    for (int mode = 0; mode < 3; ++mode) {
        Reset(); game_hudelem_t *h = &g_hudelems[0];
        h->elem.moveStartTime = 900; h->elem.moveTime = 700;
        Command("destroy"); assert(freed == 1 && h->elem.type == HE_TYPE_FREE);
        if (mode == 0) GScr_NewHudElem();
        else if (mode == 1) GScr_NewClientHudElem();
        else { scr_const.allies = 42; ints[0] = 42; GScr_NewTeamHudElem(); }
        assert(addedHud == h);
        assert(h->elem.moveStartTime == 900 && h->elem.moveTime == 700);
        assert(h->clientNum == (mode == 1 ? 3 : 1023));
        assert(h->team == (mode == 2 ? TEAM_ALLIES : TEAM_FREE) && h->archived == 1);
    }
}
static void Controls(void)
{
    Reset(); floats[0] = 7.5f; Command("setvalue");
    assert(g_hudelems[0].elem.type == HE_TYPE_VALUE && g_hudelems[0].elem.value == 7.5f);
    hudStrings[0] = "shader"; paramCount = 3; ints[1] = 16; ints[2] = 32;
    Command("setshader"); assert(g_hudelems[0].elem.materialIndex == 18);
    assert(g_hudelems[0].elem.width == 16 && g_hudelems[0].elem.height == 32);
    HudElem_DestroyAll(); assert(g_hudelems[0].elem.type == HE_TYPE_FREE);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "lookup")) Lookup();
    else if (!strcmp(argv[1], "font")) FontField();
    else if (!strcmp(argv[1], "text-value")) TextValue();
    else if (!strcmp(argv[1], "shader-value")) TypeValue("setshader");
    else if (!strcmp(argv[1], "map-value")) TypeValue("setmapnamestring");
    else if (!strcmp(argv[1], "timer-value")) TypeValue("settimer");
    else if (!strcmp(argv[1], "clock-value")) TypeValue("setclock");
    else if (!strcmp(argv[1], "reset-move")) ResetMove();
    else if (!strcmp(argv[1], "reuse-move")) ReuseMove();
    else if (!strcmp(argv[1], "controls")) Controls();
    else abort();
    printf("HUD %s: pass\n", argv[1]);
    return 0;
}
