#include "../../src/PC/qcommon/cod2x_pose.c"
#include <assert.h>

static dvar_t variables[16], cheats, running;
static vec3_t vectors[16];
const dvar_t *com_sv_running = &running;
static int count, lines, debugStrings, demo, logs, expectedServer;
static int greenControllers, redControllers;
static vec4_t lastCrossColor;
static vec3_t lastCrossStart;
static bgs_t poseWorld;
bgs_t *bgs = &poseWorld;
animStringItem_t animMoveTypesStr[44];
static gentity_t serverEntity;
static gclient_t serverClient;
void Com_Printf(const char *format, ...) { assert(strstr(format, "Controller movement")); ++logs; }
void AngleVectors(const vec_t *angles, vec_t *forward, vec_t *right, vec_t *up) {
    (void)angles; assert(!up); forward[0] = 1; forward[1] = forward[2] = 0;
    right[0] = right[2] = 0; right[1] = -1;
}
const float AngleSubtract(const float a, const float b) { return remainderf(a - b, 360); }
float GetLeanFraction(const float value) { return value; }
static playerState_t player;

const dvar_t *Dvar_FindVar(const char *name) { assert(!strcmp(name, "sv_cheats")); return &cheats; }
int Cod2x_FeaturesDemo(void) { return demo; }
playerState_t *SV_GameClientNum(int num) { assert(num == 0); return &player; }
gentity_t *SV_GentityNum(int num) { assert(num == 0); return &serverEntity; }
void G_GetPlayerViewOrigin(const gentity_t *entity, vec_t *origin) { assert(entity == &serverEntity); origin[0] = 10; origin[1] = 20; origin[2] = 30; }

static dvar_t *add(const char *name, unsigned short flags)
{
    dvar_t *var = &variables[count++];
    assert(count <= 16 && flags == 0x1080);
    var->name = name;
    var->flags = flags;
    return var;
}
const dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, unsigned short flags)
{
    dvar_t *var = add(name, flags);
    assert(min == 0 && max == 2);
    var->current.integer = value;
    return var;
}
const dvar_t *Dvar_RegisterBool(const char *name, unsigned char value, unsigned short flags)
{
    dvar_t *var = add(name, flags);
    var->current.enabled = value;
    return var;
}
const dvar_t *Dvar_RegisterVec3(const char *name, float x, float y, float z, float min, float max, unsigned short flags)
{
    dvar_t *var = add(name, flags);
    assert(min == -FLT_MAX && max == FLT_MAX);
    var->type = 3;
    var->current.vector = vectors[var - variables];
    var->current.vector[0] = x; var->current.vector[1] = y; var->current.vector[2] = z;
    return var;
}
void CL_AddDebugLine(const vec_t *start, const vec_t *end, const vec_t *color, qboolean depth, int duration, qboolean server)
{
    assert(depth == 1 && duration == 0 && color[0] == 1 && server == 0);
    assert(end[lines % 3] - start[lines % 3] == 6);
    memcpy(lastCrossColor, color, sizeof(lastCrossColor));
    memcpy(lastCrossStart, start, sizeof(lastCrossStart));
    ++lines;
}
void CL_AddDebugString(const vec_t *origin, const vec_t *color, float scale, const char *text, qboolean server)
{
    (void)origin; (void)color;
    assert((scale == .25f || scale == .3f) && *text && server == expectedServer);
    if (strstr(text, " -> ")) {
        if (color[1] == 1) ++greenControllers;
        else if (color[0] == 1) ++redControllers;
    }
    ++debugStrings;
}

int main(void)
{
    vec3_t goals[8] = {{0}};
    entityState_t entity = {0};
    clientInfo_t info = {0};
    Cod2x_PoseInit();
    assert(count == 12);
    ((dvar_t *)offsetEnable)->current.enabled = 1;
    ((dvar_t *)offsets[7])->current.vector[2] = 6;
    Cod2x_PoseOffsets(goals);
    assert(goals[7][2] == 6);
    cheats.current.enabled = 1;
    Cod2x_PoseOffsets(goals);
    assert(goals[7][2] == 12 && goals[6][2] == 0);
    ((dvar_t *)neutral)->current.enabled = 1;
    assert(!Cod2x_PoseNeutral());
    running.current.enabled = 1;
    entity.eType = 2;
    entity.torsoAnim = player.torsoAnim = 42;
    entity.legsAnim = player.legsAnim = 0x211;
    Cod2x_PoseReset(&entity);
    assert(entity.torsoAnim == 0x200 && player.torsoAnim == 0x200 && entity.legsAnim == 0 && player.legsAnim == 0);
    entity.torsoAnim = player.torsoAnim = 42; entity.legsAnim = player.legsAnim = 0x211;
    entity.eType = 1;
    Cod2x_PoseReset(&entity);
    assert(entity.torsoAnim == 0x200 && player.torsoAnim == 0x200);
    assert(entity.legsAnim == 0 && player.legsAnim == 0);
    Cod2x_PoseReset(&entity);
    assert(entity.torsoAnim == 0 && entity.legsAnim == 0x200);
    ((dvar_t *)debug)->current.integer = 1;
    Cod2x_PoseDiagnostics(&entity, &info, goals, 0, 3);
    assert(lines == 3 && debugStrings == 24);
    assert(greenControllers == 7 && redControllers == 1);
    assert(lastCrossColor[1] == .5f && lastCrossStart[2] == 57);
    cheats.current.enabled = 0;
    Cod2x_PoseDiagnostics(&entity, &info, goals, 0, 3);
    assert(lines == 6 && debugStrings == 48);
    assert(Cod2x_PoseNeutral());
    serverEntity.client = &serverClient;
    Cod2x_PoseDiagnostics(&entity, &info, goals, 0, 3);
    assert(lastCrossColor[1] == 1 && lastCrossStart[2] == 27);
    demo = 1;
    assert(Cod2x_PoseNeutral());
    running.current.enabled = 0;
    ((dvar_t *)debug)->current.integer = 2;
    Cod2x_PoseDiagnostics(&entity, &info, goals, 0, 3);
    assert(lines == 12 && debugStrings == 72);
    expectedServer = 1;
    Cod2x_PoseDiagnostics(&entity, &info, goals, 1, 3);
    assert(lines == 15 && debugStrings == 94);
    Cod2x_PoseMovement(1, 100, 65, 250, 0);
    assert(logs == 0);
    Cod2x_PoseMovement(0, 100, 65, 250, 0);
    Cod2x_PoseMovement(0, 351, 65, 250, 1);
    assert(logs == 2);
    puts("CoD2x dvar-protected pose offsets, controller neutral reset and complete diagnostics: pass");
    return 0;
}
