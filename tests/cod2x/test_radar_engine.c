/* Exercise the production radar reader and emitted radar draw commands. */
#include "../../src/PC/cgame_mp/cg_cod2x_radar.c"
#ifndef SPECTATOR_SOURCE
#define SPECTATOR_SOURCE "../../src/PC/cgame_mp/cg_cod2x_spectator.c"
#endif
#include SPECTATOR_SOURCE
#include <assert.h>

unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
cg_t cgArray[1];
static centity_t entities[1024];
centity_t *cg_entities = entities;
clientActive_t clients;
clientStatic_t cls;
sharedUiInfo_t sharedUiInfo;
clientConnection_t clientConnections;
scr_const_t scr_const;
static dvar_t spectatorDvars[40];
static char spectatorNames[40][64];
static int spectatorDvarCount, demoPlaying, headDepthAvailable, headCommands, headTraces, textDraws;
static float headTraceFraction = 1;
static FontHandle drawnFont;
static char drawnText[64];
static dvar_t gameRevision, cheatsEnabled;
static dvar_t radarAllies = { .type = 7 }, radarAxis = { .type = 7 };
static int radarTeamColors;
static WeaponDef heldWeapons[3];
static dvar_t image;
static char uiMap[64];
static int result, allocations, frees, drops, reads;
static Material materials[5];
static struct { float x, y, w, h, angle, alpha, rgb[3]; MaterialHandle material; } draws[256];
static int drawCount;

int Cod2x_FeaturesDemo(void) { return demoPlaying; }
/* The revision this build runs as. X-ray is tested as the build that has it (7); one block
   below checks that a build running as 1.4.6.8 shows none in a demo or with cheats on. */
static int buildRevision = 7;
int Cod2x_HostRevision(void) { return buildRevision; }
const dvar_t *Dvar_FindVar(const char *name)
{
    if (!strcmp(name, "g_cod2x")) return &gameRevision;
    if (!strcmp(name, "sv_cheats")) return &cheatsEnabled;
    if (radarTeamColors && !strcmp(name, "g_TeamColor_Allies")) return &radarAllies;
    if (radarTeamColors && !strcmp(name, "g_TeamColor_Axis")) return &radarAxis;
    for (int i = 0; i < spectatorDvarCount; ++i)
        if (!strcmp(name, spectatorDvars[i].name)) return &spectatorDvars[i];
    return NULL;
}
static dvar_t *register_head_dvar(const char *name, int type, int flags)
{
    assert(spectatorDvarCount < 40);
    int index = spectatorDvarCount++;
    snprintf(spectatorNames[index], sizeof(spectatorNames[index]), "%s", name);
    /* Internal engine dvars retain the supplied name pointer. */
    spectatorDvars[index].name = name;
    spectatorDvars[index].type = type;
    spectatorDvars[index].flags = flags;
    return &spectatorDvars[index];
}
const dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, unsigned short flags)
{
    dvar_t *d = register_head_dvar(name, 5, flags);
    d->current.integer = value; d->domain.integer.min = min; d->domain.integer.max = max;
    return d;
}
const dvar_t *Dvar_RegisterBool(const char *name, unsigned char value, unsigned short flags)
{
    dvar_t *d = register_head_dvar(name, 0, flags); d->current.enabled = value; return d;
}
const dvar_t *Dvar_RegisterFloat(const char *name, float value, float min, float max, unsigned short flags)
{
    dvar_t *d = register_head_dvar(name, 1, flags);
    d->current.value = value; d->domain.value.min = min; d->domain.value.max = max;
    return d;
}
const dvar_t *Dvar_RegisterString(const char *name, const char *value, unsigned short flags)
{
    dvar_t *d = register_head_dvar(name, 7, flags); d->current.string = value; return d;
}
struct DObj_s *Com_GetClientDObj(int number, int local) { (void)number; (void)local; return NULL; }
qboolean CG_DObjGetWorldTagPos(const centity_t *c, const struct DObj_s *o, unsigned int tag, float *point)
{ (void)c; (void)o; (void)tag; (void)point; return 0; }
void CG_TraceCapsule(trace_t *trace, const float *start, const float *min, const float *max,
                     const float *end, int skip, int contents)
{
    (void)start; (void)min; (void)max; (void)end; (void)skip;
    assert(contents == 0x811); ++headTraces; trace->fraction = headTraceFraction;
}
int BG_GetNumWeapons(void) { return 2; }
WeaponDef *BG_GetWeaponDef(int weapon) { assert(weapon > 0 && weapon <= 2); return &heldWeapons[weapon]; }
const char *SEH_LocalizeTextMessage(const char *text, const char *message, int flags)
{ (void)message; (void)flags; return text; }
void SV_Cod2xStripTextCodes(const char *text, char *out, size_t size)
{ snprintf(out, size, "%s", text); }
int R_Cod2xHeadAvailable(void) { return headDepthAvailable; }
int R_AddCmdCod2xHead(int operation, const float *point)
{ assert(operation == 0 || point); ++headCommands; return 1; }
FontHandle UI_GetFontHandle(int type, float scale) { (void)type; (void)scale; return NULL; }
void Dvar_GetUnpackedColor(const dvar_t *var, vec_t *out) {
    if (var == &radarAllies) { out[0]=.2f; out[1]=.3f; out[2]=.4f; out[3]=.5f; }
    if (var == &radarAxis) { out[0]=.6f; out[1]=.7f; out[2]=.8f; out[3]=.9f; }
}
const char *CL_GetConfigString(int index) { (void)index; return ""; }
const char *UI_Cod2xRadarMapName(void) { return uiMap; }
MaterialHandle CL_RegisterMaterial(const char *name, int flags)
{
    (void)flags;
    if (!strcmp(name, "radar_player_fire")) return &materials[1];
    if (!strcmp(name, "radar_player_arrow")) return &materials[2];
    if (!strcmp(name, "radar_player_circle")) return &materials[3];
    return &materials[4];
}
void CG_DrawRotatedPic(float x, float y, float w, float h, int ha, int va,
                      float angle, const vec_t *color, MaterialHandle material)
{
    (void)ha; (void)va;
    assert(drawCount < 256);
    draws[drawCount].x=x; draws[drawCount].y=y; draws[drawCount].w=w; draws[drawCount].h=h;
    memcpy(draws[drawCount].rgb, color, sizeof(draws[drawCount].rgb));
    draws[drawCount].angle=angle; draws[drawCount].alpha=color[3]; draws[drawCount].material=material;
    ++drawCount;
}
void UI_DrawText(const char *text, int max, FontHandle font, float x, float y,
                 int ha, int va, float scale, const vec_t *color, int style)
{ (void)max; (void)x; (void)y; (void)ha; (void)va; (void)scale; (void)color; (void)style;
  ++textDraws; drawnFont = font; snprintf(drawnText, sizeof(drawnText), "%s", text); }
int UI_TextWidth(const char *text, int max, FontHandle font, float scale)
{ (void)text; (void)max; (void)font; (void)scale; return 10; }
int UI_TextHeight(FontHandle font, float scale) { (void)font; (void)scale; return 8; }

int FS_ReadFile(const char *name, void **buffer)
{
    assert(strstr(name, "maps/mp/") == name);
    ++reads;
    if (result < 0) {
        *buffer = NULL;
        return -1;
    }
    *buffer = calloc(1, (size_t)result + 1);
    assert(*buffer);
    ++allocations;
    return result;
}

void FS_FreeFile(void *buffer)
{
    assert(buffer);
    ++frees;
    free(buffer);
}

void Dvar_SetString(const dvar_t *var, const char *value)
{
    ((dvar_t *)var)->current.string = value;
}
void Dvar_SetInt(const dvar_t *var, int value) { (void)var; (void)value; }
void Dvar_SetFloat(const dvar_t *var, float value) { (void)var; (void)value; }
char *Com_Parse(const char **cursor) { (void)cursor; return ""; }
void Com_Printf(const char *format, ...) { (void)format; }
void Com_Error(errorParm_t code, const char *format, ...)
{
    assert(code == ERR_DROP && strstr(format, "Invalid image name"));
    ++drops;
}

static int draw_at(int time, MaterialHandle material)
{
    cg->time = time;
    drawCount = 0;
    CG_Cod2xRadarDraw();
    for (int i = 0; i < drawCount; ++i)
        if (draws[i].material == material)
            return i;
    return -1;
}

static void firing_flashes(void)
{
    static dvar_t enabled, scale, ix, iy, angle, es, ex, ey, rx, ry, numbers, tint;
    static vec4_t tintValue = {1, 1, 1, 1};
    static snapshot_t snapshot;
    drawRadar=&enabled; radarScale=&scale; imageX=&ix; imageY=&iy; rotation=&angle;
    entityScale=&es; entityX=&ex; entityY=&ey; radarX=&rx; radarY=&ry;
    numberSwitch=&numbers; radarColor=&tint;
    tint.current.vector=tintValue;
    enabled.current.enabled=1; scale.current.value=es.current.value=1;
    image.current.string="map";
    snprintf(radarMap, sizeof(radarMap), "fixture");
    snprintf(uiMap, sizeof(uiMap), "fixture");
    snprintf(cgs->mapname, sizeof(cgs->mapname), "maps/mp/fixture.d3dbsp");
    cgs->media.objectiveMaterials[0]=&materials[0];
    cg->snap=&snapshot; cg->clientNum=63;
    cg->bgs.clientinfo[63].team=TEAM_SPECTATOR;
    cg->bgs.clientinfo[0].team=TEAM_ALLIES; cg->bgs.clientinfo[0].infoValid=1;
    entities[0].currentState.eType=1; entities[0].nextValid=1;
    entities[0].currentState.pos.trBase[0]=20;
    entities[0].currentState.pos.trBase[1]=30;
    entities[0].nextState.apos.trBase[1]=25;
    snapshot.ps.objective[0].state=OBJST_CURRENT; snapshot.ps.objective[0].entNum=1023;
    snapshot.ps.clientNum=63;
    /* Current snapshot membership controls inclusion; position is current,
       heading is next-state, and only exact qtrue client info is drawn. */
    entities[0].nextState.pos.trBase[0] = 900;
    entities[0].currentState.apos.trBase[1] = 170;
    for (int valid = 0; valid < 4; ++valid) {
        entities[0].nextValid = valid;
        int arrow = draw_at(0, &materials[2]);
        assert((arrow >= 0) == ((valid & 1) != 0));
        if (arrow >= 0) {
            assert(draws[arrow].angle == 65 && draws[arrow].w == 20);
            assert(draws[arrow].x == 30 && draws[arrow].y == 130);
            assert(draws[arrow+1].rgb[0] == 1 && draws[arrow+1].alpha == 1);
        }
    }
    entities[0].nextValid = 1; cg->bgs.clientinfo[0].infoValid = 2;
    assert(draw_at(0, &materials[2]) == -1);
    cg->bgs.clientinfo[0].infoValid = 1;
    snapshot.ps.clientNum = 0;
    int followed = draw_at(0, &materials[2]);
    assert(followed >= 0 && draws[followed].w == 30 && draws[followed+1].w == 24);
    assert(draws[followed].rgb[0] == 1 && draws[followed].rgb[1] == 1 && draws[followed].rgb[2] == 0);
    radarTeamColors = 1;
    followed = draw_at(0, &materials[2]);
    assert(draws[followed+1].rgb[0] == .2f && draws[followed+1].rgb[1] == .3f && draws[followed+1].rgb[2] == .4f && draws[followed+1].alpha == 1);
    cg->bgs.clientinfo[0].team = TEAM_AXIS;
    followed = draw_at(0, &materials[2]);
    assert(draws[followed+1].rgb[0] == .6f && draws[followed+1].rgb[1] == .7f && draws[followed+1].rgb[2] == .8f && draws[followed+1].alpha == 1);
    cg->bgs.clientinfo[0].team = TEAM_ALLIES; radarTeamColors = 0;
    snapshot.ps.clientNum = 63;
    cg->compassActors[0].beginFadeTime=500;
    assert(draw_at(100, &materials[1]) == -1);
    int flash=draw_at(101, &materials[1]);
    printf("first observed compass fire: emitted flash=%d\n",flash>=0); fflush(stdout);
    assert(flash>=0 && draws[flash-1].material==&materials[0]);
    float x=draws[flash].x, y=draws[flash].y, w=draws[flash].w, heading=draws[flash].angle;
    rx.current.value=100; scale.current.value=2; angle.current.integer=90;
    flash=draw_at(102, &materials[1]);
    assert(flash>=0 && draws[flash].x==x && draws[flash].y==y && draws[flash].w==w && draws[flash].angle==heading);
    printf("flash fixed after radar offset/scale/rotation: %.3f %.3f %.3f %.3f\n",x,y,w,heading);

    /* Existing events survive time rewind; negative ages are simply hidden. */
    assert(draw_at(50, &materials[1])==-1);
    assert(draw_at(103, &materials[1])>=0);

    /* While hidden, firing is observed only when a player is next drawn. */
    enabled.current.enabled=0; cg->compassActors[0].beginFadeTime=900;
    assert(draw_at(200, &materials[1])==-1);
    enabled.current.enabled=1;
    draw_at(300, &materials[1]);
    assert(fireEvents[1].time==300);
    cg->compassActors[0].beginFadeTime=1000; entities[0].nextValid=2;
    draw_at(301, &materials[1]);
    assert(fireEvents[2].time==0);
    entities[0].nextValid=1; draw_at(302, &materials[1]);
    assert(fireEvents[2].time==302);

    /* First expired slot, strict >1000 boundary, then slot-zero fallback. */
    for(int i=0;i<128;++i) fireEvents[i].time=100;
    fireEvents[1].time=99;
    cg->compassActors[0].beginFadeTime=1001; draw_at(1100, &materials[1]);
    assert(fireEvents[0].time==100 && fireEvents[1].time==1100);
    for(int i=0;i<128;++i) fireEvents[i].time=200;
    cg->compassActors[0].beginFadeTime=1002; draw_at(300, &materials[1]);
    assert(fireEvents[0].time==300 && fireEvents[1].time==200 && fireEvents[127].time==200);

    /* Calibration uses the full 400-square map at the screen origin. */
    calibration=1;
    rx.current.value=100; ry.current.value=20;
    ix.current.value=12; iy.current.value=5;
    int map=draw_at(301,&materials[4]);
    assert(map>=0);
    printf("calibration map with offsets: %.3f %.3f %.3f %.3f angle=%.3f\n",
           draws[map].x,draws[map].y,draws[map].w,draws[map].h,draws[map].angle); fflush(stdout);
    assert(draws[map].x==0 && draws[map].y==0 && draws[map].w==400 &&
           draws[map].h==400 && draws[map].angle==0);

    /* Each calibration draw consumes the recorded mouse deltas, even at the same time. */
    cls.realtime=7000;
    for (int point=0; point<2; ++point) {
        calibration=2*(point+1);
        clients.mouseIndex=point;
        clients.mouseDx[point^1]=30;
        clients.mouseDy[point^1]=-20;
        imagePoints[point][0]=100;
        imagePoints[point][1]=200;
        draw_at(302,&materials[4]);
        assert(imagePoints[point][0]==103 && imagePoints[point][1]==198);
        draw_at(302,&materials[4]);
        printf("calibration point%d after two draws at realtime%d: %.3f %.3f\n",
               point+1,cls.realtime,imagePoints[point][0],imagePoints[point][1]); fflush(stdout);
        assert(imagePoints[point][0]==106 && imagePoints[point][1]==196);
        assert(draws[drawCount-1].x==105 && draws[drawCount-1].y==195);
    }

    /* The upstream event list and observed sound times outlive map changes. */
    snprintf(uiMap,sizeof(uiMap),"another"); result=-1;
    calibration=3;
    CG_Cod2xRadarFrame();
    draw_at(301,&materials[1]);
    assert(fireEvents[0].time==300 && calibration==3);
    puts("CoD2x radar draw commands: saved geometry, objective ordering, eligibility, reuse and lifecycle pass");
}

static void spectator_draw(void)
{
    drawCount = textDraws = headTraces = headCommands = 0;
    CG_Cod2xHeadDraw();
}

static void spectator_controls(void)
{
    static const struct { const char *name; float value, min, max; } floats[] = {
        {"cg_xrayCenterFadeRadius",80,0,320}, {"cg_xrayCenterFadeAlpha",.15f,0,1},
        {"cg_xrayHiddenThreshold",24,0,256}, {"cg_xrayOutlineWidth",1.5f,.5f,4},
        {"cg_xrayGlowIntensity",1,0,4}, {"cg_xrayGlowCoreIntensity",.5f,0,2},
        {"cg_xrayHealthAlpha",.8f,0,1}, {"cg_headNameScale",1,.3f,3},
        {"cg_headWeaponIconScale",1,0,3}, {"cg_headIconScale",1,0,10},
        {"cg_headOffset",0,-64,128}, {"cg_headMaxDistance",0,0,10000}
    };
    CG_Cod2xSpectatorInit();
    assert(spectatorDvarCount == 33);
    for (int i = 0; i < spectatorDvarCount; ++i) {
        const dvar_t *d = &spectatorDvars[i];
        assert(Dvar_FindVar(spectatorNames[i]) == d);
        assert(d->flags == (!strcmp(spectatorNames[i], "cg_xray") ?
#if DEBUG
                           0x1000 :
#else
                           0x1080 :
#endif
                           0x1000));
    }
    for (unsigned i = 0; i < sizeof(floats)/sizeof(floats[0]); ++i) {
        const dvar_t *d = Dvar_FindVar(floats[i].name);
        assert(d && d->type == 1 && d->current.value == floats[i].value);
        assert(d->domain.value.min == floats[i].min && d->domain.value.max == floats[i].max);
    }
    const int defaults[3][4] = {{0,2,2,0},{1,2,2,0},{1,0,0,0}};
    for (int s = 0; s < 3; ++s) for (int e = 0; e < 4; ++e) {
        assert(head[s][e]->current.integer == defaults[s][e]);
        assert(head[s][e]->type == 5 && head[s][e]->domain.integer.min == 0 && head[s][e]->domain.integer.max == 2);
    }
    assert(style->current.integer == 0 && style->domain.integer.min == 0 && style->domain.integer.max == 4);
    for (int i = 0; i < 2; ++i) {
        assert(stateStyle[i]->current.integer == 2 && stateStyle[i]->domain.integer.min == 0 && stateStyle[i]->domain.integer.max == 2);
        assert(!visible[i]->current.enabled && teamColor[i]->current.string[0] == 0);
    }
    assert(health->current.enabled && healthColor->current.string[0] == 0);
    puts("CoD2x spectator: 33 control names, defaults, domains and flags pass");
}

static void spectator_states(void)
{
    static snapshot_t snap;
    memset(cg, 0, sizeof(*cg)); memset(entities, 0, sizeof(entities));
    cg->snap = cg->nextSnap = &snap; cg->clientNum = 0;
    clientConnections.state = CA_ACTIVE;
    for (int rev = 0; rev <= 7; rev += rev == 0 ? 6 : 1)
        for (int demo = 0; demo <= 1; ++demo)
            for (int cheat = 0; cheat <= 1; ++cheat)
                for (int team = 0; team < 4; ++team)
                    for (int following = 0; following < 3; ++following) {
                        gameRevision.current.integer = rev; demoPlaying = demo; cheatsEnabled.current.enabled = cheat;
                        cg->bgs.clientinfo[0].team = team;
                        snap.ps.clientNum = following == 1 ? 1 : 0;
                        snap.ps.pm_flags = following == 2 ? 0x1000000 : 0;
                        CG_Cod2xSpectatorFrame();
                        int allowed = rev >= 7 || demo || cheat;
#if DEBUG
                        allowed = 1;
#endif
                        int expected = !allowed ? -1 : demo || (team == 3 && following) ? 0 : following ? 1 : 2;
                        assert(viewer == expected);
                        assert(settings.mode == (expected == 0 || expected == 1 ? 4 : 0));
                        if (expected >= 0) for (int target = 0; target < 4; ++target)
                            assert(teams[target] == (target != 3 && (expected == 0 || (target && target == team))));
                    }
    demoPlaying = 0; cheatsEnabled.current.enabled = 0; gameRevision.current.integer = 7;
    cg->bgs.clientinfo[0].team = 1; snap.ps.clientNum = 0; snap.ps.pm_flags = 0;
    for (int mode = 0; mode < 5; ++mode) {
        ((dvar_t *)style)->current.integer = mode;
        cheatsEnabled.current.enabled = 1; CG_Cod2xSpectatorFrame();
        assert(settings.mode == mode);
        if (mode) assert(teams[0] && teams[1] && teams[2] && !teams[3]);
    }
    ((dvar_t *)style)->current.integer = 0; cheatsEnabled.current.enabled = 0;
    cg->bgs.clientinfo[0].team = 3; snap.ps.clientNum = 1;
    CG_Cod2xSpectatorFrame();
    Cod2xPlayerInfo info;
    assert(!CG_Cod2xPlayerInfo(-1, &info) && !CG_Cod2xPlayerInfo(64, &info));
    for (int i = 0; i < 64; ++i) {
        entities[i].nextValid = 1; entities[i].currentState.eType = 1;
        entities[i].currentState.dmgFlags = (int)0xC10D648B; entities[i].currentState.surfType = 100;
        assert(CG_Cod2xPlayerInfo(i, &info));
        assert(info.health == 100 && info.primary == 11 && info.secondary == 73 && info.frag == 5 && info.smoke == 3);
    }
    entities[2].currentState.dmgFlags = (int)0xC1FFFFFF; entities[2].currentState.surfType = 0x1ff;
    assert(CG_Cod2xPlayerInfo(2, &info) && info.health == 255 && info.primary == 127 && info.secondary == 127 && info.frag == 15 && info.smoke == 15);
    entities[2].currentState.dmgFlags = (int)0xC2000000; assert(!CG_Cod2xPlayerInfo(2,&info));
    entities[2].currentState.dmgFlags = (int)0xC1000000; entities[2].nextValid = 2; assert(!CG_Cod2xPlayerInfo(2,&info));
    entities[2].nextValid = 1; entities[2].currentState.eType = 2; assert(!CG_Cod2xPlayerInfo(2,&info));
    entities[2].currentState.eType = 1;
    /* Running as 1.4.6.8, which has no x-ray: a demo and sv_cheats show none. */
    buildRevision = 6; demoPlaying = 1; gameRevision.current.integer = 0; CG_Cod2xSpectatorFrame();
    assert(!settings.mode && !settings.health && !settings.visible);
    demoPlaying = 0; cheatsEnabled.current.enabled = 1; CG_Cod2xSpectatorFrame();
    assert(!settings.mode && !settings.health && !settings.visible);
    cheatsEnabled.current.enabled = 0; buildRevision = 7;
    /* Demo player data survives missing snapshots until the connection leaves active. */
    demoPlaying = 1; gameRevision.current.integer = 0; CG_Cod2xSpectatorFrame(); assert(settings.health);
    memset(entities, 0, sizeof(entities)); CG_Cod2xSpectatorFrame(); assert(settings.health);
    clientConnections.state = CA_LOADING; CG_Cod2xSpectatorFrame();
    clientConnections.state = CA_ACTIVE; CG_Cod2xSpectatorFrame(); assert(!settings.health);
    puts("CoD2x spectator: 144 viewer combinations, 64 decoded slots, signature and demo reset pass");

    cg->refdef.width = 640; cg->refdef.height = 480; cg->refdef.fov_x = 90; cg->refdef.fov_y = 60;
    cg->refdef.viewaxis[0][0] = cg->refdef.viewaxis[1][1] = cg->refdef.viewaxis[2][2] = 1;
    centity_t *c = &entities[2];
    c->currentState.eType = 1; c->nextValid = 1;
    c->currentState.clientNum = c->nextState.clientNum = c->nextState.number = 2;
    c->lerpOrigin[0] = 100; cg->bgs.clientinfo[2].infoValid = 1; cg->bgs.clientinfo[2].team = 1;
    strcpy(cg->bgs.clientinfo[2].name, "spectator target");
    sharedUiInfo.assets.textFont = (FontHandle)&materials[0];
    ((dvar_t *)head[0][2])->current.integer = 0;
    headDepthAvailable = 1; headTraceFraction = 0;
    spectator_draw(); assert(textDraws == 1 && headTraces == 1 && headCommands == 0);
    printf("spectator fixed font: actual=%p expected=%p\n", (void *)drawnFont, (void *)sharedUiInfo.assets.textFont); fflush(stdout);
    assert(drawnFont == sharedUiInfo.assets.textFont);
    assert(CG_Cod2xHeadHidesName(2) && CG_Cod2xHeadHidesNation(c));
    demoPlaying = 0; gameRevision.current.integer = 7; cg->bgs.clientinfo[0].team = 2; CG_Cod2xSpectatorFrame();
    assert(!CG_Cod2xHeadHidesName(2) && !CG_Cod2xHeadHidesNation(c));
    cg->bgs.clientinfo[0].team = 1; CG_Cod2xSpectatorFrame();
    assert(CG_Cod2xHeadHidesName(2));
    cg->bgs.clientinfo[0].team = 0; CG_Cod2xSpectatorFrame();
    assert(!CG_Cod2xHeadHidesName(2));
    demoPlaying = 1; CG_Cod2xSpectatorFrame();
    assert(fabsf(FadeBox(0, 0, 10, 10) - .15f) < .000001f);
    assert(fabsf(FadeBox(60, 0, 40, 10) - .575f) < .000001f);
    assert(FadeBox(100, 0, 40, 10) == 1);
    ((dvar_t *)maxDistance)->current.value = 99; spectator_draw(); assert(!textDraws && !CG_Cod2xHeadHidesName(2));
    ((dvar_t *)maxDistance)->current.value = 100;
    ((dvar_t *)head[0][1])->current.integer = 1;
    spectator_draw(); assert(textDraws == 1 && headTraces == 0 && headCommands == 2);
    headDepthAvailable = 0;
    spectator_draw(); assert(textDraws == 0 && headTraces == 1 && headCommands == 0);
    headTraceFraction = .99f; spectator_draw(); assert(textDraws == 1 && headTraces == 1 && headCommands == 0);
    c->currentState.eFlags = 0x20000; spectator_draw(); assert(!textDraws);
    c->currentState.eFlags = 0; cg->bgs.clientinfo[2].infoValid = 0; spectator_draw(); assert(!textDraws);
    cg->bgs.clientinfo[2].infoValid = 1; cg->bgs.clientinfo[2].team = 3; spectator_draw(); assert(!textDraws);
    cg->bgs.clientinfo[2].team = 1;
    ((dvar_t *)head[0][1])->current.integer = 0; ((dvar_t *)head[0][0])->current.integer = 1;
    assert(!CG_Cod2xHeadHidesNation(c));
    ((dvar_t *)head[0][0])->current.integer = 0;
    ((dvar_t *)head[0][3])->current.integer = 2;
    heldWeapons[1].szDisplayName = "rifle"; heldWeapons[2].szDisplayName = "grenade";
    c->currentState.weapon = 1; cg->time = 100;
    spectator_draw(); assert(textDraws == 1 && !strcmp(drawnText, "rifle"));
    c->nextState.eventParm = 2; CG_Cod2xHeadEvent(c, 0xa5);
    spectator_draw(); assert(textDraws == 1 && !strcmp(drawnText, "grenade"));
    cg->time = 5100; spectator_draw(); assert(textDraws == 1 && !strcmp(drawnText, "rifle"));
    c->currentState.eFlags = 0x4000; spectator_draw(); assert(!textDraws);
    c->currentState.eFlags = 0; cg->bgs.clientinfo[2].clientConditions[3][0] = 1<<18; spectator_draw(); assert(!textDraws);
    cg->bgs.clientinfo[2].clientConditions[3][0] = 0;
    /* Health bit level: standing 50% = 14, crouched = 10, prone = 4. */
    gameRevision.current.integer = 7; CG_Cod2xSpectatorFrame();
    c->currentState.dmgFlags = (int)0xC1000000; c->currentState.surfType = 50;
    const int flags[] = {0,4,8}, levels[] = {14,10,4};
    for (int i = 0; i < 3; ++i) {
        GfxEntity entity = {0}; c->nextState.eFlags = flags[i]; CG_Cod2xTagPlayer(&entity, NULL, c);
        assert((entity.renderFxFlags & COD2X_XRAY_TAG) && ((unsigned)entity.renderFxFlags >> COD2X_XRAY_HEALTH_SHIFT) == levels[i]);
    }
    GfxEntity entity = {0}; c->currentState.surfType = 100; CG_Cod2xTagPlayer(&entity, NULL, c);
    assert(((unsigned)entity.renderFxFlags >> COD2X_XRAY_HEALTH_SHIFT) == 0);
    memset(&entity, 0, sizeof(entity)); c->nextState.eFlags = 0;
    c->nextState.clientNum = snap.ps.clientNum; snap.ps.stats[0] = 25;
    CG_Cod2xTagPlayer(&entity, NULL, c);
    assert(((unsigned)entity.renderFxFlags >> COD2X_XRAY_HEALTH_SHIFT) == 7);
    puts("CoD2x spectator draw commands: font, distance, depth fallback, dead/spectator suppression, held weapons and health bits pass");
}

int main(void)
{
    mapImage = &image;
    snprintf(uiMap, sizeof(uiMap), "missing");
    result = -1;
    calibration=2;
    CG_Cod2xRadarFrame();
    printf("radar frame without snapshot: reads=%d calibration=%d\n",reads,calibration); fflush(stdout);
    assert(reads==1 && calibration==2 && allocations == 0 && frees == 0);
    snprintf(uiMap, sizeof(uiMap), "empty");
    result = 0;
    CG_Cod2xRadarFrame();
    assert(allocations == 1 && frees == 1);
    snprintf(uiMap, sizeof(uiMap), "invalid");
    result = 8;
    CG_Cod2xRadarFrame();
    assert(allocations == 2 && frees == 2 && drops == 1);
    CG_Cod2xRadarFrame();
    assert(reads==3 && allocations == 2 && frees == 2 && drops == 1);
    uiMap[0]='\0'; image.current.string="map";
    CG_Cod2xRadarFrame();
    assert(!*image.current.string && calibration==2 && reads==3);
    calibration=0;
    puts("CoD2x production radar reader buffer ownership: pass");
    firing_flashes();
    spectator_controls();
    spectator_states();
    return 0;
}
