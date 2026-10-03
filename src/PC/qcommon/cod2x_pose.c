#include "cod2_feature_config.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#include "cod2x_pose.h"
#include "cod2x_features.h"
#include <float.h>
#include <stdio.h>
#include <string.h>

extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterVec3(const char *, float, float, float, float, float, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern const dvar_t *com_sv_running;
extern playerState_t *SV_GameClientNum(int);
extern gentity_t *SV_GentityNum(int);
extern void G_GetPlayerViewOrigin(const gentity_t *, vec_t *);
extern void CL_AddDebugLine(const vec_t *, const vec_t *, const vec_t *, qboolean, int, qboolean);
extern void CL_AddDebugString(const vec_t *, const vec_t *, float, const char *, qboolean);

static const dvar_t *debug, *eyePosition, *offsetEnable, *neutral, *offsets[8];

static int Cod2x_PoseCheats(void)
{
    const dvar_t *cheats = Dvar_FindVar("sv_cheats");
    return Cod2x_FeaturesDemo() || (cheats && cheats->current.enabled);
}

void Cod2x_PoseInit(void)
{
    /* Controls and controller order: CoD2x shared/animation.cpp:1423. */
    static const char *names[8] = {
        "player_offsetAngleBackLow", "player_offsetAngleBackMid", "player_offsetAngleBackUp",
        "player_offsetAngleNeck", "player_offsetAngleHead", "player_offsetAnglePelvis",
        "player_offsetAngleOrigin", "player_offsetPositionOrigin"
    };
    int i;
    debug = Dvar_RegisterInt("player_debug", 0, 0, 2, 0x1080);
    eyePosition = Dvar_RegisterBool("player_debugEyePosition", 0, 0x1080);
    offsetEnable = Dvar_RegisterBool("player_offsetEnable", 0, 0x1080);
    neutral = Dvar_RegisterBool("player_offsetNeutral", 0, 0x1080);
    for (i = 0; i < 8; ++i)
        offsets[i] = Dvar_RegisterVec3(names[i], 0, 0, 0, -FLT_MAX, FLT_MAX, 0x1080);
}

int Cod2x_PoseNeutral(void)
{
    return neutral && neutral->current.enabled && com_sv_running && com_sv_running->current.enabled && Cod2x_PoseCheats();
}

void Cod2x_PoseReset(entityState_t *entity)
{
    playerState_t *state;
    if (!Cod2x_PoseNeutral() || entity->eType != 1 || entity->clientNum < 0 || entity->clientNum >= 64)
        return;
    state = SV_GameClientNum(entity->clientNum);
    if (!state)
        return;
    state->torsoAnim = (state->torsoAnim & 0x200) ^ 0x200;
    state->legsAnim = (state->legsAnim & 0x200) ^ 0x200;
    entity->torsoAnim = (entity->torsoAnim & 0x200) ^ 0x200;
    entity->legsAnim = (entity->legsAnim & 0x200) ^ 0x200;
}

void Cod2x_PoseOffsets(vec3_t goals[8])
{
    int i, axis;
    if (!offsetEnable || !offsetEnable->current.enabled || !Cod2x_PoseCheats())
        return;
    for (i = 0; i < 8; ++i)
        for (axis = 0; axis < 3; ++axis)
            goals[i][axis] += offsets[i]->current.vector[axis];
}

void Cod2x_PoseDiagnostics(const entityState_t *entity, const clientInfo_t *info, int fromServer, int frametime)
{
    static const vec4_t color = {1, .5f, .5f, 1};
    vec3_t origin, start, end;
    char text[256];
    int axis;
    playerState_t *state = NULL;
    if (!debug || (!debug->current.integer && !eyePosition->current.enabled) || !Cod2x_PoseCheats())
        return;
    memcpy(origin, entity->pos.trBase, sizeof(origin));
    origin[2] += 60;
    if (com_sv_running && com_sv_running->current.enabled && entity->clientNum >= 0 && entity->clientNum < 64) {
        state = SV_GameClientNum(entity->clientNum);
        if (state) {
            const gentity_t *player = SV_GentityNum(entity->clientNum);
            if (player && player->client)
                G_GetPlayerViewOrigin(player, origin);
            else {
                memcpy(origin, state->origin, sizeof(origin));
                origin[2] += state->viewHeightCurrent;
            }
        }
    }
    for (axis = 0; axis < 3; ++axis) {
        memcpy(start, origin, sizeof(start));
        memcpy(end, origin, sizeof(end));
        start[axis] -= 3;
        end[axis] += 3;
        CL_AddDebugLine(start, end, color, 1, 0, fromServer);
    }
    if (debug->current.integer != fromServer + 1)
        return;
    snprintf(text, sizeof(text), "%s player %d flags:%x move:%x torso:%d legs:%d frame:%d",
             fromServer ? "Server" : "Client", entity->clientNum, entity->eFlags,
             info->clientConditions[3][0], entity->torsoAnim, entity->legsAnim, frametime);
    origin[2] += 5;
    CL_AddDebugString(origin, color, .25f, text, fromServer);
    snprintf(text, sizeof(text), "yaw torso:%.2f legs:%.2f move:%.2f lean:%.3f height:%.3f",
             info->torso.yawAngle, info->legs.yawAngle, info->lerpMoveDir, entity->leanf, entity->fTorsoHeight);
    origin[2] -= 5;
    CL_AddDebugString(origin, color, .25f, text, fromServer);
    if (state) {
        snprintf(text, sizeof(text), "torso %d timer:%d duration:%d legs %d timer:%d duration:%d",
                 state->torsoAnim, state->torsoTimer, state->torsoAnimDuration,
                 state->legsAnim, state->legsTimer, state->legsAnimDuration);
        origin[2] -= 5;
        CL_AddDebugString(origin, color, .25f, text, fromServer);
    }
}
#endif
