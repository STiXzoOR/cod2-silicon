#include "cod2_feature_config.h"
#include "common_types.h"
#include "cg_cod2x_spectator.h"
#if COD2X_SPECTATOR
#include "headers/PC/cgame_mp/cg_local.h"
#include "PC/qcommon/cod2x_features.h"
#include "PC/gfx_d3d/r_cod2x_xray.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterFloat(const char *, float, float, float, unsigned short);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern void Dvar_GetUnpackedColor(const dvar_t *, float *);
extern clientConnection_t clientConnections;
extern clientStatic_t cls;
extern scr_const_t scr_const;
extern struct DObj_s *Com_GetClientDObj(int, int);
extern qboolean CG_DObjGetWorldTagPos(const centity_t *, const struct DObj_s *, unsigned int, float *);
extern void CG_TraceCapsule(trace_t *, const float *, const float *, const float *, const float *, int, int);
extern WeaponDef *BG_GetWeaponDef(int);
extern int BG_GetNumWeapons(void);
extern MaterialHandle CL_RegisterMaterial(const char *, int);
extern const char *CL_GetConfigString(int);
extern const char *SEH_LocalizeTextMessage(const char *, const char *, int);
extern sharedUiInfo_t sharedUiInfo;
extern void UI_DrawText(const char *, int, FontHandle, float, float, int, int, float, const float *, int);
extern int UI_TextWidth(const char *, int, FontHandle, float);
extern int UI_TextHeight(FontHandle, float);
extern void CG_DrawRotatedPic(float, float, float, float, int, int, float, const float *, MaterialHandle);


static const dvar_t *style, *stateStyle[2], *visible[2], *fadeRadius, *fadeAlpha;
static const dvar_t *threshold, *outline, *glow, *core, *teamColor[2], *health, *healthColor, *healthAlpha;
static const dvar_t *head[3][4], *nameScale, *weaponScale, *iconScale, *headOffset, *maxDistance;
static Cod2xXraySettings settings;
static int viewer = -1, teams[4], infoSeen;
static struct { int weapon, time; } offhand[64];

static float Dot(const float *a, const float *b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
static float Delta(const float *p, float *d)
{
    for (int k = 0; k < 3; ++k) d[k] = p[k] - cg->refdef.vieworg[k];
    return Dot(d, d);
}
static float GameFloat(const char *name, float fallback)
{
    const dvar_t *d = Dvar_FindVar(name);
    return d && d->type == 1 ? d->current.value : fallback;
}
static void ReadColor(const dvar_t *override, const char *fallback, float r, float g, float b, float *out)
{
    out[0] = r; out[1] = g; out[2] = b; out[3] = 1;
    const dvar_t *d = override->current.string[0] ? override : (fallback ? Dvar_FindVar(fallback) : NULL);
    if (d) Dvar_GetUnpackedColor(d, out);
    for (int i = 0; i < 3; ++i) out[i] = fminf(1, fmaxf(0, out[i]));
    out[3] = 1;
}
void CG_Cod2xSpectatorInit(void)
{
#if DEBUG
    style = Dvar_RegisterInt("cg_xray", 0, 0, 4, 0x1000);
#else
    style = Dvar_RegisterInt("cg_xray", 0, 0, 4, 0x1080);
#endif
    stateStyle[0] = Dvar_RegisterInt("cg_xraySpectator", 2, 0, 2, 0x1000);
    stateStyle[1] = Dvar_RegisterInt("cg_xrayDead", 2, 0, 2, 0x1000);
    visible[0] = Dvar_RegisterBool("cg_xrayVisibleSpectator", 0, 0x1000);
    visible[1] = Dvar_RegisterBool("cg_xrayVisibleDead", 0, 0x1000);
    fadeRadius = Dvar_RegisterFloat("cg_xrayCenterFadeRadius", 80, 0, 320, 0x1000);
    fadeAlpha = Dvar_RegisterFloat("cg_xrayCenterFadeAlpha", .15f, 0, 1, 0x1000);
    threshold = Dvar_RegisterFloat("cg_xrayHiddenThreshold", 24, 0, 256, 0x1000);
    outline = Dvar_RegisterFloat("cg_xrayOutlineWidth", 1.5f, .5f, 4, 0x1000);
    glow = Dvar_RegisterFloat("cg_xrayGlowIntensity", 1, 0, 4, 0x1000);
    core = Dvar_RegisterFloat("cg_xrayGlowCoreIntensity", .5f, 0, 2, 0x1000);
    teamColor[0] = Dvar_RegisterString("cg_xrayColorAxis", "", 0x1000);
    teamColor[1] = Dvar_RegisterString("cg_xrayColorAllies", "", 0x1000);
    health = Dvar_RegisterBool("cg_xrayHealth", 1, 0x1000);
    healthColor = Dvar_RegisterString("cg_xrayHealthColor", "", 0x1000);
    healthAlpha = Dvar_RegisterFloat("cg_xrayHealthAlpha", .8f, 0, 1, 0x1000);
    const char *states[] = {"Spectator", "Dead", "Alive"};
    const char *elements[] = {"NationIcon", "Name", "WeaponIcon", "WeaponName"};
    const int defaults[3][4] = {{0,2,2,0}, {1,2,2,0}, {1,0,0,0}};
    for (int s = 0; s < 3; ++s) for (int e = 0; e < 4; ++e) {
        char name[64]; snprintf(name, sizeof(name), "cg_head%s%s", elements[e], states[s]);
        head[s][e] = Dvar_RegisterInt(name, defaults[s][e], 0, 2, 0x1000);
    }
    nameScale = Dvar_RegisterFloat("cg_headNameScale", 1, .3f, 3, 0x1000);
    weaponScale = Dvar_RegisterFloat("cg_headWeaponIconScale", 1, 0, 3, 0x1000);
    iconScale = Dvar_RegisterFloat("cg_headIconScale", 1, 0, 10, 0x1000);
    headOffset = Dvar_RegisterFloat("cg_headOffset", 0, -64, 128, 0x1000);
    maxDistance = Dvar_RegisterFloat("cg_headMaxDistance", 0, 0, 10000, 0x1000);
}
int CG_Cod2xPlayerInfo(int client, Cod2xPlayerInfo *out)
{
    if ((unsigned)client >= 64) return 0;
    const centity_t *c = &cg_entities[client];
    unsigned bits = (unsigned)c->currentState.dmgFlags;
    if (!(c->nextValid & 1) || c->currentState.eType != 1 || bits >> 24 != 0xc1) return 0;
    out->health = c->currentState.surfType & 255;
    out->primary = bits & 127; out->secondary = (bits >> 7) & 127;
    out->frag = (bits >> 14) & 15; out->smoke = (bits >> 18) & 15;
    return 1;
}
void CG_Cod2xSpectatorReset(void)
{
    viewer = -1; infoSeen = 0; settings.mode = 0;
}
void CG_Cod2xSpectatorFrame(void)
{
    if (!style) return;
    viewer = -1; settings.mode = 0; settings.visible = 0;
    const dvar_t *revision = Dvar_FindVar("g_cod2x"), *cheat = Dvar_FindVar("sv_cheats");
    int cheats = Cod2x_FeaturesDemo() || (cheat && cheat->current.enabled);
#if DEBUG
    cheats = 1;
#endif
    if (clientConnections.state != CA_ACTIVE || !cg->snap) { infoSeen = 0; return; }
    if (cheats || (revision && revision->current.integer >= 7)) {
        int team = cg->bgs.clientinfo[cg->clientNum].team;
        int following = cg->snap->ps.clientNum != cg->clientNum || (cg->snap->ps.pm_flags & 0x1000000);
        viewer = Cod2x_FeaturesDemo() || (team == 3 && following) ? 0 : following ? 1 : 2;
        int all = viewer == 0;
        if (cheats && style->current.integer) {
            settings.mode = style->current.integer; all = 1;
        } else if (viewer < 2) {
            int v = stateStyle[viewer]->current.integer;
            settings.mode = v ? v + 2 : 0;
        }
        settings.visible = viewer < 2 && visible[viewer]->current.enabled;
        for (int i = 0; i < 4; ++i) teams[i] = i != 3 && (all || (i && i == team));
    }
    Cod2xPlayerInfo info;
    if (!infoSeen && (!revision || revision->current.integer < 7))
        for (int i = 0; i < 64; ++i) if (CG_Cod2xPlayerInfo(i, &info)) { infoSeen = 1; break; }
    settings.health = health->current.enabled && (infoSeen || (revision && revision->current.integer >= 7)) && settings.mode >= 2;
    ReadColor(teamColor[0], "g_TeamColor_Axis", 1, .2f, .2f, settings.color[1]);
    ReadColor(teamColor[1], "g_TeamColor_Allies", .2f, .4f, 1, settings.color[2]);
    memcpy(settings.color[0], settings.color[1], sizeof(settings.color[0]));
    memcpy(settings.color[3], settings.color[2], sizeof(settings.color[0]));
    ReadColor(healthColor, NULL, 1, 0, 0, settings.healthColor);
    for (int i = 0; i < 4; ++i) settings.healthColor[i] *= healthAlpha->current.value;
    settings.nearPlane = GameFloat("r_znear", 4);
    settings.threshold = threshold->current.value; settings.outline = outline->current.value;
    settings.glow = glow->current.value; settings.core = core->current.value;
    settings.fadeRadius = fadeRadius->current.value; settings.fadeAlpha = fadeAlpha->current.value;
}
void CG_Cod2xSpectatorSettings(Cod2xXraySettings *out)
{
    *out = settings;
    memcpy(out->origin, cg->refdef.vieworg, sizeof(out->origin));
}
void CG_Cod2xTagPlayer(GfxEntity *ent, const struct DObj_s *obj, const centity_t *cent)
{
    int team = cg->bgs.clientinfo[cent->nextState.clientNum].team & 3;
    if (!settings.mode || !teams[team]) return;
    ent->renderFxFlags |= COD2X_XRAY_TAG | (team << COD2X_XRAY_TEAM_SHIFT);
    if (settings.mode == 1) ent->renderFxFlags |= 0x10;
    if (!settings.health) return;
    int hp; Cod2xPlayerInfo info;
    if (cg->snap && cent->nextState.clientNum == cg->snap->ps.clientNum) hp = cg->snap->ps.stats[0];
    else if (CG_Cod2xPlayerInfo(cent->nextState.clientNum, &info)) hp = info.health;
    else return;
    if (hp <= 0 || hp >= 100) return;
    float point[3], h;
    if (obj && CG_DObjGetWorldTagPos(cent, obj, scr_const.j_head, point)) h = point[2] + 7 - cent->lerpOrigin[2];
    else h = cent->nextState.eFlags & 8 ? 20 : cent->nextState.eFlags & 4 ? 50 : 70;
    int level = (int)(fmaxf(10, h) * hp / 100.0f / 2.5f + .5f);
    if (level < 1) level = 1; if (level > 31) level = 31;
    ent->renderFxFlags |= (unsigned)level << COD2X_XRAY_HEALTH_SHIFT;
}
float CG_Cod2xHeadIconScale(void) { return iconScale ? iconScale->current.value : 1; }
int CG_Cod2xHeadOffset(void) { return headOffset ? (int)headOffset->current.value : 0; }
static int HeadInfo(const centity_t *c, int values[4])
{
    memset(values, 0, 4 * sizeof(int));
    if (viewer < 0 || !cg->snap || !(c->nextValid & 1) || c->currentState.eType != 1 || (c->currentState.eFlags & 0x20000)) return 0;
    int n = c->currentState.clientNum;
    if ((unsigned)n >= 64) return 0;
    const clientInfo_t *ci = &cg->bgs.clientinfo[n];
    if (ci->infoValid != 1 || ci->team == 3 || n == cg->clientNum || n == cg->snap->ps.clientNum) return 0;
    int team = cg->bgs.clientinfo[cg->clientNum].team;
    if (viewer != 0 && (!team || ci->team != team)) return 0;
    float delta[3];
    if (maxDistance->current.value && Delta(c->lerpOrigin, delta) > maxDistance->current.value * maxDistance->current.value) return 1;
    for (int e = 0; e < 4; ++e) values[e] = head[viewer][e]->current.integer;
    return 1;
}
int CG_Cod2xHeadHidesNation(const centity_t *c)
{
    int values[4];
    if (!HeadInfo(c, values)) return 0;
    /* Distance suppresses head information but retains the cvar-only stock-icon rule. */
    return !(head[viewer][0]->current.integer == 1 && !head[viewer][1]->current.integer &&
             !head[viewer][2]->current.integer && !head[viewer][3]->current.integer);
}
int CG_Cod2xHeadHidesName(int n)
{
    int values[4];
    return (unsigned)n < 64 && HeadInfo(&cg_entities[n], values) && values[1];
}
void CG_Cod2xHeadEvent(const centity_t *c, int event)
{
    int n = c->nextState.number;
    if ((unsigned)n >= 64) return;
    if (event == 0xa5) { offhand[n].weapon = c->nextState.eventParm; offhand[n].time = cg->time; }
    if (event == 0xa6) offhand[n].weapon = 0;
}
static float FadeBox(float x, float y, float width, float height)
{
    float radius = settings.fadeRadius;
    if (radius <= 0) return 1;
    float dx = fmaxf(0, fabsf(x) - width * .5f);
    float dy = y - height > 0 ? y - height : y < 0 ? y : 0;
    float t = fminf(1, sqrtf(dx*dx + dy*dy) / radius);
    return settings.fadeAlpha + (1-settings.fadeAlpha)*t*t*(3-2*t);
}
static void HeadPoint(const centity_t *c, float *point)
{
    struct DObj_s *obj = Com_GetClientDObj(c->nextState.number, c->localClientNum);
    if (!obj || !CG_DObjGetWorldTagPos(c, obj, scr_const.j_head, point)) {
        memcpy(point, c->lerpOrigin, sizeof(vec3_t)); point[2] += 61;
    }
}
void CG_Cod2xHeadDraw(void)
{
    if (viewer < 0 || !cg->snap) return;
    FontHandle font = sharedUiInfo.assets.textFont;
    int depthAvailable = 0;
#if COD2X_XRAY_RENDER
    depthAvailable = R_Cod2xHeadAvailable();
#endif
    float scale = .2f * nameScale->current.value;
    float line = UI_TextHeight(font, scale) * .75f;
    float weaponHeight = UI_TextHeight(font, .2f) * .75f * 2.4f * weaponScale->current.value;
    for (int i = 0; i < 64; ++i) {
        centity_t *c = &cg_entities[i]; int values[4];
        if (!HeadInfo(c, values)) continue;
        if (values[0] == 1 && !values[1] && !values[2] && !values[3]) values[0] = 0;
        if (!(values[0] || values[1] || values[2] || values[3])) continue;
        clientInfo_t *ci = &cg->bgs.clientinfo[c->currentState.clientNum];
        float point[3], headPoint[3], d[3]; HeadPoint(c, headPoint);
        memcpy(point, headPoint, sizeof(point)); point[2] += headOffset->current.value;
        int viewTeam = cg->bgs.clientinfo[(cg->nextSnap ? cg->nextSnap : cg->snap)->ps.clientNum].team;
        float size = -1;
        if (c->nextState.eFlags & 0x80) size = GameFloat("cg_connectionIconSize", 0);
        else if ((ci->team == viewTeam || viewTeam == 3) && (c->voiceChatSpriteTime > cg->time || (c->nextState.eFlags & 0x200000))) size = GameFloat("cg_voiceIconSize", 0);
        float factor = iconScale->current.value;
        if (size >= 0 && factor > 0) {
            float dist = sqrtf(Delta(point, d));
            float min = factor * GameFloat("cg_headIconMinScreenRadius", .02f) * dist * tanf(cg->refdef.fov_y*(float)M_PI/360.0f)*2;
            point[2] += 23 + fmaxf((size+10)*factor, min);
        } else point[2] += 10;
        Delta(point, d); float forward = Dot(d, cg->refdef.viewaxis[0]);
        if (forward < 1 || cg->refdef.height <= 0) continue;
        float x = -Dot(d, cg->refdef.viewaxis[1]) / forward / tanf(cg->refdef.fov_x*(float)M_PI/360.0f) * (cg->refdef.width/2.0f) * 480 / cg->refdef.height;
        float y = -Dot(d, cg->refdef.viewaxis[2]) / forward / tanf(cg->refdef.fov_y*(float)M_PI/360.0f) * 240;
        float viewPoint[3] = {-Dot(d,cg->refdef.viewaxis[1]), Dot(d,cg->refdef.viewaxis[2]), forward};
        int hidden = 0;
        for (int e = 0; e < 4; ++e) {
            if ((values[e] == 1 && !depthAvailable) ||
                (values[e] == 2 && settings.fadeRadius > 0)) {
                trace_t trace; float zero[3] = {0}; headPoint[2] += 4;
                CG_TraceCapsule(&trace, cg->refdef.vieworg, zero, zero, headPoint, cg->snap->ps.clientNum, 0x811);
                hidden = trace.fraction < .99f;
                break;
            }
        }
        int weapon = c->currentState.weapon;
        int client = c->currentState.clientNum;
        if (offhand[client].weapon && cg->time >= offhand[client].time && cg->time - offhand[client].time < 5000) weapon = offhand[client].weapon;
        else if ((c->currentState.eFlags & (0x300 | 0x4000)) || (ci->clientConditions[3][0] & ((1<<18)|(1<<19)))) weapon = 0;
        WeaponDef *def = weapon > 0 && weapon <= BG_GetNumWeapons() ? BG_GetWeaponDef(weapon) : NULL;
        for (int e = 0; e < 4; ++e) {
            if (!values[e]) continue;
            char text[64] = ""; const char *material = NULL;
            float h = line, w = 0, textScale = scale;
            float color[4] = {1,1,1,.85f};
            if (e == 0) {
                if (!c->nextState.iHeadIcon || (c->nextState.iHeadIconTeam && viewTeam != 3 && viewTeam != c->nextState.iHeadIconTeam)) continue;
                material = CL_GetConfigString(c->nextState.iHeadIcon + 30);
                float r = (GameFloat("cg_scriptIconSize", 0)+10)*factor;
                const dvar_t *fixed = Dvar_FindVar("cg_constantSizeHeadIcons");
                h = fixed && fixed->type == 0 && fixed->current.enabled ? r*.0043f*480*2 : fmaxf(r/(2*forward*tanf(cg->refdef.fov_y*(float)M_PI/360.0f)),factor*GameFloat("cg_headIconMinScreenRadius",.02f))*480*2;
                w = h;
            } else if (e == 1) {
                SV_Cod2xStripTextCodes(ci->name, text, sizeof(text));
                memcpy(color, settings.color[ci->team & 3], sizeof(color));
            } else if (e == 2) {
                if (!def) continue;
                material = def->killIcon; h = weaponHeight; w = h * (def->wideKillIcon ? 2 : 1);
                if (!material || !material[0]) { material = def->szHudIcon; w = h; }
            } else {
                if (!def || !def->szDisplayName || !def->szDisplayName[0]) continue;
                SV_Cod2xStripTextCodes(SEH_LocalizeTextMessage(def->szDisplayName,"weapon name",0),text,sizeof(text));
                h *= .8f; textScale *= .8f;
            }
            if (e == 1 || e == 3) w = UI_TextWidth(text, INT_MAX, font, textScale);
            if ((material && !material[0]) || h <= 0) continue;
            if (hidden && values[e] == 2) color[3] *= FadeBox(x,y,w,h);
            int draw = 1;
#if COD2X_XRAY_RENDER
            if (values[e] == 1 && depthAvailable) draw = R_AddCmdCod2xHead(1, viewPoint);
#endif
            if (values[e] == 1 && !depthAvailable && hidden) draw = 0;
            if (draw) {
                if (material) CG_DrawRotatedPic(x-w*.5f,y-h,w,h,2,2,0,color,CL_RegisterMaterial(material,7));
                else UI_DrawText(text,INT_MAX,font,x-w*.5f,y,2,2,textScale,color,3);
#if COD2X_XRAY_RENDER
                if (values[e] == 1 && depthAvailable) R_AddCmdCod2xHead(0,NULL);
#endif
            }
            y -= h + (e == 0 ? 2 : 1);
        }
    }
}
#endif
