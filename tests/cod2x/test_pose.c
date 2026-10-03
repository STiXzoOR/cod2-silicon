#include "../../src/PC/qcommon/cod2x_pose.c"
#include <assert.h>

static dvar_t variables[16], cheats, running;
static vec3_t vectors[16];
const dvar_t *com_sv_running = &running;
static int count, lines, debugStrings, demo;
static playerState_t player;

const dvar_t *Dvar_FindVar(const char *name) { assert(!strcmp(name, "sv_cheats")); return &cheats; }
int Cod2x_FeaturesDemo(void) { return demo; }
playerState_t *SV_GameClientNum(int num) { assert(num == 0); return &player; }
gentity_t *SV_GentityNum(int num) { assert(num == 0); return NULL; }
void G_GetPlayerViewOrigin(const gentity_t *entity, vec_t *origin) { (void)entity; (void)origin; assert(0); }

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
    ++lines;
}
void CL_AddDebugString(const vec_t *origin, const vec_t *color, float scale, const char *text, qboolean server)
{
    (void)origin; (void)color;
    assert(scale == .25f && *text && server == 0);
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
    assert(goals[7][2] == 0);
    cheats.current.enabled = 1;
    Cod2x_PoseOffsets(goals);
    assert(goals[7][2] == 6 && goals[6][2] == 0);
    ((dvar_t *)neutral)->current.enabled = 1;
    assert(!Cod2x_PoseNeutral());
    running.current.enabled = 1;
    entity.eType = 2;
    entity.torsoAnim = player.torsoAnim = 42;
    entity.legsAnim = player.legsAnim = 0x211;
    Cod2x_PoseReset(&entity);
    assert(entity.torsoAnim == 42 && player.torsoAnim == 42 && entity.legsAnim == 0x211 && player.legsAnim == 0x211);
    entity.eType = 1;
    Cod2x_PoseReset(&entity);
    assert(entity.torsoAnim == 0x200 && player.torsoAnim == 0x200);
    assert(entity.legsAnim == 0 && player.legsAnim == 0);
    Cod2x_PoseReset(&entity);
    assert(entity.torsoAnim == 0 && entity.legsAnim == 0x200);
    ((dvar_t *)debug)->current.integer = 1;
    Cod2x_PoseDiagnostics(&entity, &info, 0, 3);
    assert(lines == 3 && debugStrings == 3);
    cheats.current.enabled = 0;
    Cod2x_PoseDiagnostics(&entity, &info, 0, 3);
    assert(lines == 3 && debugStrings == 3);
    assert(!Cod2x_PoseNeutral());
    demo = 1;
    assert(Cod2x_PoseNeutral());
    running.current.enabled = 0;
    ((dvar_t *)debug)->current.integer = 2;
    Cod2x_PoseDiagnostics(&entity, &info, 0, 3);
    assert(lines == 6 && debugStrings == 3);
    puts("CoD2x cheat-protected pose offsets, neutral reset and native diagnostics: pass");
    return 0;
}
