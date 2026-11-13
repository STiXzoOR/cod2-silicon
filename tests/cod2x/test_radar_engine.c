/* Exercise the production radar reader and emitted radar draw commands. */
#include "../../src/PC/cgame_mp/cg_cod2x_radar.c"
#include <assert.h>

unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
cg_t cgArray[1];
static centity_t entities[1024];
centity_t *cg_entities = entities;
clientActive_t clients;
clientStatic_t cls;
sharedUiInfo_t sharedUiInfo;
static dvar_t image;
static char uiMap[64];
static int result, allocations, frees, drops, reads;
static Material materials[5];
static struct { float x, y, w, h, angle, alpha; MaterialHandle material; } draws[256];
static int drawCount;

int Cod2x_FeaturesDemo(void) { return 0; }
const dvar_t *Dvar_FindVar(const char *name) { (void)name; return NULL; }
void Dvar_GetUnpackedColor(const dvar_t *var, vec_t *out) { (void)var; (void)out; }
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
    draws[drawCount].angle=angle; draws[drawCount].alpha=color[3]; draws[drawCount].material=material;
    ++drawCount;
}
void UI_DrawText(const char *text, int max, FontHandle font, float x, float y,
                 int ha, int va, float scale, const vec_t *color, int style)
{ (void)text; (void)max; (void)font; (void)x; (void)y; (void)ha; (void)va; (void)scale; (void)color; (void)style; }
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

    /* The upstream event list and observed sound times outlive map changes. */
    snprintf(uiMap,sizeof(uiMap),"another"); result=-1;
    calibration=3;
    CG_Cod2xRadarFrame();
    draw_at(301,&materials[1]);
    assert(fireEvents[0].time==300 && calibration==3);
    puts("CoD2x radar draw commands: saved geometry, objective ordering, eligibility, reuse and lifecycle pass");
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
    return 0;
}
